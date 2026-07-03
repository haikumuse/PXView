#ifndef PXVIEW_CORE_DATAFEEDPARSER_H
#define PXVIEW_CORE_DATAFEEDPARSER_H

#include <libsigrok.h>

namespace pv {

class SigSession;

namespace core {

class EventBus;

/**
 * DataFeedParser — owns the data feed callback trampoline and the feed_in_*
 * packet dispatch methods. Extracted from SigSession (SubTask 10.4) as a
 * mechanical refactoring: no behavior change.
 *
 * The parser holds an injected EventBus* (for trigger_message) and a
 * SigSession* (for accessing _capture_data / _view_data / _device_agent /
 * _is_triged / _data_lock / etc.). Declared as a friend of SigSession so it
 * can touch private members.
 */
class DataFeedParser {
public:
  DataFeedParser(EventBus *bus, SigSession *session);
  ~DataFeedParser();

  // Static trampoline registered with libsigrok. user_data is a DataFeedParser*.
  static void data_feed_callback_ex(const struct sr_dev_inst *sdi,
                                    const struct sr_datafeed_packet *packet,
                                    void *user_data);

  void data_feed_in(const struct sr_dev_inst *sdi,
                    const struct sr_datafeed_packet *packet);

private:
  void feed_in_header(const sr_dev_inst *sdi);
  void feed_in_meta(const sr_dev_inst *sdi, const sr_datafeed_meta &meta);
  void feed_in_trigger(const ds_trigger_pos &trigger_pos);
  void feed_in_logic(const sr_datafeed_logic &o);
  void feed_in_dso(const sr_datafeed_dso &o);
  void feed_in_analog(const sr_datafeed_analog &o);

  EventBus *_event_bus;
  // Circular reference: this manager needs many SigSession state fields and
  // methods (_capture_data / _view_data / _device_agent / _is_triged /
  // _trig_time / _trigger_flag / _trigger_ch / _hw_replied / _dso_status_valid /
  // _dso_status / _error / _data_mutex / _decode_task_manager /
  // _capture_manager / _spectrum_stacks / _math_stack / receive_header() /
  // receive_trigger() / frame_began() / frame_ended() / session_error() /
  // set_receive_data_len() / set_cur_snap_samplerate() / set_session_time() /
  // data_lock() / get_ch_num()) and cannot be easily decoupled without
  // further SigSession splitting. This is a known tech debt tracked by
  // modernize-core-layer-final Task 7.
  SigSession *_session;
};

} // namespace core
} // namespace pv

#endif // PXVIEW_CORE_DATAFEEDPARSER_H
