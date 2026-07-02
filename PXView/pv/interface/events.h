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
// STATUS (B1.1 fix): This is currently FRONT-INFRASTRUCTURE with 0 IEventListener
// consumers and 0 direct broadcast<T>() emission points. The OnMessage() translation
// table covers 14 of 43 DSV_MSG_* codes. 4 event structs (DecodeDone/SignalsChanged/
// DataUpdated/SampleRateChanged) are double dead-code (no emitter AND no consumer).
// The "new code MUST use IEventListener" hard constraint has been REMOVED from
// AGENTS.md — this interface is now RECOMMENDED, not mandatory. Full migration
// (registering MainWindow sub-components as IEventListener consumers) is deferred
// until MainWindow::OnMessage is split into per-responsibility handlers (Task 9/C5).
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

// The Windows SDK (shobjidl.h / objbase.h) defines `interface` as a
// preprocessor macro for COM interface declarations. This conflicts with the
// `namespace interface` declaration below. When this header is included into a
// translation unit that has already pulled in the Windows SDK (e.g. via
// mainframe.h -> wintaskbarprogress.h -> shobjidl.h), the macro is cleared so
// the namespace declaration parses correctly. The macro is NOT restored
// afterwards: PXView's own code does not use the `interface` COM macro
// anywhere (verified by grep), and restoring it would break any subsequent
// `pv::interface::` qualified name in the including file (e.g. mainwindow.h's
// `public pv::interface::IEventListener` base-class clause). Windows SDK
// headers that define `interface` carry include guards, so re-including them
// after this header will not re-define the macro anyway.
#ifdef interface
#  undef interface
#  define PXVIEW_EVENTS_UNDONE_INTERFACE 1
#endif

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

// DSV_MSG_START_COLLECT_WORK — capture starting.
struct StartCollectWork {};

// DSV_MSG_COLLECT_START — collection started.
struct CollectStart {};

// DSV_MSG_COLLECT_END — collection ended.
struct CollectEnd {};

// DSV_MSG_END_COLLECT_WORK — capture fully stopped.
struct EndCollectWork {};

// DSV_MSG_END_DEVICE_OPTIONS — device options batch update ended.
struct EndDeviceOptions {};

// DSV_MSG_DEVICE_CONFIG_UPDATED — device config changed.
struct DeviceConfigUpdated {};

// DSV_MSG_DEMO_OPERATION_MODE_CHNAGED — demo mode changed.
struct DemoModeChanged {};

// DSV_MSG_DATA_POOL_CHANGED — data pool swapped.
struct DataPoolChanged {};

// DSV_MSG_SIMPLE_TRIGGER_CHANGED — simple trigger (edge) changed.
struct SimpleTriggerChanged {};

// DSV_MSG_GLITCH_FILTER_STARTED — glitch filter task started.
struct GlitchFilterStarted {};

// DSV_MSG_GLITCH_FILTER_PROGRESS — glitch filter progress update.
struct GlitchFilterProgress {
    int progress;  // 0-100
};

// DSV_MSG_GLITCH_FILTER_COMPLETED — glitch filter task completed.
struct GlitchFilterCompleted {};

// DSV_MSG_GLITCH_FILTER_CLEARED — glitch filter cleared.
struct GlitchFilterCleared {};

// DSV_MSG_SIGNAL_INVERT_STARTED — signal invert task started.
struct SignalInvertStarted {};

// DSV_MSG_SIGNAL_INVERT_COMPLETED — signal invert task completed.
struct SignalInvertCompleted {};

// DSV_MSG_SIGNAL_INVERT_CLEARED — signal invert cleared.
struct SignalInvertCleared {};

// DSV_MSG_COPY_IN_PROGRESS_CHANGED — copy thread state changed.
struct CopyInProgressChanged {
    bool in_progress;
};

// DSV_MSG_TRIG_NEXT_COLLECT — trigger next collection (repeat mode).
struct TrigNextCollect {};

// DSV_MSG_CLEAR_DECODE_DATA — decode data cleared.
struct ClearDecodeData {};

// DSV_MSG_APP_OPTIONS_CHANGED — app options changed.
struct AppOptionsChanged {};

// DSV_MSG_FONT_OPTIONS_CHANGED — font options changed.
struct FontOptionsChanged {};

// DSV_MSG_SHORTCUT_CHANGED — shortcut changed.
struct ShortcutChanged {};

// DSV_MSG_STYLE_CHANGED — style changed.
struct StyleChanged {};

// Note on DataUpdated: this struct has no DSV_MSG_* counterpart and no
// emitter yet. It is retained for future use; once a clear "underlying
// sample data updated" emission point is identified (e.g. a snapshot
// post-feed hook in SigSession), add a direct broadcast<DataUpdated>({})
// there. Until then it remains dead-code.

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
    virtual void on_event(const StartCollectWork &) {}
    virtual void on_event(const CollectStart &) {}
    virtual void on_event(const CollectEnd &) {}
    virtual void on_event(const EndCollectWork &) {}
    virtual void on_event(const EndDeviceOptions &) {}
    virtual void on_event(const DeviceConfigUpdated &) {}
    virtual void on_event(const DemoModeChanged &) {}
    virtual void on_event(const DataPoolChanged &) {}
    virtual void on_event(const SimpleTriggerChanged &) {}
    virtual void on_event(const GlitchFilterStarted &) {}
    virtual void on_event(const GlitchFilterProgress &) {}
    virtual void on_event(const GlitchFilterCompleted &) {}
    virtual void on_event(const GlitchFilterCleared &) {}
    virtual void on_event(const SignalInvertStarted &) {}
    virtual void on_event(const SignalInvertCompleted &) {}
    virtual void on_event(const SignalInvertCleared &) {}
    virtual void on_event(const CopyInProgressChanged &) {}
    virtual void on_event(const TrigNextCollect &) {}
    virtual void on_event(const ClearDecodeData &) {}
    virtual void on_event(const AppOptionsChanged &) {}
    virtual void on_event(const FontOptionsChanged &) {}
    virtual void on_event(const ShortcutChanged &) {}
    virtual void on_event(const StyleChanged &) {}
};

} // namespace interface
} // namespace pv

#ifdef PXVIEW_EVENTS_UNDONE_INTERFACE
#  undef PXVIEW_EVENTS_UNDONE_INTERFACE
#endif

#endif // PXVIEW_PV_INTERFACE_EVENTS_H
