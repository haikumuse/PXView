import { create } from 'zustand';
import { McpClient } from '../lib/mcp-client';
import type { McpTool } from '../lib/mcp-client';
import type { ProgressEvent } from '../lib/mcp-client';
import { LlmClient, mcpToOpenAITools } from '../lib/llm-client';
import type { ChatMessage, ToolCall } from '../lib/llm-client';
import { SYSTEM_PROMPT } from '../lib/system-prompt';

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
  // LLM fields
  tool_calls?: ToolCall[];
  tool_call_id?: string;
  // UI fields
  toolCallStatuses?: ToolCallStatus[];
  isStreaming?: boolean;
  isToolRunning?: boolean;
  isStopped?: boolean;
}

const MAX_CONTEXT_MESSAGES = 30;
const TRUNCATION_THRESHOLD = 50;
const MAX_TOOL_RESULT_LENGTH = 4000;

/** Derive ChatMessage[] for LLM from ConversationMessage[] */
function messagesToChatHistory(messages: ConversationMessage[]): ChatMessage[] {
  let relevant = messages.filter(m => m.role !== 'system');

  // Truncate if too many messages
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
}

interface AppState {
  // Connection
  mcpConnected: boolean;
  mcpTools: McpTool[];
  settings: AppSettings;

  // Chat
  messages: ConversationMessage[];
  isProcessing: boolean;

  // Device
  deviceInfo: { name: string; usbType: string; mode: string } | null;
  captureStatus: 'idle' | 'capturing' | 'completed';
  captureProgress: ProgressEvent | null;
  reconnectStatus: 'idle' | 'reconnecting' | 'failed';

  // Actions
  updateSettings: (settings: Partial<AppSettings>) => void;
  connectMcp: () => Promise<void>;
  disconnectMcp: () => void;
  sendMessage: (content: string) => Promise<void>;
  stopGeneration: () => void;
  clearChat: () => void;
  setCaptureStatus: (status: 'idle' | 'capturing' | 'completed') => void;
  setCaptureProgress: (progress: ProgressEvent | null) => void;
  attemptReconnect: () => Promise<void>;
}

const CHAT_STORAGE_KEY = 'pxview-mcp-chat';
const MAX_PERSIST_MESSAGES = 100;

function loadChatMessages(): ConversationMessage[] {
  try {
    const saved = localStorage.getItem(CHAT_STORAGE_KEY);
    if (saved) {
      const parsed = JSON.parse(saved);
      if (Array.isArray(parsed)) return parsed.slice(-MAX_PERSIST_MESSAGES);
    }
  } catch {}
  return [];
}

function saveChatMessages(messages: ConversationMessage[]) {
  try {
    const toSave = messages
      .filter(m => !m.isStreaming && !m.isToolRunning) // don't save in-progress messages
      .slice(-MAX_PERSIST_MESSAGES);
    localStorage.setItem(CHAT_STORAGE_KEY, JSON.stringify(toSave));
  } catch {}
}

let saveTimer: ReturnType<typeof setTimeout> | null = null;
function debouncedSaveMessages(messages: ConversationMessage[]) {
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
let msgCounter = 0;
let abortController: AbortController | null = null;

/** Update a specific message in the store by id, returning new array */
function updateMessage(messages: ConversationMessage[], id: string, patch: Partial<ConversationMessage>): ConversationMessage[] {
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
  reconnectStatus: 'idle',

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
    // Stop any ongoing requests first
    if (get().isProcessing) {
      get().stopGeneration();
    }
    if (mcpClient) {
      mcpClient.disconnect();
      mcpClient = null;
    }
    set({ mcpConnected: false, mcpTools: [], deviceInfo: null, captureStatus: 'idle', captureProgress: null, reconnectStatus: 'idle' });
  },

  sendMessage: async (content: string) => {
    const { settings, mcpConnected, mcpTools, isProcessing } = get();
    if (isProcessing) return;
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
    const userMsg: ConversationMessage = { id: userMsgId, role: 'user', content };
    const afterUserMsg = [...get().messages, userMsg];
    set({ messages: afterUserMsg, isProcessing: true });
    debouncedSaveMessages(afterUserMsg);

    const openaiTools = mcpToOpenAITools(mcpTools);

    try {
      let continueLoop = true;
      while (continueLoop) {
        // Check if aborted
        if (signal.aborted) break;

        // Derive chat history from current messages
        const chatHistory = messagesToChatHistory(get().messages);

        // Create a placeholder assistant message — visible immediately
        const assistantMsgId = `msg-${++msgCounter}`;
        const assistantMsg: ConversationMessage = {
          id: assistantMsgId,
          role: 'assistant',
          content: '',
          toolCallStatuses: [],
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
                  toolCallStatuses: [...streamToolCalls],
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
                  tool_calls: msg.tool_calls || undefined,
                  toolCallStatuses: streamToolCalls.length > 0 ? [...streamToolCalls] : undefined,
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

        // Execute each tool call — update status in real-time
        const toolFailCount = new Map<string, number>();
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
              toolCallStatuses: [...streamToolCalls],
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
            let resultText = result.content?.map(c => c.text).join('\n') || '';
            if (resultText.length > MAX_TOOL_RESULT_LENGTH) {
              resultText = resultText.slice(0, MAX_TOOL_RESULT_LENGTH) +
                `\n[结果已截断，共 ${resultText.length} 字符]`;
            }
            const elapsed = Date.now() - tcStatus.startTime;

            tcStatus.status = 'success';
            tcStatus.result = resultText;
            tcStatus.elapsed = elapsed;

            if (tc.function.name === 'wait_capture') {
              set({ captureStatus: 'completed', captureProgress: null });
            }

            // Add tool result as ConversationMessage
            const toolResultMsg: ConversationMessage = {
              id: `msg-${++msgCounter}`,
              role: 'tool',
              content: resultText,
              tool_call_id: tc.id,
            };
            set(s => ({ messages: [...s.messages, toolResultMsg] }));
          } catch (err: any) {
            const elapsed = Date.now() - tcStatus.startTime;

            // If aborted by user, mark as cancelled and stop tool loop
            if (err.name === 'AbortError') {
              tcStatus.status = 'cancelled';
              tcStatus.result = 'Cancelled by user';
              tcStatus.elapsed = elapsed;

              if (tc.function.name === 'wait_capture') {
                set({ captureStatus: 'idle', captureProgress: null });
              }

              // Update tool call status and break out of tool loop
              set(s => ({
                messages: updateMessage(s.messages, assistantMsgId, {
                  toolCallStatuses: [...streamToolCalls],
                }),
              }));
              break;
            }

            tcStatus.status = 'error';
            const failCount = (toolFailCount.get(tc.function.name) || 0) + 1;
            toolFailCount.set(tc.function.name, failCount);

            let errorText = err.message || String(err);
            if (errorText.length > MAX_TOOL_RESULT_LENGTH) {
              errorText = errorText.slice(0, MAX_TOOL_RESULT_LENGTH) +
                `\n[错误信息已截断，共 ${errorText.length} 字符]`;
            }

            let errorContent = `Error: ${errorText}`;
            if (failCount >= 3) {
              errorContent += '\n[此工具已连续失败 3 次，请尝试其他方法或检查设备状态]';
            }

            tcStatus.result = errorContent;
            tcStatus.elapsed = elapsed;

            if (tc.function.name === 'wait_capture') {
              set({ captureStatus: 'idle', captureProgress: null });
            }

            // Add tool error result as ConversationMessage
            const toolResultMsg: ConversationMessage = {
              id: `msg-${++msgCounter}`,
              role: 'tool',
              content: errorContent,
              tool_call_id: tc.id,
            };
            set(s => ({ messages: [...s.messages, toolResultMsg] }));
          }

          // Update tool call status in the message
          set(s => ({
            messages: updateMessage(s.messages, assistantMsgId, {
              toolCallStatuses: [...streamToolCalls],
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

      // Check if it's a network error and we were connected — trigger auto-reconnect
      if (err.message?.includes('Failed to fetch') || err.message?.includes('NetworkError')) {
        get().attemptReconnect();
      }

      // Add error as assistant message
      const errorMsg: ConversationMessage = {
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
    // Mark in-progress messages as stopped
    set(s => {
      const messages = s.messages.map(m => {
        if (!m.isStreaming && !m.isToolRunning) return m;
        const updated: ConversationMessage = {
          ...m,
          isStreaming: false,
          isToolRunning: false,
          isStopped: true,
          content: m.content,
        };
        // Cancel in-progress tool calls
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
      return { messages, isProcessing: false };
    });
    debouncedSaveMessages(get().messages);
  },

  clearChat: () => {
    localStorage.removeItem(CHAT_STORAGE_KEY);
    set({ messages: [] });
  },

  setCaptureStatus: (status) => set({ captureStatus: status }),
  setCaptureProgress: (progress) => set({ captureProgress: progress }),

  attemptReconnect: async () => {
    const { settings, reconnectStatus, mcpConnected } = get();
    if (reconnectStatus === 'reconnecting') return; // Already reconnecting
    if (mcpConnected) return; // Still connected, no need

    set({ reconnectStatus: 'reconnecting', mcpConnected: false });

    const maxAttempts = 6;
    const intervalMs = 5000;

    for (let attempt = 1; attempt <= maxAttempts; attempt++) {
      try {
        await new Promise(resolve => setTimeout(resolve, intervalMs));

        // Try to create a new MCP client and connect
        const newClient = new McpClient(settings.mcpServerUrl);
        await newClient.connect();

        mcpClient = newClient;

        // Refresh tools and device info
        const tools = await newClient.listTools();

        // Try to get device info (same logic as connectMcp)
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
        return; // Success
      } catch {
        // Wait before next attempt (already waited at top of loop)
      }
    }

    // All attempts failed
    set({ reconnectStatus: 'failed' });
  },
}));
