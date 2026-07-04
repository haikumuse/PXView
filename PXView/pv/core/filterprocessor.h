#ifndef PXVIEW_CORE_FILTERPROCESSOR_H
#define PXVIEW_CORE_FILTERPROCESSOR_H

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <cstdint>

// GlitchFilterMode is a global unscoped enum defined here; cannot be
// forward-declared without a fixed underlying type.
#include "../data/logicsnapshot.h"

namespace pv {

namespace core {

class EventBus;
class SessionStateContext;

/**
 * FilterProcessor — owns the glitch filter and signal invert background
 * threads and their running flags. Extracted from SigSession (SubTask 10.5)
 * as a mechanical refactoring: no behavior change, just code movement.
 *
 * The processor holds an injected EventBus* (for typed event dispatch via
 * broadcast_async<T>/broadcast_sync<T>) and a SessionStateContext* (for
 * accessing view_data / device_agent / data_updated()).
 * modernize-core-layer-radical phase 1 replaced the previous SigSession* +
 * friend-declaration coupling.
 */
class FilterProcessor {
public:
  FilterProcessor(EventBus *bus, SessionStateContext *state);
  ~FilterProcessor();

  void set_glitch_filter(const std::vector<uint32_t> &thresholds,
                         const std::vector<GlitchFilterMode> &filter_modes);
  void clear_glitch_filter();
  bool is_glitch_filter_active();

  void set_signal_invert(const std::vector<bool> &channels);
  void clear_signal_invert();
  bool is_signal_invert_active();

  /// Stop both background threads. Called from SigSession::Close().
  void stop();

private:
  void glitch_filter_task(const std::vector<uint32_t> thresholds,
                          const std::vector<GlitchFilterMode> filter_modes);
  void signal_invert_task(const std::vector<bool> channels);

  EventBus *_event_bus;
  // Shared session state (view_data / device_agent / data_updated())
  // accessed via SessionStateContext accessors. modernize-core-layer-radical
  // phase 1 replaced the previous SigSession* + friend-declaration coupling.
  SessionStateContext *_state;

  // modernize-core-layer-final Task 5: RAII-managed background threads.
  // unique_ptr replaces raw std::thread* + manual new/delete. The destructor
  // path joins (if joinable) and resets the pointer automatically — no
  // `delete` calls remain in the .cpp.
  std::unique_ptr<std::thread> _glitch_filter_thread;
  std::atomic<bool> _glitch_filter_running;
  std::unique_ptr<std::thread> _signal_invert_thread;
  std::atomic<bool> _signal_invert_running;
};

} // namespace core
} // namespace pv

#endif // PXVIEW_CORE_FILTERPROCESSOR_H
