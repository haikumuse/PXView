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

} // namespace core
} // namespace pv
