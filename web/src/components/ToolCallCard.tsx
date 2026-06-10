import { useState, useEffect } from 'react';
import { Loader2, CheckCircle2, XCircle, Ban, ChevronDown, ChevronRight } from 'lucide-react';
import type { ToolCallStatus } from '../hooks/useAppStore';
import DecoderResultTable from './DecoderResultTable';

function getFriendlyLabel(name: string, args: Record<string, unknown>): string | null {
  if (name === 'get_devices') return 'Fetching devices…';
  if (name === 'start_capture') return 'Starting capture…';
  if (name === 'add_analyzer') {
    const decoder = args.decoder ?? args.protocol ?? '';
    const ch = args.channel ?? args.ch ?? '';
    return `Adding ${decoder} on channel ${ch}`;
  }
  return null;
}

const borderColors: Record<ToolCallStatus['status'], string> = {
  pending: 'border-l-blue-400',
  running: 'border-l-yellow-500',
  success: 'border-l-success',
  error: 'border-l-error',
  cancelled: 'border-l-gray-400',
};

const StatusIcon = ({ status }: { status: ToolCallStatus['status'] }) => {
  if (status === 'pending') return <Loader2 className="w-4 h-4 text-blue-400 animate-pulse" />;
  if (status === 'running') return <Loader2 className="w-4 h-4 text-yellow-500 animate-spin" />;
  if (status === 'success') return <CheckCircle2 className="w-4 h-4 text-success" />;
  if (status === 'cancelled') return <Ban className="w-4 h-4 text-gray-400" />;
  return <XCircle className="w-4 h-4 text-error" />;
};

function DeviceCards({ data }: { data: string }) {
  try {
    const devices = JSON.parse(data);
    if (!Array.isArray(devices)) return <pre className="text-xs text-text-primary bg-bg-card rounded p-2 overflow-x-auto whitespace-pre-wrap">{data}</pre>;
    return (
      <div className="space-y-1">
        {devices.map((d: any, i: number) => (
          <div key={i} className="flex items-center gap-2 px-2 py-1 bg-bg-card rounded text-xs">
            <span className="text-text-primary font-medium">{d.name || d.modelName || 'Unknown'}</span>
            {d.usbType && <span className="text-[10px] px-1.5 py-0.5 rounded bg-accent/20 text-accent">{d.usbType}</span>}
            {d.mode && <span className="text-text-secondary">{d.mode}</span>}
          </div>
        ))}
      </div>
    );
  } catch {
    return <pre className="text-xs text-text-primary bg-bg-card rounded p-2 overflow-x-auto whitespace-pre-wrap">{data}</pre>;
  }
}

export default function ToolCallCard({ toolCall }: { toolCall: ToolCallStatus }) {
  const [expanded, setExpanded] = useState(false);
  const [showFull, setShowFull] = useState(false);
  const [liveElapsed, setLiveElapsed] = useState<number | null>(null);

  // Live elapsed timer when running
  useEffect(() => {
    if (toolCall.status !== 'running') return;

    const update = () => {
      setLiveElapsed(Math.round((Date.now() - toolCall.startTime) / 1000));
    };
    update(); // initial tick
    const timer = setInterval(update, 1000);

    return () => clearInterval(timer);
  }, [toolCall.status, toolCall.startTime]);

  const args = (() => {
    try {
      return typeof toolCall.args === 'string' ? JSON.parse(toolCall.args) : toolCall.args ?? {};
    } catch {
      return {};
    }
  })();

  const friendly = getFriendlyLabel(toolCall.name, args);
  const resultText = toolCall.result ?? '';
  const truncated = !showFull && resultText.length > 500;
  const displayResult = truncated ? resultText.slice(0, 500) : resultText;

  return (
    <div className={`border-l-2 ${borderColors[toolCall.status]} bg-bg-primary rounded-r-lg my-1`}>
      <button
        onClick={() => setExpanded(!expanded)}
        className="w-full flex items-center gap-2 px-3 py-2 text-left hover:bg-bg-card/50 transition-colors"
      >
        {expanded ? <ChevronDown className="w-3 h-3 text-text-secondary" /> : <ChevronRight className="w-3 h-3 text-text-secondary" />}
        <span className="font-semibold text-sm text-text-primary">{toolCall.name}</span>
        <StatusIcon status={toolCall.status} />
        {friendly && <span className="text-xs text-text-secondary ml-1">— {friendly}</span>}
        {toolCall.status === 'pending' && (
          <span className="ml-auto text-xs text-blue-400">waiting…</span>
        )}
        {toolCall.status === 'running' && liveElapsed != null && (
          <span className="ml-auto text-xs text-yellow-500">{liveElapsed}s</span>
        )}
        {toolCall.status === 'cancelled' && (
          <span className="ml-auto text-xs text-gray-400">Cancelled</span>
        )}
        {toolCall.status !== 'running' && toolCall.status !== 'pending' && toolCall.status !== 'cancelled' && toolCall.elapsed != null && (
          <span className="ml-auto text-xs text-text-secondary">{toolCall.elapsed}ms</span>
        )}
      </button>

      {expanded && (
        <div className="px-3 pb-2 space-y-2">
          {Object.keys(args).length > 0 && (
            <pre className="text-xs text-text-secondary bg-bg-card rounded p-2 overflow-x-auto">
              {JSON.stringify(args, null, 2)}
            </pre>
          )}
          {resultText && (
            <div>
              {toolCall.name === 'get_analyzer_results' ? (
                <DecoderResultTable data={resultText} />
              ) : toolCall.name === 'get_devices' ? (
                <DeviceCards data={resultText} />
              ) : (
                <>
                  <pre className="text-xs text-text-primary bg-bg-card rounded p-2 overflow-x-auto whitespace-pre-wrap">
                    {displayResult}
                  </pre>
                  {truncated && (
                    <button onClick={() => setShowFull(true)} className="text-xs text-accent hover:underline mt-1">
                      Show more
                    </button>
                  )}
                </>
              )}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
