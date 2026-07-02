#include "eventbus.h"

#include <algorithm>
#include <assert.h>

namespace pv {
namespace core {

thread_local int EventBus::_broadcast_depth = 0;

EventBus::EventBus() {}

EventBus::~EventBus() {}

void EventBus::add_callback(ISessionCallbackBase *cb) {
    if (cb)
        _callbacks.push_back(cb);
}

void EventBus::remove_callback(ISessionCallbackBase *cb) {
    auto it = std::find(_callbacks.begin(), _callbacks.end(), cb);
    if (it != _callbacks.end())
        _callbacks.erase(it);
}

void EventBus::add_msg_listener(IMessageListener *l) {
    if (l)
        _msg_listeners.push_back(l);
}

void EventBus::add_event_listener(interface::IEventListener *l) {
    if (!l)
        return;
    if (std::find(_event_listeners.begin(), _event_listeners.end(), l) !=
        _event_listeners.end())
        return;
    _event_listeners.push_back(l);
}

void EventBus::remove_event_listener(interface::IEventListener *l) {
    auto it = std::find(_event_listeners.begin(), _event_listeners.end(), l);
    if (it != _event_listeners.end())
        _event_listeners.erase(it);
}

// ---- Async broadcast ----
void EventBus::broadcast_msg(int msg, int param) {
    // Queue the actual dispatch onto qApp's event loop.
    // This ensures the caller's stack frame completes before any listener
    // processes the message, eliminating re-entrant UAF.
    QMetaObject::invokeMethod(qApp, [this, msg, param]() {
        broadcast_msg_sync(msg, param);
    }, Qt::QueuedConnection);
}

void EventBus::trigger_message(int msg) {
    // Queue the full trigger_message (ITriggerCallback dispatch + broadcast_msg)
    // to the next event-loop iteration.
    QMetaObject::invokeMethod(qApp, [this, msg]() {
        trigger_message_sync(msg);
    }, Qt::QueuedConnection);
}

// ---- Internal sync dispatch ----
void EventBus::broadcast_msg_sync(int msg, int param) {
    for (IMessageListener *cb : _msg_listeners) {
        cb->OnMessage(msg, param);
    }
}

void EventBus::trigger_message_sync(int msg) {
    // ITriggerCallback dispatch (MainWindow via EventObject, SessionService)
    dispatch_to<ITriggerCallback>(
        [msg](ITriggerCallback *cb) { cb->trigger_message(msg); });
    // IMessageListener dispatch (SigSession::OnMessage, MainWindow, SessionService)
    broadcast_msg_sync(msg, 0);
}

} // namespace core
} // namespace pv
