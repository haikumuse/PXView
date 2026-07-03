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

class SigSession;

namespace core {

class EventBus;

/**
 * FilterProcessor — owns the glitch filter and signal invert background
 * threads and their running flags. Extracted from SigSession (SubTask 10.5)
 * as a mechanical refactoring: no behavior change, just code movement.
 *
 * The processor holds an injected EventBus* (for trigger_message) and a
 * SigSession* (for accessing _view_data / _device_agent / data_updated()).
 * It is declared as a friend of SigSession so it can touch private members.
 */
class FilterProcessor {
public:
  FilterProcessor(EventBus *bus, SigSession *session);
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
  // Circular reference: this manager needs SigSession state and methods
  // (_view_data / _device_agent / data_updated()). Note _view_data is a
  // direct SigSession private member, not accessible via CaptureManager, so
  // the spec-hypothesized EventBus* + CaptureManager* injection is not
  // feasible. This is a known tech debt tracked by modernize-core-layer-final
  // Task 7.
  SigSession *_session;

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
