import { Cpu, Settings, MessageSquarePlus } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';

export default function TopBar({
  onSettingsClick,
  onNewChat,
}: {
  onSettingsClick: () => void;
  onNewChat: () => void;
}) {
  const mcpConnected = useAppStore((s) => s.mcpConnected);

  return (
    <div className="h-12 bg-bg-secondary border-b border-border flex items-center justify-between px-4 shrink-0">
      {/* Left: Logo */}
      <div className="flex items-center gap-2">
        <Cpu className="w-5 h-5 text-accent" />
        <span className="text-accent font-bold text-lg tracking-tight">PXView MCP</span>
      </div>

      {/* Center: Connection status */}
      <div className="flex items-center gap-2">
        <span
          className={`w-2 h-2 rounded-full ${
            mcpConnected ? 'bg-success' : 'bg-error'
          }`}
        />
        <span
          className={`text-sm ${
            mcpConnected ? 'text-success' : 'text-error'
          }`}
        >
          {mcpConnected ? 'Connected' : 'Disconnected'}
        </span>
      </div>

      {/* Right: Actions */}
      <div className="flex items-center gap-2">
        <button
          onClick={onSettingsClick}
          className="p-2 rounded-lg hover:bg-bg-card transition-colors text-text-secondary hover:text-text-primary"
          title="Settings"
        >
          <Settings className="w-4 h-4" />
        </button>
        <button
          onClick={onNewChat}
          className="p-2 rounded-lg hover:bg-bg-card transition-colors text-text-secondary hover:text-text-primary"
          title="New Chat"
        >
          <MessageSquarePlus className="w-4 h-4" />
        </button>
      </div>
    </div>
  );
}
