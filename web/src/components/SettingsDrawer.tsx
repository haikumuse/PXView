import { useState, useEffect } from 'react';
import { X } from 'lucide-react';
import { useAppStore } from '../hooks/useAppStore';

interface Settings {
  mcpServerUrl: string;
  llmBaseUrl: string;
  llmApiKey: string;
  llmModel: string;
}

export default function SettingsDrawer({ open, onClose }: { open: boolean; onClose: () => void }) {
  const settings = useAppStore((s) => s.settings);
  const updateSettings = useAppStore((s) => s.updateSettings);

  const [form, setForm] = useState<Settings>({
    mcpServerUrl: '',
    llmBaseUrl: '',
    llmApiKey: '',
    llmModel: '',
  });

  useEffect(() => {
    if (open) {
      setForm({
        mcpServerUrl: settings?.mcpServerUrl ?? '',
        llmBaseUrl: settings?.llmBaseUrl ?? '',
        llmApiKey: settings?.llmApiKey ?? '',
        llmModel: settings?.llmModel ?? '',
      });
    }
  }, [open, settings]);

  const handleSave = () => {
    updateSettings(form);
    onClose();
  };

  if (!open) return null;

  const field = (label: string, key: keyof Settings, type: string = 'text') => (
    <div>
      <label className="block text-xs text-text-secondary mb-1">{label}</label>
      <input
        type={type}
        value={form[key]}
        onChange={(e) => setForm({ ...form, [key]: e.target.value })}
        className="w-full rounded-lg bg-bg-card border border-border px-3 py-2 text-sm text-text-primary placeholder:text-text-secondary focus:outline-none focus:border-accent transition-colors"
      />
    </div>
  );

  return (
    <>
      {/* Backdrop */}
      <div className="fixed inset-0 bg-black/50 z-40" onClick={onClose} />

      {/* Drawer */}
      <div className="fixed top-0 right-0 h-full w-[360px] max-w-full bg-bg-secondary border-l border-border z-50 flex flex-col shadow-xl">
        {/* Header */}
        <div className="flex items-center justify-between px-4 py-3 border-b border-border">
          <h2 className="text-sm font-semibold text-text-primary">Settings</h2>
          <button onClick={onClose} className="p-1 rounded hover:bg-bg-card text-text-secondary hover:text-text-primary transition-colors">
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Fields */}
        <div className="flex-1 overflow-y-auto p-4 space-y-4">
          {field('MCP Server URL', 'mcpServerUrl')}
          {field('LLM Base URL', 'llmBaseUrl')}
          {field('LLM API Key', 'llmApiKey', 'password')}
          {field('LLM Model', 'llmModel')}
        </div>

        {/* Save */}
        <div className="p-4 border-t border-border">
          <button
            onClick={handleSave}
            className="w-full py-2 rounded-lg bg-accent text-white text-sm font-medium hover:bg-accent/80 transition-colors"
          >
            Save
          </button>
        </div>
      </div>
    </>
  );
}
