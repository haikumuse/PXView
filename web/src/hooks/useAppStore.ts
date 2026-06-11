import { create } from 'zustand';
import { McpClient } from '../lib/mcp-client';
import type { McpTool } from '../lib/mcp-client';
import type { ProgressEvent } from '../lib/mcp-client';
import { LlmClient, mcpToOpenAITools } from '../lib/llm-client';
import type { ChatMessage, ToolCall } from '../lib/llm-client';
import { SYSTEM_PROMPT as DEFAULT_SYSTEM_PROMPT } from '../lib/system-prompt';
import i18n from '../i18n';

export interface ToolCallStatus {
  id: string;
  name: string;
  args: Record<string, unknown>;
  status: 'pending' | 'running' | 'success' | 'error' | 'cancelled';
  result?: string;
  elapsed?: number;
  startTime: number;
}

export interface ConversationMessage {
  id: string;
  role: 'user' | 'assistant' | 'system' | 'tool';
  content: string;
  tool_calls?: ToolCall[];
  tool_call_id?: string;
  toolCallStatuses?: ToolCallStatus[];
  isStreaming?: boolean;
  isToolRunning?: boolean;
  isStopped?: boolean;
}

export interface ChatSession {
  id: string;
  title: string;
  updatedAt: number;
  messages: ConversationMessage[];
}

const MAX_CONTEXT_MESSAGES = 30;
const TRUNCATION_THRESHOLD = 50;
const MAX_TOOL_RESULT_LENGTH = 4000;

function messagesToChatHistory(messages: ConversationMessage[]): ChatMessage[] {
  let relevant = messages.filter(m => m.role !== 'system');
  if (relevant.length > TRUNCATION_THRESHOLD) {
    relevant = relevant.slice(-MAX_CONTEXT_MESSAGES);
  }
  return relevant.map(m => {
    const base: ChatMessage = { role: m.role, content: m.content };
    if (m.tool_calls) base.tool_calls = m.tool_calls;
    if (m.tool_call_id) base.tool_call_id = m.tool_call_id;
    return base;
  });
}

interface AppSettings {
  mcpServerUrl: string;
  llmBaseUrl: string;
  llmApiKey: string;
  llmModel: string;
  systemPrompt: string;
  language: 'en' | 'zh';
}

interface AppState {
  mcpConnected: boolean;
  mcpTools: McpTool[];
  settings: AppSettings;

  sessions: Record<string, ChatSession>;
  currentSessionId: string | null;
  messages: ConversationMessage[];
  isProcessing: boolean;

  deviceInfo: { name: string; usbType: string; mode: string } | null;
  captureStatus: 'idle' | 'capturing' | 'completed';
  captureProgress: ProgressEvent | null;
  reconnectStatus: 'idle' | 'reconnecting' | 'failed';

  updateSettings: (settings: Partial<AppSettings>) => void;
  connectMcp: () => Promise<void>;
  disconnectMcp: () => void;
  sendMessage: (content: string) => Promise<void>;
  stopGeneration: () => void;
  clearChat: () => void;
  setCaptureStatus: (status: 'idle' | 'capturing' | 'completed') => void;
  setCaptureProgress: (progress: ProgressEvent | null) => void;
  attemptReconnect: () => Promise<void>;

  createNewSession: () => void;
  switchSession: (id: string) => void;
  deleteSession: (id: string) => void;
  deleteMessageAndFollowing: (msgId: string) => void;
  regenerateMessage: (msgId: string) => Promise<void>;
}

const CHAT_STORAGE_KEY = 'pxview-mcp-sessions';
const CURRENT_SESSION_KEY = 'pxview-mcp-current-session';

function generateId() {
  return Date.now().toString(36) + Math.random().toString(36).substring(2, 9);
}

function loadSessions(): { sessions: Record<string, ChatSession>, current: string | null } {
  try {
    const saved = localStorage.getItem(CHAT_STORAGE_KEY);
    if (saved) {
      const sessions = JSON.parse(saved);
      const current = localStorage.getItem(CURRENT_SESSION_KEY);
      if (Object.keys(sessions).length > 0) {
        return { sessions, current: current && sessions[current] ? current : Object.keys(sessions)[0] };
      }
    }
  } catch {}
  return { sessions: {}, current: null };
}

function saveSessions(sessions: Record<string, ChatSession>, currentId: string | null) {
  try {
    const toSave: Record<string, ChatSession> = {};
    for (const [id, s] of Object.entries(sessions)) {
      toSave[id] = {
        ...s,
        messages: s.messages.filter(m => !m.isStreaming && !m.isToolRunning)
      };
    }
    localStorage.setItem(CHAT_STORAGE_KEY, JSON.stringify(toSave));
    if (currentId) localStorage.setItem(CURRENT_SESSION_KEY, currentId);
  } catch {}
}

let saveTimer: ReturnType<typeof setTimeout> | null = null;
function debouncedSaveSessions(sessions: Record<string, ChatSession>, currentId: string | null) {
  if (saveTimer) clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    saveSessions(sessions, currentId);
    saveTimer = null;
  }, 500);
}

function loadSettings(): AppSettings {
  try {
    const saved = localStorage.getItem('pxview-mcp-settings');
    if (saved) {
        const parsed = JSON.parse(saved);
        if (!parsed.systemPrompt) parsed.systemPrompt = DEFAULT_SYSTEM_PROMPT;
        return parsed;
    }
  } catch {}
  return {
    mcpServerUrl: 'http://127.0.0.1:10530',
    llmBaseUrl: 'https://api.openai.com/v1',
    llmApiKey: '',
    llmModel: 'gpt-4o',
    systemPrompt: DEFAULT_SYSTEM_PROMPT,
    language: 'zh',
  };
}

function saveSettings(settings: AppSettings) {
  localStorage.setItem('pxview-mcp-settings', JSON.stringify(settings));
}

let mcpClient: McpClient | null = null;
let llmClient: LlmClient | null = null;
let msgCounter = 0;
let abortController: AbortController | null = null;

function updateMessage(messages: ConversationMessage[], id: string, patch: Partial<ConversationMessage>): ConversationMessage[] {
  return messages.map(m => m.id === id ? { ...m, ...patch } : m);
}

export const useAppStore = create<AppState>((set, get) => {
  const initialSessions = loadSessions();
  let sessions = initialSessions.sessions;
  let currentSessionId = initialSessions.current;

  if (!currentSessionId) {
    currentSessionId = generateId();
    sessions = {
      [currentSessionId]: { id: currentSessionId, title: 'New Diagnostic', updatedAt: Date.now(), messages: [] }
    };
    saveSessions(sessions, currentSessionId);
  }

  return {
    mcpConnected: false,
    mcpTools: [],
    settings: loadSettings(),
    sessions,
    currentSessionId,
    messages: sessions[currentSessionId].messages,
    isProcessing: false,
    deviceInfo: null,
    captureStatus: 'idle',
    captureProgress: null,
    reconnectStatus: 'idle',

    updateSettings: (partial) => {
      const settings = { ...get().settings, ...partial };
      saveSettings(settings);
      set({ settings });
      if (llmClient) {
        llmClient.updateConfig(settings.llmBaseUrl, settings.llmApiKey, settings.llmModel);
      }
      if (partial.language && partial.language !== i18n.language) {
        i18n.changeLanguage(partial.language);
      }
    },

    connectMcp: async () => {
      const { settings } = get();
      try {
        mcpClient = new McpClient(settings.mcpServerUrl);
        await mcpClient.connect();
        const tools = await mcpClient.listTools();
        set({ mcpConnected: true, mcpTools: tools });

        try {
          const result = await mcpClient.callTool('get_devices', {});
          const text = result.content?.[0]?.text;
          if (text) {
            const devices = JSON.parse(text);
            if (devices.length > 0) {
              set({ deviceInfo: {
                name: devices[0].name || devices[0].modelName || 'Unknown',
                usbType: devices[0].usbType || 'Unknown',
                mode: devices[0].mode || 'Logic'
              }});
            }
          }
        } catch {}
      } catch (err) {
        mcpClient = null;
        throw err;
      }
    },

    disconnectMcp: () => {
      if (get().isProcessing) {
        get().stopGeneration();
      }
      if (mcpClient) {
        mcpClient.disconnect();
        mcpClient = null;
      }
      set({ mcpConnected: false, mcpTools: [], deviceInfo: null, captureStatus: 'idle', captureProgress: null, reconnectStatus: 'idle' });
    },

    createNewSession: () => {
      if (get().isProcessing) return;
      const id = generateId();
      const newSession: ChatSession = { id, title: 'New Diagnostic', updatedAt: Date.now(), messages: [] };
      const newSessions = { ...get().sessions, [id]: newSession };
      set({ sessions: newSessions, currentSessionId: id, messages: [] });
      debouncedSaveSessions(newSessions, id);
    },

    switchSession: (id: string) => {
      if (get().isProcessing) return;
      const s = get().sessions[id];
      if (s) {
        set({ currentSessionId: id, messages: s.messages });
        localStorage.setItem(CURRENT_SESSION_KEY, id);
      }
    },

    deleteSession: (id: string) => {
      if (get().isProcessing) return;
      const newSessions = { ...get().sessions };
      delete newSessions[id];
      
      let nextCurrent = get().currentSessionId;
      let nextMessages = get().messages;
      
      if (nextCurrent === id) {
        const keys = Object.keys(newSessions).sort((a,b) => newSessions[b].updatedAt - newSessions[a].updatedAt);
        if (keys.length > 0) {
          nextCurrent = keys[0];
          nextMessages = newSessions[nextCurrent].messages;
        } else {
          nextCurrent = generateId();
          newSessions[nextCurrent] = { id: nextCurrent, title: 'New Diagnostic', updatedAt: Date.now(), messages: [] };
          nextMessages = [];
        }
      }
      set({ sessions: newSessions, currentSessionId: nextCurrent, messages: nextMessages });
      debouncedSaveSessions(newSessions, nextCurrent);
    },

    deleteMessageAndFollowing: (msgId: string) => {
      if (get().isProcessing) return;
      const { messages, currentSessionId, sessions } = get();
      if (!currentSessionId) return;
      
      const idx = messages.findIndex(m => m.id === msgId);
      if (idx === -1) return;
      
      const newMessages = messages.slice(0, idx);
      const newSessions = {
        ...sessions,
        [currentSessionId]: { ...sessions[currentSessionId], messages: newMessages, updatedAt: Date.now() }
      };
      set({ messages: newMessages, sessions: newSessions });
      debouncedSaveSessions(newSessions, currentSessionId);
    },

    regenerateMessage: async (msgId: string) => {
      if (get().isProcessing) return;
      const { messages } = get();
      const idx = messages.findIndex(m => m.id === msgId);
      if (idx === -1) return;
      
      let userIdx = idx;
      while (userIdx >= 0 && messages[userIdx].role !== 'user') {
        userIdx--;
      }
      if (userIdx === -1) return;
      
      const userContent = messages[userIdx].content;
      get().deleteMessageAndFollowing(messages[userIdx].id);
      await get().sendMessage(userContent);
    },

    sendMessage: async (content: string) => {
      const { settings, mcpConnected, mcpTools, isProcessing, currentSessionId, sessions } = get();
      if (isProcessing || !currentSessionId) return;
      if (!mcpClient || !mcpConnected) {
        throw new Error('MCP server not connected');
      }

      if (!llmClient) {
        llmClient = new LlmClient(settings.llmBaseUrl, settings.llmApiKey, settings.llmModel);
      }

      abortController = new AbortController();
      const signal = abortController.signal;

      const userMsgId = `msg-${++msgCounter}`;
      const userMsg: ConversationMessage = { id: userMsgId, role: 'user', content };
      const afterUserMsg = [...get().messages, userMsg];
      
      let title = sessions[currentSessionId].title;
      if (afterUserMsg.filter(m => m.role === 'user').length === 1) {
        title = content.slice(0, 30).trim();
        if (title.length === 0) title = 'New Diagnostic';
      }

      const updateState = (newMessages: ConversationMessage[]) => {
        const newSessions = {
          ...get().sessions,
          [currentSessionId]: { ...get().sessions[currentSessionId], messages: newMessages, title, updatedAt: Date.now() }
        };
        set({ messages: newMessages, sessions: newSessions });
        debouncedSaveSessions(newSessions, currentSessionId);
      };

      set({ isProcessing: true });
      updateState(afterUserMsg);

      const openaiTools = mcpToOpenAITools(mcpTools);

      try {
        let continueLoop = true;
        while (continueLoop) {
          if (signal.aborted) break;

          const chatHistory = messagesToChatHistory(get().messages);

          const assistantMsgId = `msg-${++msgCounter}`;
          const assistantMsg: ConversationMessage = {
            id: assistantMsgId,
            role: 'assistant',
            content: '',
            toolCallStatuses: [],
            isStreaming: true,
          };
          updateState([...get().messages, assistantMsg]);

          let streamContent = '';
          const streamToolCalls: ToolCallStatus[] = [];

          const finalMessage = await llmClient.chatStream(
            [{ role: 'system', content: settings.systemPrompt }, ...chatHistory],
            openaiTools,
            {
              onText: (delta) => {
                streamContent += delta;
                updateState(updateMessage(get().messages, assistantMsgId, {
                  content: streamContent,
                }));
              },

              onToolCallStart: (tc) => {
                const status: ToolCallStatus = {
                  id: tc.id,
                  name: tc.function.name,
                  args: {},
                  status: 'pending',
                  startTime: Date.now(),
                };
                streamToolCalls.push(status);
                updateState(updateMessage(get().messages, assistantMsgId, {
                  toolCallStatuses: [...streamToolCalls],
                }));
              },

              onToolCallArgs: () => {},

              onDone: (msg) => {
                if (msg.tool_calls) {
                  for (let i = 0; i < msg.tool_calls.length; i++) {
                    if (streamToolCalls[i]) {
                      try {
                        streamToolCalls[i].args = JSON.parse(msg.tool_calls[i].function.arguments || '{}');
                      } catch {
                        streamToolCalls[i].args = {};
                      }
                    }
                  }
                }
                updateState(updateMessage(get().messages, assistantMsgId, {
                  content: streamContent || msg.content || '',
                  tool_calls: msg.tool_calls || undefined,
                  toolCallStatuses: streamToolCalls.length > 0 ? [...streamToolCalls] : undefined,
                  isStreaming: false,
                  isToolRunning: msg.tool_calls ? msg.tool_calls.length > 0 : false,
                }));
              },

              onError: (err) => {
                updateState(updateMessage(get().messages, assistantMsgId, {
                  content: `Error: ${err.message}`,
                  isStreaming: false,
                  isToolRunning: false,
                }));
              },
            },
            signal,
          );

          if (!finalMessage.tool_calls || finalMessage.tool_calls.length === 0) {
            updateState(updateMessage(get().messages, assistantMsgId, {
              isStreaming: false,
              isToolRunning: false,
            }));
            set({ isProcessing: false });
            return;
          }

          const toolFailCount = new Map<string, number>();
          for (let i = 0; i < finalMessage.tool_calls.length; i++) {
            if (signal.aborted) break;
            const tc = finalMessage.tool_calls[i];
            const tcStatus = streamToolCalls[i];
            if (!tcStatus) continue;

            tcStatus.status = 'running';
            tcStatus.startTime = Date.now();
            updateState(updateMessage(get().messages, assistantMsgId, {
              toolCallStatuses: [...streamToolCalls],
            }));

            try {
              const onProgress = (tc.function.name === 'wait_capture')
                ? (event: ProgressEvent) => {
                    set({ captureProgress: event });
                    if (event.progress !== undefined || event.message) {
                      set({ captureStatus: 'capturing' });
                    }
                  }
                : undefined;

              const result = await mcpClient.callTool(tc.function.name, tcStatus.args, onProgress, signal);
              let resultText = result.content?.map(c => c.text).join('\n') || '';
              if (resultText.length > MAX_TOOL_RESULT_LENGTH) {
                resultText = resultText.slice(0, MAX_TOOL_RESULT_LENGTH) +
                  `\n[Result truncated, total length ${resultText.length}]`;
              }
              const elapsed = Date.now() - tcStatus.startTime;

              tcStatus.status = 'success';
              tcStatus.result = resultText;
              tcStatus.elapsed = elapsed;

              if (tc.function.name === 'wait_capture') {
                set({ captureStatus: 'completed', captureProgress: null });
              }

              const toolResultMsg: ConversationMessage = {
                id: `msg-${++msgCounter}`,
                role: 'tool',
                content: resultText,
                tool_call_id: tc.id,
              };
              updateState([...get().messages, toolResultMsg]);
            } catch (err: any) {
              const elapsed = Date.now() - tcStatus.startTime;

              if (err.name === 'AbortError') {
                tcStatus.status = 'cancelled';
                tcStatus.result = 'Cancelled by user';
                tcStatus.elapsed = elapsed;

                if (tc.function.name === 'wait_capture') {
                  set({ captureStatus: 'idle', captureProgress: null });
                }

                updateState(updateMessage(get().messages, assistantMsgId, {
                  toolCallStatuses: [...streamToolCalls],
                }));
                break;
              }

              tcStatus.status = 'error';
              const failCount = (toolFailCount.get(tc.function.name) || 0) + 1;
              toolFailCount.set(tc.function.name, failCount);

              let errorText = err.message || String(err);
              if (errorText.length > MAX_TOOL_RESULT_LENGTH) {
                errorText = errorText.slice(0, MAX_TOOL_RESULT_LENGTH) +
                  `\n[Error truncated, total length ${errorText.length}]`;
              }

              let errorContent = `Error: ${errorText}`;
              if (failCount >= 3) {
                errorContent += '\n[Tool failed 3 consecutive times, please review]';
              }

              tcStatus.result = errorContent;
              tcStatus.elapsed = elapsed;

              if (tc.function.name === 'wait_capture') {
                set({ captureStatus: 'idle', captureProgress: null });
              }

              const toolResultMsg: ConversationMessage = {
                id: `msg-${++msgCounter}`,
                role: 'tool',
                content: errorContent,
                tool_call_id: tc.id,
              };
              updateState([...get().messages, toolResultMsg]);
            }

            updateState(updateMessage(get().messages, assistantMsgId, {
              toolCallStatuses: [...streamToolCalls],
            }));
          }

          updateState(updateMessage(get().messages, assistantMsgId, {
            isToolRunning: false,
          }));
        }
      } catch (err: any) {
        if (signal.aborted || err.name === 'AbortError') {
          set({ isProcessing: false });
          return;
        }

        if (err.message?.includes('Failed to fetch') || err.message?.includes('NetworkError')) {
          get().attemptReconnect();
        }

        const errorMsg: ConversationMessage = {
          id: `msg-${++msgCounter}`,
          role: 'assistant',
          content: `Error: ${err.message || String(err)}`,
          isStreaming: false,
          isToolRunning: false,
        };
        updateState([...get().messages, errorMsg]);
        set({ isProcessing: false });
      } finally {
        abortController = null;
      }
    },

    stopGeneration: () => {
      if (abortController) {
        abortController.abort();
        abortController = null;
      }
      
      const { messages, currentSessionId, sessions } = get();
      if (!currentSessionId) return;

      const newMessages = messages.map(m => {
        if (!m.isStreaming && !m.isToolRunning) return m;
        const updated: ConversationMessage = {
          ...m,
          isStreaming: false,
          isToolRunning: false,
          isStopped: true,
          content: m.content,
        };
        if (updated.toolCallStatuses) {
          updated.toolCallStatuses = updated.toolCallStatuses.map(tc => {
            if (tc.status === 'pending' || tc.status === 'running') {
              return { ...tc, status: 'cancelled' as const, result: 'Cancelled by user' };
            }
            return tc;
          });
        }
        return updated;
      });
      
      const newSessions = {
        ...sessions,
        [currentSessionId]: { ...sessions[currentSessionId], messages: newMessages, updatedAt: Date.now() }
      };
      
      set({ messages: newMessages, sessions: newSessions, isProcessing: false });
      debouncedSaveSessions(newSessions, currentSessionId);
    },

    clearChat: () => {
      const current = get().currentSessionId;
      if (current) {
        get().deleteSession(current);
        get().createNewSession();
      }
    },

    setCaptureStatus: (status) => set({ captureStatus: status }),
    setCaptureProgress: (progress) => set({ captureProgress: progress }),

    attemptReconnect: async () => {
      // (unchanged)
      const { settings, reconnectStatus, mcpConnected } = get();
      if (reconnectStatus === 'reconnecting') return;
      if (mcpConnected) return;

      set({ reconnectStatus: 'reconnecting', mcpConnected: false });

      const maxAttempts = 6;
      const intervalMs = 5000;

      for (let attempt = 1; attempt <= maxAttempts; attempt++) {
        try {
          await new Promise(resolve => setTimeout(resolve, intervalMs));

          const newClient = new McpClient(settings.mcpServerUrl);
          await newClient.connect();

          mcpClient = newClient;

          const tools = await newClient.listTools();

          let deviceInfo = null;
          try {
            const result = await newClient.callTool('get_devices', {});
            const text = result.content?.[0]?.text;
            if (text) {
              const devices = JSON.parse(text);
              if (devices.length > 0) {
                deviceInfo = {
                  name: devices[0].name || devices[0].modelName || 'Unknown',
                  usbType: devices[0].usbType || 'Unknown',
                  mode: devices[0].mode || 'Logic',
                };
              }
            }
          } catch {}

          set({
            mcpConnected: true,
            reconnectStatus: 'idle',
            mcpTools: tools,
            deviceInfo,
          });
          return;
        } catch {}
      }

      set({ reconnectStatus: 'failed' });
    },
  };
});
