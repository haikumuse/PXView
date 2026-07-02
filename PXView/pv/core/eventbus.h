#ifndef PXVIEW_CORE_EVENTBUS_H
#define PXVIEW_CORE_EVENTBUS_H

#include <QObject>
#include <QCoreApplication>
#include <vector>
#include <functional>

#include "../interface/icallbacks.h"
#include "../interface/events.h"
#include "../log.h"

namespace pv {
namespace core {

/**
 * EventBus — central dispatch hub for Core→View/Service notifications.
 *
 * All broadcast_msg and trigger_message calls are ASYNCHRONOUS: they queue
 * the actual dispatch onto the qApp (QCoreApplication singleton) event loop
 * via Qt::QueuedConnection. This eliminates re-entrant broadcast UAF where
 * a synchronous broadcast triggers nested reload() → View AllReplaced →
 * deleting `this` mid-method.
 *
 * SigSession is NOT a QObject, so we queue on qApp which always has a
 * running event loop in both GUI and headless modes.
 *
 * broadcast<T>() (typed events) stays SYNCHRONOUS: it is called from within
 * the async-dispatched OnMessage handler, so it already runs on the main
 * thread after the caller's stack frame has unwound. A thread_local
 * _broadcast_depth guard prevents re-entrant typed event dispatch.
 */
class EventBus {
public:
    EventBus();
    ~EventBus();

    // ---- Listener registration ----
    void add_callback(ISessionCallbackBase *cb);
    void remove_callback(ISessionCallbackBase *cb);
    void add_msg_listener(IMessageListener *l);
    void add_event_listener(interface::IEventListener *l);
    void remove_event_listener(interface::IEventListener *l);

    // ---- Listener queries ----
    bool has_callbacks() const { return !_callbacks.empty(); }

    // ---- Async broadcast (queues to qApp event loop) ----
    // These are the PUBLIC API. They always queue the dispatch.
    void broadcast_msg(int msg, int param = 0);
    void trigger_message(int msg);

    // ---- Sync typed event broadcast ----
    // Called from within async-dispatched OnMessage. Stays sync because
    // it's already on the main thread and can't re-enter the caller.
    template <typename EventType> void broadcast(const EventType &ev) {
        ++_broadcast_depth;
        if (_broadcast_depth > 1) {
            pxv_err("Event broadcast loop detected (depth=%d), suppressing",
                    _broadcast_depth);
            assert(_broadcast_depth <= 1 && "Event broadcast loop detected");
            --_broadcast_depth;
            return;
        }
        for (auto *l : _event_listeners) {
            l->on_event(ev);
        }
        --_broadcast_depth;
    }

    // ---- Sync dispatch to specific callback interface ----
    // Used by helper methods (data_updated, frame_began, etc.) that are
    // already called from the correct thread.
    template <typename Iface, typename F> void dispatch_to(F fn) {
        for (auto *cb : _callbacks) {
            if (auto *iface = dynamic_cast<Iface *>(cb))
                fn(iface);
        }
    }

private:
    // ---- Internal sync dispatch (called from queued lambda) ----
    void broadcast_msg_sync(int msg, int param);
    void trigger_message_sync(int msg);

    std::vector<ISessionCallbackBase *> _callbacks;
    std::vector<IMessageListener *> _msg_listeners;
    std::vector<interface::IEventListener *> _event_listeners;
    static thread_local int _broadcast_depth;
};

} // namespace core
} // namespace pv

#endif // PXVIEW_CORE_EVENTBUS_H
