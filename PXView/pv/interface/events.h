/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

// Typed event bus for PXView.
//
// This header defines a set of semantic event structs (one per DSV_MSG_*
// notification) and the IEventListener interface. Each event carries its full
// context as typed fields rather than a bare (int msg, int param) pair, so
// consumers cannot accidentally mis-handle a message code or forget a payload.
//
// New code SHOULD register an IEventListener with SigSession and override only
// the event handlers it cares about. The legacy IMessageListener / DSV_MSG_*
// path (interface/icallbacks.h) is retained as a compatibility shim: SigSession
// translates every relevant DSV_MSG_* into the matching typed event inside
// OnMessage(), so both listener kinds run in parallel during the migration.
//
// Layer: this is a Core-layer header. It may depend only on Qt6::Core and the
// STL — it MUST NOT include QWidget/QMainWindow/QDialog or any pv/view/*.h.

#ifndef PXVIEW_PV_INTERFACE_EVENTS_H
#define PXVIEW_PV_INTERFACE_EVENTS_H

#include <stdint.h>

#include <QString>

#include "../data/triggerconfig.h"  // for pv::data::TriggerConfig (complete type)

namespace pv {

// Forward declarations to avoid heavy includes. SessionDocument pulls in many
// Core data headers; a pointer member only needs the forward declaration.
namespace data {
class SessionDocument;
}

namespace interface {

// ---------------------------------------------------------------------------
// Semantic event structs.
//
// Field type notes:
//   * device_status holds a DEVICE_STATUS_TYPE value (ST_INIT/ST_RUNNING/
//     ST_STOPPED). That enum lives in sigsession.h (pv namespace). To keep
//     events.h decoupled from sigsession.h (which would create a circular
//     include — sigsession.h includes events.h), the field is typed as int.
//   * mode fields similarly hold DEVICE_COLLECT_MODE / work-mode integer
//     constants and are typed as int for the same reason.
// ---------------------------------------------------------------------------

// DSV_MSG_CAPTURE_STATE_CHANGED — capture started / stopped.
struct CaptureStateChanged {
    bool is_working;
    int  device_status;  // DEVICE_STATUS_TYPE (ST_INIT/ST_RUNNING/ST_STOPPED)
};

// DSV_MSG_CAPTURE_OWNER_CHANGED — the SessionDocument owning the live capture
// changed. old_owner is nullptr when emitted from the OnMessage() compat path
// because the legacy (int,int) call site has already mutated the owner before
// broadcasting and cannot recover the previous value.
struct CaptureOwnerChanged {
    data::SessionDocument *old_owner;
    data::SessionDocument *new_owner;
};

// DSV_MSG_TRIGGER_CONFIG_CHANGED — advanced/serial trigger config was rewritten.
// config points at SigSession::_trigger_config and is only valid for the
// duration of the dispatch (do not store).
struct TriggerConfigChanged {
    const data::TriggerConfig *config;
};

// DSV_MSG_SAMPLE_COUNT_UPDATED — sample-depth / sample-count metadata changed.
struct SampleCountUpdated {
    uint64_t sample_count;
};

// DSV_MSG_DEVICE_OPTIONS_UPDATED — device options changed; signals need reload.
struct DeviceOptionsUpdated {};

// DSV_MSG_ACTIVE_DOCUMENT_CHANGED — the active SessionDocument switched.
struct ActiveDocumentChanged {
    data::SessionDocument *old_doc;
    data::SessionDocument *new_doc;
};

// DSV_MSG_COPY_TO_DOC_DONE — background copy of capture data into a document
// finished; decoders can now be started.
struct CopyToDocDone {
    data::SessionDocument *doc;
};

// Decode task finished.
struct DecodeDone {};

// Signal list changed.
struct SignalsChanged {};

// Underlying sample data updated.
struct DataUpdated {};

// DSV_MSG_DEVICE_MODE_CHANGED — LOGIC/DSO/ANALOG work mode switched.
struct DeviceModeChanged {
    int mode;  // LOGIC/DSO/ANALOG
};

// DSV_MSG_COLLECT_MODE_CHANGED — single/repeat/loop collect mode switched.
struct CollectModeChanged {
    int mode;  // DEVICE_COLLECT_MODE (COLLECT_SINGLE/COLLECT_REPEAT/COLLECT_LOOP)
};

// DSV_MSG_DEVICE_LIST_UPDATED — the device list changed.
struct DeviceListUpdated {};

// DSV_MSG_CURRENT_DEVICE_CHANGED — the current device selection changed.
struct CurrentDeviceChanged {};

// DSV_MSG_NEW_USB_DEVICE — a USB device arrived.
struct UsbDeviceArrived {};

// DSV_MSG_CURRENT_DEVICE_DETACHED — the current device was detached.
struct DeviceDetached {};

// DSV_MSG_DEVICE_DURATION_UPDATED / sample-rate changed.
struct SampleRateChanged {};

// DSV_MSG_SAVE_COMPLETE — save operation finished.
struct SaveComplete {};

// ---------------------------------------------------------------------------
// IEventListener — typed event consumer interface.
//
// Each event type has its own virtual overload with a default empty
// implementation, so a subclass overrides only the events it cares about. This
// is non-intrusive (no CRTP, no std::variant visitor boilerplate) and keeps
// adding a new event to a one-line change here plus the new struct above.
//
// Implementations MUST NOT call back into SigSession::broadcast() synchronously
// from within an on_event() handler in a way that re-emits the same event — the
// SigSession::broadcast<>() template carries a thread-local re-entrancy guard
// that short-circuits nested broadcasts to prevent infinite recursion / stack
// overflow (see sigsession.h).
// ---------------------------------------------------------------------------
class IEventListener {
public:
    virtual ~IEventListener() = default;

    virtual void on_event(const CaptureStateChanged &) {}
    virtual void on_event(const CaptureOwnerChanged &) {}
    virtual void on_event(const TriggerConfigChanged &) {}
    virtual void on_event(const SampleCountUpdated &) {}
    virtual void on_event(const DeviceOptionsUpdated &) {}
    virtual void on_event(const ActiveDocumentChanged &) {}
    virtual void on_event(const CopyToDocDone &) {}
    virtual void on_event(const DecodeDone &) {}
    virtual void on_event(const SignalsChanged &) {}
    virtual void on_event(const DataUpdated &) {}
    virtual void on_event(const DeviceModeChanged &) {}
    virtual void on_event(const CollectModeChanged &) {}
    virtual void on_event(const DeviceListUpdated &) {}
    virtual void on_event(const CurrentDeviceChanged &) {}
    virtual void on_event(const UsbDeviceArrived &) {}
    virtual void on_event(const DeviceDetached &) {}
    virtual void on_event(const SampleRateChanged &) {}
    virtual void on_event(const SaveComplete &) {}
};

} // namespace interface
} // namespace pv

#endif // PXVIEW_PV_INTERFACE_EVENTS_H
