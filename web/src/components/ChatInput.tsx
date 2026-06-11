import { useState, useRef, useEffect, type KeyboardEvent } from 'react';
import { useAppStore } from '../hooks/useAppStore';
import { useTranslation } from 'react-i18next';

export default function ChatInput() {
  const [text, setText] = useState('');
  const textareaRef = useRef<HTMLTextAreaElement>(null);
  const sendMessage = useAppStore((s) => s.sendMessage);
  const stopGeneration = useAppStore((s) => s.stopGeneration);
  const isProcessing = useAppStore((s) => s.isProcessing);
  const { t } = useTranslation();

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
    <div className="flex items-end gap-2 p-3 bg-bg-screen-light border-t-2 border-border shadow-[inset_0_2px_4px_rgba(0,0,0,0.5)]">
      <div className="text-text-screen font-bold pt-2 select-none">&gt;</div>
      <textarea
        ref={textareaRef}
        value={text}
        onChange={(e) => setText(e.target.value)}
        onKeyDown={handleKeyDown}
        placeholder={t('INPUT_PLACEHOLDER')}
        rows={1}
        className="flex-1 resize-none bg-transparent outline-none border-none px-2 py-2 text-lg text-text-screen placeholder:text-text-screen-alt placeholder:opacity-40 font-mono"
        spellCheck="false"
      />
      {isProcessing ? (
        <button
          onClick={stopGeneration}
          className="px-4 py-2 border-2 border-border bg-error text-bg-casing font-bold uppercase tracking-widest hover:bg-opacity-80 active:translate-y-[2px]"
        >
          {t('HALT')}
        </button>
      ) : (
        <button
          onClick={handleSend}
          disabled={!text.trim() || isProcessing}
          className="px-4 py-2 border-2 border-border bg-text-screen text-bg-screen font-bold uppercase tracking-widest hover:bg-text-screen-alt disabled:opacity-30 disabled:hover:bg-text-screen active:translate-y-[2px]"
        >
          {t('EXECUTE')}
        </button>
      )}
    </div>
  );
}
