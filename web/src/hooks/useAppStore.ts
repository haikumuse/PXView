import { create } from 'zustand';
import { McpClient } from '../lib/mcp-client';
import type { McpTool } from '../lib/mcp-client';
import type { ProgressEvent } from '../lib/mcp-client';
import { LlmClient, mcpToOpenAITools } from '../lib/llm-client';
import type { ChatMessage } from '../lib/llm-client';
import { SYSTEM_PROMPT } from '../lib/system-prompt';

export interface ToolCallStatus {
  id: string;
  name: string;
  args: Record<string, unknown>;
  status: 'pending' | 'running' | 'success' | 'error';
  result?: string;
  elapsed?: number;
  startTime: number;
}

export interface DisplayMessage {
  id: string;
  role: 'user' | 'assistant';
  content: string;
  toolCalls?: ToolCallStatus[];
  isStreaming?: boolean;       // still receiving LLM text
  isToolRunning?: boolean;     // currently executing tools
}

interface AppSettings {
  mcpServerUrl: string;
  llmBaseUrl: string;
  llmApiKey: string;
  llmModel: string;
}

interface AppState {
  // Connection
  mcpConnected: boolean;
  mcpTools: McpTool[];
  settings: AppSettings;

  // Chat
  messages: DisplayMessage[];
  isProcessing: boolean;

  // Device
  deviceInfo: { name: string; usbType: string; mode: string } | null;
  captureStatus: 'idle' | 'capturing' | 'completed';
  captureProgress: ProgressEvent | null;

  // Actions
  updateSettings: (settings: Partial<AppSettings>) => void;
  connectMcp: () => Promise<void>;
  disconnectMcp: () => void;
  sendMessage: (content: string) => Promise<void>;
  stopGeneration: () => void;
  clearChat: () => void;
  setCaptureStatus: (status: 'idle' | 'capturing' | 'completed') => void;
  setCaptureProgress: (progress: ProgressEvent | null) => void;
}

const CHAT_STORAGE_KEY = 'pxview-mcp-chat';
const MAX_PERSIST_MESSAGES = 100;

function loadChatMessages(): DisplayMessage[] {
  try {
    const saved = localStorage.getItem(CHAT_STORAGE_KEY);
    if (saved) {
      const parsed = JSON.parse(saved);
      if (Array.isArray(parsed)) return parsed.slice(-MAX_PERSIST_MESSAGES);
    }
  } catch {}
  return [];
}

function saveChatMessages(messages: DisplayMessage[]) {
  try {
    const toSave = messages
      .filter(m => !m.isStreaming && !m.isToolRunning) // don't save in-progress messages
      .slice(-MAX_PERSIST_MESSAGES);
    localStorage.setItem(CHAT_STORAGE_KEY, JSON.stringify(toSave));
  } catch {}
}

let saveTimer: ReturnType<typeof setTimeout> | null = null;
function debouncedSaveMessages(messages: DisplayMessage[]) {
  if (saveTimer) clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    saveChatMessages(messages);
    saveTimer = null;
  }, 500);
}

function loadSettings(): AppSettings {
  try {
    const saved = localStorage.getItem('pxview-mcp-settings');
    if (saved) return JSON.parse(saved);
  } catch {}
  return {
    mcpServerUrl: 'http://127.0.0.1:10530',
    llmBaseUrl: 'https://api.openai.com/v1',
    llmApiKey: '',
    llmModel: 'gpt-4o',
  };
}

function saveSettings(settings: AppSettings) {
  localStorage.setItem('pxview-mcp-settings', JSON.stringify(settings));
}

let mcpClient: McpClient | null = null;
let llmClient: LlmClient | null = null;
let chatHistory: ChatMessage[] = [];
let msgCounter = 0;
let abortController: AbortController | null = null;

/** Update a specific message in the store by id, returning new array */
function updateMessage(messages: DisplayMessage[], id: string, patch: Partial<DisplayMessage>): DisplayMessage[] {
  return messages.map(m => m.id === id ? { ...m, ...patch } : m);
}

export const useAppStore = create<AppState>((set, get) => ({
  mcpConnected: false,
  mcpTools: [],
  settings: loadSettings(),
  messages: loadChatMessages(),
  isProcessing: false,
  deviceInfo: null,
  captureStatus: 'idle',
  captureProgress: null,

  updateSettings: (partial) => {
    const settings = { ...get().settings, ...partial };
    saveSettings(settings);
    set({ settings });
    if (llmClient) {
      llmClient.updateConfig(settings.llmBaseUrl, settings.llmApiKey, settings.llmModel);
    }
  },

  connectMcp: async () => {
    const { settings } = get();
    try {
      mcpClient = new McpClient(settings.mcpServerUrl);
      await mcpClient.connect();
      const tools = await mcpClient.listTools();
      set({ mcpConnected: true, mcpTools: tools });

      // Try to get device info
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
    if (mcpClient) {
      mcpClient.disconnect();
      mcpClient = null;
    }
    set({ mcpConnected: false, mcpTools: [], deviceInfo: null });
  },

  sendMessage: async (content: string) => {
    const { settings, mcpConnected, mcpTools } = get();
    if (!mcpClient || !mcpConnected) {
      throw new Error('MCP server not connected');
    }

    // Initialize LLM client if needed
    if (!llmClient) {
      llmClient = new LlmClient(settings.llmBaseUrl, settings.llmApiKey, settings.llmModel);
    }

    // Create abort controller for this request
    abortController = new AbortController();
    const signal = abortController.signal;

    // Add user message
    const userMsgId = `msg-${++msgCounter}`;
    const userMsg: DisplayMessage = { id: userMsgId, role: 'user', content };
    const afterUserMsg = [...get().messages, userMsg];
    set({ messages: afterUserMsg, isProcessing: true });
    debouncedSaveMessages(afterUserMsg);
    chatHistory.push({ role: 'user', content });

    const openaiTools = mcpToOpenAITools(mcpTools);

    try {
      let continueLoop = true;
      while (continueLoop) {
        // Check if aborted
        if (signal.aborted) break;
        // Create a placeholder assistant message — visible immediately
        const assistantMsgId = `msg-${++msgCounter}`;
        const assistantMsg: DisplayMessage = {
          id: assistantMsgId,
          role: 'assistant',
          content: '',
          toolCalls: [],
          isStreaming: true,
        };
        set(s => ({ messages: [...s.messages, assistantMsg] }));

        // Accumulate text & tool calls from streaming
        let streamContent = '';
        const streamToolCalls: ToolCallStatus[] = [];

        // Call LLM with streaming
        const finalMessage = await llmClient.chatStream(
          [{ role: 'system', content: SYSTEM_PROMPT }, ...chatHistory],
          openaiTools,
          {
            onText: (delta) => {
              streamContent += delta;
              set(s => ({
                messages: updateMessage(s.messages, assistantMsgId, {
                  content: streamContent,
                }),
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
              set(s => ({
                messages: updateMessage(s.messages, assistantMsgId, {
                  toolCalls: [...streamToolCalls],
                }),
              }));
            },

            onToolCallArgs: (_id, _delta) => {
              // We'll parse args after streaming completes
            },

            onDone: (msg) => {
              // Parse tool call args now that streaming is complete
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
              // Mark streaming done
              set(s => ({
                messages: updateMessage(s.messages, assistantMsgId, {
                  content: streamContent || msg.content || '',
                  toolCalls: streamToolCalls.length > 0 ? [...streamToolCalls] : undefined,
                  isStreaming: false,
                  isToolRunning: msg.tool_calls ? msg.tool_calls.length > 0 : false,
                }),
              }));
            },

            onError: (err) => {
              set(s => ({
                messages: updateMessage(s.messages, assistantMsgId, {
                  content: `Error: ${err.message}`,
                  isStreaming: false,
                  isToolRunning: false,
                }),
              }));
            },
          },
          signal,
        );

        // If no tool calls, we're done
        if (!finalMessage.tool_calls || finalMessage.tool_calls.length === 0) {
          chatHistory.push({ role: 'assistant', content: finalMessage.content || '' });
          set(s => {
            const final = updateMessage(s.messages, assistantMsgId, {
              isStreaming: false,
              isToolRunning: false,
            });
            return { messages: final, isProcessing: false };
          });
          debouncedSaveMessages(get().messages);
          return;
        }

        // Add assistant message with tool_calls to history
        chatHistory.push({
          role: 'assistant',
          content: finalMessage.content,
          tool_calls: finalMessage.tool_calls,
        });

        // Execute each tool call — update status in real-time
        for (let i = 0; i < finalMessage.tool_calls.length; i++) {
          if (signal.aborted) break;
          const tc = finalMessage.tool_calls[i];
          const tcStatus = streamToolCalls[i];
          if (!tcStatus) continue;

          // Mark as running
          tcStatus.status = 'running';
          tcStatus.startTime = Date.now();
          set(s => ({
            messages: updateMessage(s.messages, assistantMsgId, {
              toolCalls: [...streamToolCalls],
            }),
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
            const resultText = result.content?.map(c => c.text).join('\n') || '';
            const elapsed = Date.now() - tcStatus.startTime;

            tcStatus.status = 'success';
            tcStatus.result = resultText;
            tcStatus.elapsed = elapsed;

            if (tc.function.name === 'wait_capture') {
              set({ captureStatus: 'completed', captureProgress: null });
            }

            chatHistory.push({
              role: 'tool',
              tool_call_id: tc.id,
              content: resultText,
            });
          } catch (err: any) {
            const elapsed = Date.now() - tcStatus.startTime;
            tcStatus.status = 'error';
            tcStatus.result = err.message || String(err);
            tcStatus.elapsed = elapsed;

            if (tc.function.name === 'wait_capture') {
              set({ captureStatus: 'idle', captureProgress: null });
            }

            chatHistory.push({
              role: 'tool',
              tool_call_id: tc.id,
              content: `Error: ${err.message || String(err)}`,
            });
          }

          // Update tool call status in the message
          set(s => ({
            messages: updateMessage(s.messages, assistantMsgId, {
              toolCalls: [...streamToolCalls],
            }),
          }));
        }

        // All tools done for this round — mark not running
        set(s => ({
          messages: updateMessage(s.messages, assistantMsgId, {
            isToolRunning: false,
          }),
        }));

        // Loop back to LLM with tool results
      }
    } catch (err: any) {
      // If aborted by user, don't show error
      if (signal.aborted || err.name === 'AbortError') {
        set({ isProcessing: false });
        debouncedSaveMessages(get().messages);
        return;
      }
      // Add error as assistant message
      const errorMsg: DisplayMessage = {
        id: `msg-${++msgCounter}`,
        role: 'assistant',
        content: `Error: ${err.message || String(err)}`,
        isStreaming: false,
        isToolRunning: false,
      };
      set(s => ({ messages: [...s.messages, errorMsg], isProcessing: false }));
      debouncedSaveMessages(get().messages);
    } finally {
      abortController = null;
    }
  },

  stopGeneration: () => {
    if (abortController) {
      abortController.abort();
      abortController = null;
    }
    set({ isProcessing: false });
  },

  clearChat: () => {
    chatHistory = [];
    localStorage.removeItem(CHAT_STORAGE_KEY);
    set({ messages: [] });
  },

  setCaptureStatus: (status) => set({ captureStatus: status }),
  setCaptureProgress: (progress) => set({ captureProgress: progress }),
}));
