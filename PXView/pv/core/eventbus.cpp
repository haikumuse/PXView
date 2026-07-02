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

void EventBus::trigger_message(int msg, int param) {
    // Queue the full trigger_message (ITriggerCallback dispatch + broadcast_msg)
    // to the next event-loop iteration.
    QMetaObject::invokeMethod(qApp, [this, msg, param]() {
        trigger_message_sync(msg, param);
    }, Qt::QueuedConnection);
}

// ---- Internal sync dispatch ----
void EventBus::broadcast_msg_sync(int msg, int param) {
    for (IMessageListener *cb : _msg_listeners) {
        cb->OnMessage(msg, param);
    }
}

void EventBus::trigger_message_sync(int msg, int param) {
    // ITriggerCallback dispatch (MainWindow via EventObject, SessionService).
    // ITriggerCallback::trigger_message(int) does not take a param — extend
    // that interface if a trigger-callback consumer ever needs the payload.
    dispatch_to<ITriggerCallback>(
        [msg](ITriggerCallback *cb) { cb->trigger_message(msg); });
    // IMessageListener dispatch (SigSession::OnMessage, MainWindow,
    // SessionService) — param is forwarded so typed-event translators (e.g.
    // SigSession::OnMessage DSV_MSG_GLITCH_FILTER_PROGRESS branch) can populate
    // event structs like GlitchFilterProgress{param}.
    broadcast_msg_sync(msg, param);
}

} // namespace core
} // namespace pv
