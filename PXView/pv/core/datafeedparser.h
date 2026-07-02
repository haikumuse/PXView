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
  SigSession *_session;
};

} // namespace core
} // namespace pv

#endif // PXVIEW_CORE_DATAFEEDPARSER_H
