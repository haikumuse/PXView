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
 * Three dispatch paths, all typed:
 *   * broadcast<T>() — synchronous. Invoked from within the async-dispatched
 *     handler (or from the main thread directly), so it already runs after the
 *     caller's stack frame has unwound. A thread_local _broadcast_depth guard
 *     prevents re-entrant typed event dispatch.
 *   * broadcast_sync<T>() — synchronous direct dispatch for the few pre/post
 *     ordering codes that MUST run synchronously BEFORE the state mutation
 *     (e.g. CurrentDeviceChangePrev / StartCollectWorkPrev / StoreConfPrev).
 *     Callers must guarantee they are on the main thread. Shares the same
 *     _broadcast_depth re-entrancy guard as broadcast<T>().
 *   * broadcast_async<T>() — asynchronous. Queues a typed event onto the qApp
 *     event loop via Qt::QueuedConnection, so worker threads (e.g. libsigrok
 *     data-feed callbacks) can emit typed events without touching QWidget from
 *     a non-GUI thread. The event is captured BY VALUE (copy) so it survives
 *     the caller's stack frame.
 *
 * SigSession is NOT a QObject, so broadcast_async queues on qApp which always
 * has a running event loop in both GUI and headless modes.
 */
class EventBus {
public:
    EventBus();
    ~EventBus();

    // ---- Listener registration ----
    void add_callback(ISessionCallbackBase *cb);
    void remove_callback(ISessionCallbackBase *cb);
    void add_event_listener(interface::IEventListener *l);
    void remove_event_listener(interface::IEventListener *l);

    // ---- Listener queries ----
    bool has_callbacks() const { return !_callbacks.empty(); }

    // ---- Sync typed event broadcast ----
    // Synchronous dispatch to all registered IEventListener consumers. Called
    // from within the async-dispatched handler (or directly from the main
    // thread), so it stays sync and can't re-enter the caller.
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

    // ---- Sync direct typed event broadcast (pre-broadcast ordering) ----
    // modernize-core-layer-radical Task 9: explicit synchronous dispatch for
    // the few pre/post ordering codes that MUST run synchronously BEFORE the
    // state mutation (e.g. CurrentDeviceChangePrev / StartCollectWorkPrev
    // / StoreConfPrev). Unlike broadcast(), this is intended to be called
    // directly from the mutating thread (not from within an async-dispatched
    // handler), so callers must guarantee they are on the main thread.
    // Shares the same thread_local _broadcast_depth re-entrancy guard as
    // broadcast().
    template <typename EventType> void broadcast_sync(const EventType &ev) {
        ++_broadcast_depth;
        if (_broadcast_depth > 1) {
            pxv_err("Event broadcast_sync loop detected (depth=%d), suppressing",
                    _broadcast_depth);
            assert(_broadcast_depth <= 1 && "Event broadcast_sync loop detected");
            --_broadcast_depth;
            return;
        }
        for (auto *l : _event_listeners) {
            l->on_event(ev);
        }
        --_broadcast_depth;
    }

    // ---- Async typed event broadcast (worker-thread → main-thread) ----
    // modernize-core-layer-radical Task 13: queues a typed event onto the qApp
    // event loop via Qt::QueuedConnection, so worker threads (e.g. libsigrok
    // data-feed callbacks invoking DataFeedParser::feed_in_*) can emit typed
    // events without touching QWidget from a non-GUI thread. The event is
    // captured BY VALUE (copy) so it survives the caller's stack frame. Empty
    // event structs (e.g. DataUpdated) incur no copy cost. Once dispatched on
    // the main thread, on_event handlers run inside _broadcast_depth guard.
    template <typename EventType> void broadcast_async(const EventType &ev) {
        // Capture event by value to avoid dangling references.
        // `this` is safe — EventBus outlives all worker threads (owned by
        // SigSession unique_ptr, destroyed after device threads join).
        QMetaObject::invokeMethod(qApp, [this, ev]() {
            broadcast(ev);
        }, Qt::QueuedConnection);
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
    std::vector<ISessionCallbackBase *> _callbacks;
    std::vector<interface::IEventListener *> _event_listeners;
    static thread_local int _broadcast_depth;
};

} // namespace core
} // namespace pv

#endif // PXVIEW_CORE_EVENTBUS_H
