import { Activity, Usb, Plug, Unplug, Loader2 } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';

export default function DevicePanel() {
  const deviceInfo = useAppStore((s) => s.deviceInfo);
  const captureStatus = useAppStore((s) => s.captureStatus);
  const mcpConnected = useAppStore((s) => s.mcpConnected);
  const reconnectStatus = useAppStore((s) => s.reconnectStatus);
  const connectMcp = useAppStore((s) => s.connectMcp);
  const disconnectMcp = useAppStore((s) => s.disconnectMcp);
  const attemptReconnect = useAppStore((s) => s.attemptReconnect);

  const statusColors: Record<string, string> = {
    idle: 'bg-text-secondary',
    capturing: 'bg-success animate-pulse',
    completed: 'bg-accent',
  };

  const statusLabels: Record<string, string> = {
    idle: 'Idle',
    capturing: 'Capturing',
    completed: 'Completed',
  };

  const dot = statusColors[captureStatus] ?? 'bg-text-secondary';
  const label = statusLabels[captureStatus] ?? captureStatus;

  return (
    <div className="w-full flex flex-col">
      {/* Device info card */}
      <div className="p-4 border-b border-border">
        <h3 className="text-xs font-semibold text-text-secondary uppercase tracking-wider mb-3">Device</h3>
        {deviceInfo ? (
          <div className="bg-bg-card rounded-lg p-3 space-y-2">
            <div className="flex items-center justify-between">
              <span className="text-sm font-medium text-text-primary">{deviceInfo.name}</span>
              {deviceInfo.usbType && (
                <span
                  className={`text-[10px] px-1.5 py-0.5 rounded font-medium ${
                    deviceInfo.usbType.includes('3')
                      ? 'bg-accent/20 text-accent'
                      : 'bg-text-secondary/20 text-text-secondary'
                  }`}
                >
                  <Usb className="w-3 h-3 inline mr-0.5" />
                  {deviceInfo.usbType}
                </span>
              )}
            </div>
            {deviceInfo.mode && (
              <div className="text-xs text-text-secondary">Mode: {deviceInfo.mode}</div>
            )}
          </div>
        ) : (
          <div className="bg-bg-card rounded-lg p-4 text-center">
            <Unplug className="w-6 h-6 text-text-secondary mx-auto mb-2" />
            <p className="text-xs text-text-secondary">No device connected</p>
          </div>
        )}
      </div>

      {/* Capture status */}
      <div className="p-4 border-b border-border">
        <h3 className="text-xs font-semibold text-text-secondary uppercase tracking-wider mb-3">Capture</h3>
        <div className="flex items-center gap-2">
          <span className={`w-2 h-2 rounded-full ${dot}`} />
          <Activity className="w-4 h-4 text-text-secondary" />
          <span className="text-sm text-text-primary capitalize">{label}</span>
        </div>
      </div>

      {/* Connect / Disconnect / Reconnect */}
      <div className="p-4 mt-auto">
        {reconnectStatus === 'reconnecting' ? (
          <div className="w-full flex items-center justify-center gap-2 py-2 rounded-lg bg-warning/15 text-warning text-sm">
            <Loader2 className="w-4 h-4 animate-spin" />
            <span>Reconnecting…</span>
          </div>
        ) : reconnectStatus === 'failed' ? (
          <div className="p-2 space-y-2">
            <p className="text-xs text-error text-center">Connection lost</p>
            <button
              onClick={attemptReconnect}
              className="w-full flex items-center justify-center gap-2 py-2 rounded-lg bg-accent/15 text-accent text-sm hover:bg-accent/25 transition-colors"
            >
              <Plug className="w-4 h-4" /> Reconnect
            </button>
          </div>
        ) : mcpConnected ? (
          <button
            onClick={disconnectMcp}
            className="w-full flex items-center justify-center gap-2 py-2 rounded-lg bg-error/15 text-error text-sm hover:bg-error/25 transition-colors"
          >
            <Unplug className="w-4 h-4" /> Disconnect
          </button>
        ) : (
          <button
            onClick={connectMcp}
            className="w-full flex items-center justify-center gap-2 py-2 rounded-lg bg-accent/15 text-accent text-sm hover:bg-accent/25 transition-colors"
          >
            <Plug className="w-4 h-4" /> Connect
          </button>
        )}
      </div>
    </div>
  );
}
