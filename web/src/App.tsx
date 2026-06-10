import { useState, useEffect } from 'react';
import { useAppStore } from './hooks/useAppStore';
import TopBar from './components/TopBar';
import ChatPanel from './components/ChatPanel';
import DevicePanel from './components/DevicePanel';
import SettingsDrawer from './components/SettingsDrawer';
import ErrorBoundary from './components/ErrorBoundary';

export default function App() {
  const settings = useAppStore((s) => s.settings);
  const connectMcp = useAppStore((s) => s.connectMcp);
  const clearChat = useAppStore((s) => s.clearChat);
  const mcpConnected = useAppStore((s) => s.mcpConnected);

  const [settingsOpen, setSettingsOpen] = useState(false);
  const [mobileDeviceOpen, setMobileDeviceOpen] = useState(false);

  // Auto-connect on first load if settings exist (silently catch errors)
  useEffect(() => {
    if (settings?.mcpServerUrl && !mcpConnected) {
      connectMcp().catch(() => {});
    }
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  return (
    <ErrorBoundary fallback={
      <div className="h-screen flex items-center justify-center bg-bg-primary text-text-primary">
        <div className="text-center space-y-4">
          <p className="text-lg">应用错误</p>
          <button onClick={() => window.location.reload()} className="px-4 py-2 bg-accent text-white rounded-lg">
            重新加载
          </button>
        </div>
      </div>
    }>
    <div className="h-screen flex flex-col bg-bg-primary text-text-primary">
      <TopBar
        onSettingsClick={() => setSettingsOpen(true)}
        onNewChat={clearChat}
      />

      {/* Main area */}
      <div className="flex-1 flex overflow-hidden">
        {/* Chat panel — always visible */}
        <ChatPanel />

        {/* Device panel — desktop sidebar */}
        <div className="hidden md:flex md:w-[30%]">
          <div className="w-full border-l border-border bg-bg-secondary flex flex-col">
            <DevicePanel />
          </div>
        </div>
      </div>

      {/* Mobile device panel — bottom drawer */}
      {mobileDeviceOpen && (
        <>
          <div className="md:hidden fixed inset-0 bg-black/50 z-30" onClick={() => setMobileDeviceOpen(false)} />
          <div className="md:hidden fixed bottom-0 left-0 right-0 z-40 max-h-[60vh] overflow-y-auto rounded-t-2xl bg-bg-secondary border-t border-border shadow-xl">
            <div className="flex justify-center py-2">
              <button onClick={() => setMobileDeviceOpen(false)} className="w-10 h-1 rounded-full bg-border" />
            </div>
            <DevicePanel />
          </div>
        </>
      )}

      {/* Mobile device toggle */}
      {!mobileDeviceOpen && (
        <button
          onClick={() => setMobileDeviceOpen(true)}
          className="md:hidden fixed bottom-4 right-4 z-20 p-3 rounded-full bg-accent text-white shadow-lg"
        >
          ▲
        </button>
      )}

      {/* Settings drawer */}
      <SettingsDrawer open={settingsOpen} onClose={() => setSettingsOpen(false)} />
    </div>
    </ErrorBoundary>
  );
}
