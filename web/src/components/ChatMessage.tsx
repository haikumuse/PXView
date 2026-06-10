import type { ConversationMessage } from '../hooks/useAppStore';
import ToolCallCard from './ToolCallCard';
import { Loader2, Brain } from 'lucide-react';

export default function ChatMessage({ message }: { message: ConversationMessage }) {
  const isUser = message.role === 'user';
  const isStreaming = message.isStreaming;
  const isToolRunning = message.isToolRunning;
  const isStopped = message.isStopped;
  const hasToolCalls = message.toolCallStatuses && message.toolCallStatuses.length > 0;
  const showThinking = isStreaming && !message.content && (!hasToolCalls || message.toolCallStatuses!.every(tc => tc.status === 'pending'));

  return (
    <div className={`flex ${isUser ? 'justify-end' : 'justify-start'} mb-3`}>
      <div
        className={`max-w-[80%] rounded-xl px-4 py-2.5 ${
          isUser
            ? 'bg-accent/15 text-text-primary'
            : 'bg-bg-card text-text-primary'
        }`}
      >
        {/* Thinking indicator */}
        {showThinking && (
          <div className="flex items-center gap-2 text-text-secondary text-sm py-1">
            <Brain className="w-4 h-4 text-accent animate-pulse" />
            <span>Thinking…</span>
          </div>
        )}

        {/* Text content */}
        {message.content && (
          <p className="text-sm whitespace-pre-wrap leading-relaxed">
            {message.content}
            {isStreaming && message.content && (
              <span className="inline-block w-0.5 h-4 bg-accent animate-pulse ml-0.5 align-text-bottom" />
            )}
          </p>
        )}

        {/* Stopped indicator */}
        {isStopped && (
          <span className="text-xs text-text-secondary ml-1">[已停止]</span>
        )}

        {/* Tool calls */}
        {hasToolCalls && (
          <div className="mt-2 space-y-1">
            {message.toolCallStatuses!.map((tc) => (
              <ToolCallCard key={tc.id} toolCall={tc} />
            ))}
          </div>
        )}

        {/* Tool running indicator (when no more text but tools still executing) */}
        {isToolRunning && hasToolCalls && message.toolCallStatuses!.some(tc => tc.status === 'running') && (
          <div className="flex items-center gap-2 text-text-secondary text-xs mt-2 pt-2 border-t border-border/50">
            <Loader2 className="w-3 h-3 animate-spin" />
            <span>Executing tools…</span>
          </div>
        )}
      </div>
    </div>
  );
}
