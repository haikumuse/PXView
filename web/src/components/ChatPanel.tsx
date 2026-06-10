import { useEffect, useRef } from 'react';
import { Cpu } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';
import ChatMessage from './ChatMessage';
import ChatInput from './ChatInput';
import ErrorBoundary from './ErrorBoundary';

export default function ChatPanel() {
  const messages = useAppStore((s) => s.messages);
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  return (
    <div className="flex flex-col h-full w-full md:w-[70%]">
      {/* Messages area */}
      <div className="flex-1 overflow-y-auto px-4 py-3">
        {messages.length === 0 ? (
          <div className="flex flex-col items-center justify-center h-full text-center gap-3">
            <Cpu className="w-12 h-12 text-accent/50" />
            <p className="text-text-secondary text-sm">
              Connect to PXView and start analyzing signals
            </p>
          </div>
        ) : (
          messages.map((msg) => (
            <ErrorBoundary key={msg.id}>
              <ChatMessage message={msg} />
            </ErrorBoundary>
          ))
        )}
        <div ref={bottomRef} />
      </div>

      {/* Input area */}
      <ChatInput />
    </div>
  );
}
