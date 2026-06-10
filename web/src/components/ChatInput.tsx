import { useState, useRef, useEffect, type KeyboardEvent } from 'react';
import { SendHorizontal, Square } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';

export default function ChatInput() {
  const [text, setText] = useState('');
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const sendMessage = useAppStore((s) => s.sendMessage);
  const stopGeneration = useAppStore((s) => s.stopGeneration);
  const isProcessing = useAppStore((s) => s.isProcessing);

  useEffect(() => {
    const el = textareaRef.current;
    if (!el) return;
    el.style.height = 'auto';
    el.style.height = `${Math.min(el.scrollHeight, 4 * 24)}px`;
  }, [text]);

  const handleSend = () => {
    const trimmed = text.trim();
    if (!trimmed || isProcessing) return;
    sendMessage(trimmed);
    setText('');
    if (textareaRef.current) textareaRef.current.style.height = 'auto';
  };

  const handleKeyDown = (e: KeyboardEvent<HTMLTextAreaElement>) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <div className="flex items-end gap-2 p-3 border-t border-border bg-bg-secondary">
      <textarea
        ref={textareaRef}
        value={text}
        onChange={(e) => setText(e.target.value)}
        onKeyDown={handleKeyDown}
        placeholder="Type a message…"
        rows={1}
        className="flex-1 resize-none rounded-xl bg-bg-card border border-border px-3 py-2 text-sm text-text-primary placeholder:text-text-secondary focus:outline-none focus:border-accent transition-colors"
      />
      {isProcessing ? (
        <button
          onClick={stopGeneration}
          className="p-2 rounded-xl bg-error/20 text-error hover:bg-error/30 transition-colors shrink-0"
          title="Stop"
        >
          <Square className="w-4 h-4" />
        </button>
      ) : (
        <button
          onClick={handleSend}
          disabled={!text.trim() || isProcessing}
          className="p-2 rounded-xl bg-accent/20 text-accent hover:bg-accent/30 disabled:opacity-40 disabled:cursor-not-allowed transition-colors shrink-0"
          title="Send"
        >
          <SendHorizontal className="w-4 h-4" />
        </button>
      )}
    </div>
  );
}
