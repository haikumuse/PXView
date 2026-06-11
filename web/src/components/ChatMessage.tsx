import { useState } from 'react';
import type { ConversationMessage } from '../hooks/useAppStore';
import ToolCallCard from './ToolCallCard';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';

export default function ChatMessage({ message }: { message: ConversationMessage }) {
  const isUser = message.role === 'user';
  const isAssistant = message.role === 'assistant';
  const isStreaming = message.isStreaming;
  const isToolRunning = message.isToolRunning;
  const isStopped = message.isStopped;
  const hasToolCalls = message.toolCallStatuses && message.toolCallStatuses.length > 0;
  const showThinking = isStreaming && !message.content && (!hasToolCalls || message.toolCallStatuses!.every(tc => tc.status === 'pending'));

  const regenerateMessage = useAppStore(s => s.regenerateMessage);
  const isProcessing = useAppStore(s => s.isProcessing);
  const { t } = useTranslation();

  const [copied, setCopied] = useState(false);

  const handleCopy = () => {
    navigator.clipboard.writeText(message.content);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleRegenerate = () => {
    if (!isProcessing) {
      regenerateMessage(message.id);
    }
  };

  const prefix = isUser ? 'USER>' : message.role === 'tool' ? 'OUT >' : 'SYS >';
  const color = isUser ? 'text-text-screen-alt' : message.role === 'tool' ? 'text-text-casing-muted' : 'text-text-screen';

  return (
    <div className={`mb-6 group ${color}`}>
      <div className="flex items-start gap-2 relative">
        <span className="font-bold shrink-0 mt-0.5">{prefix}</span>
        
        <div className="flex-1 min-w-0">
          {/* Action buttons (Copy, Regenerate) shown on hover */}
          <div className="absolute right-0 -top-2 opacity-0 group-hover:opacity-100 transition-opacity flex gap-2">
            {message.content && (
              <button 
                onClick={handleCopy} 
                className="text-xs bg-bg-screen-light border border-border px-1 py-0.5 text-text-screen hover:bg-border hover:text-bg-screen transition-colors"
              >
                {copied ? t('COPIED') : t('COPY')}
              </button>
            )}
            {isAssistant && !isProcessing && (
              <button 
                onClick={handleRegenerate}
                className="text-xs bg-bg-screen-light border border-border px-1 py-0.5 text-warning hover:bg-warning hover:text-bg-screen transition-colors"
                title="Regenerate this response and discard everything after it"
              >
                {t('REGENERATE')}
              </button>
            )}
          </div>

          {/* Thinking indicator */}
          {showThinking && (
            <div className="animate-pulse">{t('PROCESSING')}</div>
          )}

          {/* Text content with Markdown support */}
          {message.content && (
            <div className="markdown-body leading-relaxed max-w-full overflow-hidden">
              <ReactMarkdown remarkPlugins={[remarkGfm]}>
                {message.content}
              </ReactMarkdown>
              {isStreaming && (
                <span className="inline-block w-2 h-4 bg-text-screen animate-pulse ml-1 align-text-bottom" />
              )}
            </div>
          )}

          {/* Stopped indicator */}
          {isStopped && (
            <div className="text-error uppercase mt-2">{t('HALTED_USER')}</div>
          )}

          {/* Tool calls */}
          {hasToolCalls && (
            <div className="mt-4 space-y-2">
              {message.toolCallStatuses!.map((tc) => (
                <ToolCallCard key={tc.id} toolCall={tc} />
              ))}
            </div>
          )}

          {/* Tool running indicator */}
          {isToolRunning && hasToolCalls && message.toolCallStatuses!.some(tc => tc.status === 'running') && (
            <div className="mt-2 animate-pulse text-warning">
              {t('EXECUTING_SUBROUTINE')}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
