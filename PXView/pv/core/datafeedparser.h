#ifndef PXVIEW_CORE_DATAFEEDPARSER_H
#define PXVIEW_CORE_DATAFEEDPARSER_H

#include <libsigrok/libsigrok.h>

namespace pv {

namespace core {

class EventBus;
class SessionStateContext;
class ISessionCoordination;

/**
 * DataFeedParser — owns the data feed callback trampoline and the feed_in_*
 * packet dispatch methods. Extracted from SigSession (SubTask 10.4) as a
 * mechanical refactoring: no behavior change.
 *
 * The parser holds an injected EventBus* (for typed event dispatch via
 * broadcast_async<T>/broadcast_sync<T>) and a SessionStateContext* (for
 * accessing capture_data / view_data / device_agent / is_triged / data_lock
 * / etc.). modernize-core-layer-radical phase 1 replaced the previous
 * SigSession* + friend-declaration coupling.
 */
class DataFeedParser {
public:
  DataFeedParser(EventBus *bus, SessionStateContext *state, ISessionCoordination *coord);
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
  void feed_in_trigger();
  void feed_in_logic(const sr_datafeed_logic &o);
  void feed_in_analog(const sr_datafeed_analog &o);
  void feed_in_dso(const sr_datafeed_dso &o);

  EventBus *_event_bus;
  SessionStateContext *_state;
  ISessionCoordination *_coord;
};

} // namespace core
} // namespace pv

#endif // PXVIEW_CORE_DATAFEEDPARSER_H
