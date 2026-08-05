// mainwindow_event_dispatcher.cpp
// Phase 2: SessionEventDispatcher — extracted from MainWindow's IEventListener
// implementation. All 45 typed on_event overrides moved here verbatim;
// references to MainWindow members go through _window-> (friend access).

#include "mainwindow_event_dispatcher.h"

#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStatusBar>
#include <QTimer>

#include "log.h"
#include "mainwindow.h"
#include "mainwindow_dock_manager.h"
#include "sigsession.h"
#include "tabcontext.h"
#include "data/sessiondocument.h"
#include "ui/uimanager.h"
#include "ui/msgbox.h"
#include "ui/langresource.h"
#include "ui/string_ids.h"
#include "view/view.h"
#include "dock/protocoldock.h"
#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/deviceoptionsdock.h"
#include "dock/searchdock.h"
#include "dialogs/dsmessagebox.h"
#include "storesession.h"
#include "toolbars/samplingbar.h"
#include "toolbars/logobar.h"
#include "toolbars/titlebar.h"
#include "widgets/sidebar.h"
#include "pxvdef.h"
#include "api/types.h"

// build_channel_layout is accessed via MainWindow::build_channel_layout()

using namespace pv;

// ===========================================================================
// Helper macros — MainWindow's private members are accessed via _window->
// ===========================================================================

// P1 guard: SessionEventDispatcher is an IEventListener registered with the
// global EventBus. Events are broadcast via Qt::QueuedConnection onto qApp's
// event loop, so a queued event may still fire AFTER MainWindow has been
// destroyed. _window is a QPointer, so it becomes null on teardown — bail out
// of every handler the moment the owning window is gone.
#define PV_WIN_GUARD() \
  do {                 \
    if (_window.isNull()) return; \
  } while (0)

namespace pv {
// P0/P1: Safely resolve the current view. Returns nullptr if MainWindow is
// gone (QPointer) or no tab is active. Callers MUST null-check the result
// before dereferencing.
view::View *SessionEventDispatcher::safe_current_view() const {
  if (_window.isNull())
    return nullptr;
  return _window->current_view();
}
} // namespace pv

// --- Capture state group ---
void SessionEventDispatcher::on_event(const pv::interface::CaptureStateChanged &) {
  _window->update_capture_ui_status();
}
void SessionEventDispatcher::on_event(const pv::interface::StartCollectWork &) {
  PV_WIN_GUARD();
  _window->update_capture_ui_status();
  if (_window->_session->is_instant()) {
    _window->_dock_manager->side_bar()->setItemRunning(_window->SIDEBAR_INSTANT, true);
  } else {
      _window->_dock_manager->side_bar()->setItemRunning(_window->SIDEBAR_RUNSTOP, true);
  }
  if (auto *v = safe_current_view()) v->on_state_changed(false);
}
void SessionEventDispatcher::on_event(const pv::interface::CollectStart &) {
  _window->statusBar()->showMessage(MainWindow::tr("采集中..."), 3000);
}
void SessionEventDispatcher::on_event(const pv::interface::CollectEnd &) {
  PV_WIN_GUARD();
  _window->prgRate(0);
  if (auto *v = safe_current_view()) {
    v->repeat_unshow();
    v->on_state_changed(true);
  }
}
void SessionEventDispatcher::on_event(const pv::interface::EndCollectWork &) {
  _window->update_capture_ui_status();

  pv::TabContext *ctx = _window->current_context();
  if (ctx && ctx->document() && ctx->document()->has_pending_config()) {
    ctx->document()->apply_pending_config();
    for (const auto &ch : ctx->document()->get_signal_config().channels) {
      auto m = _window->_session->get_signal_by_index(ch.index);
      if (m)
        m->set_trig_type(ch.trig_type);
    }
    _window->_dock_manager->device_options_widget()->update_view();
  }
  if (ctx) {
    _window->_session->set_active_document(ctx->document());
  }
}
void SessionEventDispatcher::on_event(const pv::interface::TrigNextCollect &) {
  _window->statusBar()->showMessage(MainWindow::tr("等待下一次采集..."), 3000);
}

// --- Device management group ---
void SessionEventDispatcher::on_event(const pv::interface::DeviceListUpdated &) {
  _window->_sampling_bar->update_device_list();
}
void SessionEventDispatcher::on_event(const pv::interface::CurrentDeviceChanged &) {
  PV_WIN_GUARD();
  _window->reset_all_view();
  _window->load_device_config();
  _window->update_title_bar_text();
  _window->_sampling_bar->update_device_list();
  _window->_sampling_bar->reload();
  _window->_dock_manager->device_options_widget()->update_view();
  _window->_logo_bar->dsl_connected(_window->_session->get_device()->is_hardware());
  _window->update_toolbar_view_status();
  _window->_session->device_event_object()->device_updated();

  {
    pv::TabContext *ctx = _window->current_context();
    if (ctx && ctx->document()) {
      ctx->document()->save_signal_config(
          _window->_session->get_signal_models(),
          _window->build_channel_layout(safe_current_view()));
      if (auto *v = safe_current_view()) v->rebuild_signals();
      pxv_info("CurrentDeviceChanged: saved config and rebuilt signals for current tab");
    }
  }

  if (_window->_device_agent->is_hardware()) {
    _window->_session->on_load_config_end();
  }

  if (_window->_device_agent->get_work_mode() == LOGIC &&
      _window->_device_agent->is_file() == false)
    if (auto *v = safe_current_view()) v->auto_set_max_scale();

  if (_window->_device_agent->is_file()) {
    _window->check_config_file_version();

    bool bDoneDecoder = false;
    bool bLoadSuccess = false;
    QJsonDocument doc =
        _window->get_config_json_from_data_file(_window->_device_agent->path(), bLoadSuccess);

    if (bLoadSuccess) {
      _window->load_config_from_json(doc, bDoneDecoder);
    }

    if (!bDoneDecoder && _window->_device_agent->get_work_mode() == LOGIC) {
      QJsonArray deArray = _window->get_decoder_json_from_data_file(
          _window->_device_agent->path(), bLoadSuccess);
      if (bLoadSuccess) {
        StoreSession ss(_window->_session);
        ss.load_decoders(_window->_dock_manager->protocol_widget(), deArray);
      }
    }

    if (auto *v = safe_current_view()) v->update_all_trace_postion();
    if (!_window->_device_agent->is_input_module()) {
      QTimer::singleShot(100, _window,
                         [this]() { _window->_session->start_capture(true); });
    }
  } else if (_window->_device_agent->is_demo()) {
    if (_window->_device_agent->get_work_mode() == LOGIC) {
      _window->_pattern_mode = _window->_device_agent->get_demo_operation_mode();
      _window->_dock_manager->protocol_widget()->del_all_protocol();
      if (auto *v = safe_current_view()) v->auto_set_max_scale();

      if (_window->_pattern_mode != "random") {
        _window->load_demo_decoder_config(_window->_pattern_mode);
      }
    }
  }

  _window->calc_min_height();

  if (_window->_device_agent->is_hardware() && _window->_device_agent->is_new_device()) {
    _window->check_usb_device_speed();
  }
}
void SessionEventDispatcher::on_event(const pv::interface::UsbDeviceArrived &) {
  if (_window->_msg != nullptr) {
    _window->_msg->close();
    _window->_msg = nullptr;
  }

  _window->_sampling_bar->update_device_list();

  if (_window->_session->get_device()->is_hardware() && _window->_session->is_working()) {
    return;
  }

  if (_window->_session->get_device()->is_demo() == false && !_window->_is_save_confirm_msg) {
    QString msgText = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_TO_SWITCH_DEVICE),
                          "To switch the new device?");
    if (MsgBox::Confirm(msgText, "", &_window->_msg, nullptr) == false) {
      _window->_msg = nullptr;
      return;
    }
    _window->_msg = nullptr;
  }

  if (_window->_is_save_confirm_msg) {
    pxv_info("New device attached:Waitting for the confirm box be closed.");
    _window->_is_auto_switch_device = true;
    return;
  }

  if (_window->_session->is_saving()) {
    pxv_info("New device attached:Waitting for store the data. and will switch to new device.");
    _window->_is_auto_switch_device = true;
    return;
  }

  int mode = _window->_device_agent->get_work_mode();

  if (mode != DSO && _window->confirm_to_store_data()) {
    _window->_is_auto_switch_device = true;
    if (_window->_session->is_working())
      _window->_session->stop_capture();
    _window->on_save();
  } else {
    if (_window->_session->is_working())
      _window->_session->stop_capture();
    _window->_session->set_default_device();
  }
}
void SessionEventDispatcher::on_event(const pv::interface::DeviceDetached &) {
  if (_window->_msg != nullptr) {
    _window->_msg->close();
    _window->_msg = nullptr;
  }

  _window->_session->device_event_object()->device_updated();
  _window->save_config();

  if (_window->_session->is_saving()) {
    pxv_info("Device detached:Waitting for store the data. and will switch to new device.");
    _window->_is_auto_switch_device = true;
    return;
  }

  if (_window->confirm_to_store_data()) {
    _window->_is_auto_switch_device = true;
    _window->on_save();
  } else {
    _window->_session->set_default_device();
  }
}
void SessionEventDispatcher::on_event(const pv::interface::DeviceOpenFailed &evt) {
  QString driver = QString::fromStdString(evt.driver_name);
  QString err = QString::fromStdString(evt.error_message);
  QString title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_OPEN_FAILED),
                       "Failed to open device");
  QString text;
  if (err.isEmpty()) {
    text = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_OPEN_FAILED_REASON),
               "The device could not be opened. Check USB connection, "
               "driver, and that no other program is using it.");
  } else {
    text = err;
  }
  if (!driver.isEmpty()) {
    text = QString("[%1] %2").arg(driver, text);
  }
  pxv_err("DeviceOpenFailed: driver=%s reason=%s",
          driver.toUtf8().constData(), err.toUtf8().constData());
  MsgBox::Show(title, text, _window);
}

// --- Device options group ---
void SessionEventDispatcher::on_event(const pv::interface::DeviceOptionsUpdated &) {
  _window->_dock_manager->trigger_widget()->device_updated();
  _window->_dock_manager->device_options_widget()->device_updated();
  _window->_dock_manager->measure_widget()->reload();

  pv::TabContext *ctx = _window->current_context();
  if (ctx && ctx->document()) {
    ctx->document()->save_signal_config(
        _window->_session->get_signal_models(), _window->build_channel_layout(safe_current_view()));
  }

  if (auto *v = safe_current_view()) {
    v->rebuild_signals();
    v->signals_changed(nullptr);
  }
}
void SessionEventDispatcher::on_event(const pv::interface::DsoViewOptionChanged &) {
  _window->_dock_manager->trigger_widget()->device_updated();
  _window->_dock_manager->device_options_widget()->device_updated();
  _window->_dock_manager->measure_widget()->reload();

  pv::TabContext *ctx = _window->current_context();
  if (ctx && ctx->document()) {
    ctx->document()->save_signal_config(
        _window->_session->get_signal_models(), _window->build_channel_layout(safe_current_view()));
  }
}
void SessionEventDispatcher::on_event(const pv::interface::SampleRateChanged &) {
  _window->_dock_manager->trigger_widget()->device_updated();
  if (auto *v = safe_current_view()) v->timebase_changed();
}
void SessionEventDispatcher::on_event(const pv::interface::SampleCountUpdated &) {
  _window->_sampling_bar->update_sample_count_selector();
}
void SessionEventDispatcher::on_event(const pv::interface::DeviceModeChanged &) {
  PV_WIN_GUARD();
  if (auto *v = safe_current_view()) v->mode_changed();
  _window->reset_all_view();
  _window->load_device_config();
  _window->update_title_bar_text();
  _window->_dock_manager->device_options_widget()->on_mode_changed();
  _window->update_toolbar_view_status();
  _window->_sampling_bar->update_sample_rate_list();
  _window->_sampling_bar->reload();

  {
    pv::TabContext *ctx = _window->current_context();
    if (ctx && ctx->document()) {
      ctx->document()->save_signal_config(
          _window->_session->get_signal_models(),
          _window->build_channel_layout(safe_current_view()));
      if (auto *v = safe_current_view()) v->rebuild_signals();
      pxv_info("DeviceModeChanged: saved config and rebuilt signals for current tab");
    }
  }

  if (_window->_device_agent->is_hardware())
    _window->_session->on_load_config_end();

  if (_window->_device_agent->get_work_mode() == LOGIC)
    if (auto *v = safe_current_view()) v->auto_set_max_scale();

  if (_window->_device_agent->is_demo()) {
    _window->_pattern_mode = _window->_device_agent->get_demo_operation_mode();
    _window->_dock_manager->protocol_widget()->del_all_protocol();

    if (_window->_device_agent->get_work_mode() == LOGIC) {
      if (_window->_pattern_mode != "random") {
        _window->_device_agent->update();
        _window->load_demo_decoder_config(_window->_pattern_mode);
      }
    }
  }

  _window->calc_min_height();
}
void SessionEventDispatcher::on_event(const pv::interface::CollectModeChanged &) {
  PV_WIN_GUARD();
  if (_window->_device_agent->is_demo()) {
    _window->_pattern_mode = _window->_device_agent->get_demo_operation_mode();
  }
  _window->_dock_manager->trigger_widget()->device_updated();
  if (auto *v = safe_current_view()) v->update();
}
void SessionEventDispatcher::on_event(const pv::interface::EndDeviceOptions &) {
  if (_window->_device_agent->is_demo() && _window->_device_agent->get_work_mode() == LOGIC) {
    QString pattern_mode = _window->_device_agent->get_demo_operation_mode();

    if (pattern_mode != _window->_pattern_mode) {
      _window->_pattern_mode = pattern_mode;

      _window->_device_agent->update();
      _window->_session->clear_view_data();
      _window->_session->init_signals();
      _window->update_toolbar_view_status();
      _window->_sampling_bar->update_sample_rate_list();
      _window->_dock_manager->protocol_widget()->del_all_protocol();

      if (_window->_pattern_mode != "random") {
        _window->_session->set_collect_mode(COLLECT_SINGLE);
        _window->load_demo_decoder_config(_window->_pattern_mode);
        _window->_session->start_capture(false);
      }
    }
  }
  _window->calc_min_height();
}
void SessionEventDispatcher::on_event(const pv::interface::DemoModeChanged &) {
  if (_window->_device_agent->is_demo() && _window->_device_agent->get_work_mode() == LOGIC) {
    QString pattern_mode = _window->_device_agent->get_demo_operation_mode();

    if (pattern_mode != _window->_pattern_mode) {
      _window->_pattern_mode = pattern_mode;

      _window->_device_agent->update();
      _window->_session->clear_view_data();
      _window->_session->init_signals();
      _window->update_toolbar_view_status();
      _window->_sampling_bar->update_sample_rate_list();
      _window->_dock_manager->protocol_widget()->del_all_protocol();

      if (_window->_pattern_mode != "random") {
        _window->_session->set_collect_mode(COLLECT_SINGLE);
        _window->load_demo_decoder_config(_window->_pattern_mode);
      }
    }
  }
  _window->calc_min_height();
}

// --- UI options group ---
void SessionEventDispatcher::on_event(const pv::interface::AppOptionsChanged &) {
  _window->update_title_bar_text();
}
void SessionEventDispatcher::on_event(const pv::interface::FontOptionsChanged &) {
  UiManager::Instance()->Update(UI_UPDATE_ACTION_FONT);
}
void SessionEventDispatcher::on_event(const pv::interface::ShortcutChanged &) {
}
void SessionEventDispatcher::on_event(const pv::interface::StyleChanged &) {
  UiManager::Instance()->Update(UI_UPDATE_ACTION_THEME);
  for (QWidget *w : qApp->topLevelWidgets()) {
    w->update();
  }
}

// --- Data group ---
void SessionEventDispatcher::on_event(const pv::interface::DataPoolChanged &) {
  PV_WIN_GUARD();
  if (auto *v = safe_current_view()) v->check_measure();
}
void SessionEventDispatcher::on_event(const pv::interface::CopyInProgressChanged &) {
  if (_window->_disk_cache_status_label)
    _window->_disk_cache_status_label->setText(MainWindow::tr("后台数据拷贝中..."));
}
void SessionEventDispatcher::on_event(const pv::interface::ActiveDocumentChanged &) {
  _window->update_title_bar_text();
}
void SessionEventDispatcher::on_event(const pv::interface::SaveComplete &) {
  _window->_session->clear_store_confirm_flag();

  if (_window->_is_auto_switch_device) {
    _window->_is_auto_switch_device = false;
    _window->_session->set_default_device();
  } else {
    ds_device_handle devh = _window->_sampling_bar->get_next_device_handle();
    if (devh != NULL_HANDLE) {
      pxv_info("Auto switch to the selected device.");
      _window->_session->set_device(devh);
    }
  }
}
void SessionEventDispatcher::on_event(const pv::interface::ClearDecodeData &) {
  if (_window->_device_agent->get_work_mode() == LOGIC)
    _window->_dock_manager->protocol_widget()->reset_view();
}

// --- Filter / invert group ---
void SessionEventDispatcher::on_event(const pv::interface::GlitchFilterStarted &) {
  if (_window->_disk_cache_status_label)
    _window->_disk_cache_status_label->setText(MainWindow::tr("毛刺滤波处理中..."));
}
void SessionEventDispatcher::on_event(const pv::interface::GlitchFilterProgress &e) {
  int p = e.progress;
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  _window->statusBar()->showMessage(
      MainWindow::tr("毛刺滤波进行中... %1%").arg(p), 2000);
}
void SessionEventDispatcher::on_event(const pv::interface::GlitchFilterCompleted &) {
  pv::TabContext *ctx = _window->current_context();
  if (ctx && ctx->document()) {
    _window->_session->copy_data_to_document(ctx->document());
  }
  _window->_session->restart_decoders();
  if (auto *v = safe_current_view()) {
    v->on_glitch_filter_completed();
  }
}
void SessionEventDispatcher::on_event(const pv::interface::GlitchFilterCleared &) {
  pv::TabContext *ctx = _window->current_context();
  if (ctx && ctx->document()) {
    _window->_session->copy_data_to_document(ctx->document());
  }
  _window->_session->restart_decoders();
  if (auto *v = safe_current_view()) {
    v->on_glitch_filter_cleared();
  }
}
void SessionEventDispatcher::on_event(const pv::interface::SignalInvertStarted &) {
  if (_window->_disk_cache_status_label)
    _window->_disk_cache_status_label->setText(MainWindow::tr("信号反相处理中..."));
}
void SessionEventDispatcher::on_event(const pv::interface::SignalInvertCompleted &) {
  pv::TabContext *ctx2 = _window->current_context();
  if (ctx2 && ctx2->document()) {
    _window->_session->copy_data_to_document(ctx2->document());
  }
  _window->_session->restart_decoders();
}
void SessionEventDispatcher::on_event(const pv::interface::SignalInvertCleared &) {
  pv::TabContext *ctx2 = _window->current_context();
  if (ctx2 && ctx2->document()) {
    _window->_session->copy_data_to_document(ctx2->document());
  }
  _window->_session->restart_decoders();
}

// --- Trigger group ---
void SessionEventDispatcher::on_event(const pv::interface::SimpleTriggerChanged &) {
  if (_window->_dock_manager->trigger_widget()) {
    _window->_dock_manager->trigger_widget()->select_simple_trigger();
  }
}
void SessionEventDispatcher::on_event(const pv::interface::TriggerConfigChanged &) {
  if (_window->_dock_manager->trigger_widget())
    _window->_dock_manager->trigger_widget()->update_view();
}

// --- Empty-body / pre-broadcast overrides ---
void SessionEventDispatcher::on_event(const pv::interface::CaptureOwnerChanged &) {
}
void SessionEventDispatcher::on_event(const pv::interface::CopyToDocDone &) {
  PV_WIN_GUARD();
  pv::TabContext *ctx = _window->current_context();
  if (ctx && ctx->document() && ctx->document()->has_data()) {
    if (auto *v = safe_current_view()) v->set_data_document(ctx->document());
  }
}
void SessionEventDispatcher::on_event(const pv::interface::DecodeDone &) {
  PV_WIN_GUARD();
  _window->on_data_updated();
  if (auto *v = safe_current_view()) {
    v->update();
    v->viewport_update();
  }
  _window->on_decode_done();
}
void SessionEventDispatcher::on_event(const pv::interface::SignalsChanged &) {}
void SessionEventDispatcher::on_event(const pv::interface::DataUpdated &) {
  _window->on_data_updated();
}
void SessionEventDispatcher::on_event(const pv::interface::DeviceConfigUpdated &) {}

void SessionEventDispatcher::on_event(const pv::interface::StoreConfPrev &) {
  if (_window->_device_agent && _window->_device_agent->is_hardware() &&
      _window->_session && !_window->_session->have_hardware_data()) {
    _window->_sampling_bar->commit_settings();
  }
}

void SessionEventDispatcher::on_event(const pv::interface::CurrentDeviceChangePrev &) {
  if (_window->_msg != nullptr) {
    _window->_msg->close();
    _window->_msg = nullptr;
  }
  _window->_dock_manager->protocol_widget()->del_all_protocol();
  if (auto *v = safe_current_view()) v->reload();
}

void SessionEventDispatcher::on_event(const pv::interface::StartCollectWorkPrev &) {
  if (_window->_device_agent->get_work_mode() == LOGIC)
    _window->_dock_manager->trigger_widget()->try_commit_trigger();
  else if (_window->_device_agent->get_work_mode() == DSO)
    _window->_dock_manager->dso_trigger_widget()->check_setting();

  if (auto *v = safe_current_view()) {
    v->capture_init();
    v->on_state_changed(false);
  }
}

void SessionEventDispatcher::on_event(const pv::interface::EndCollectWorkPrev &) {
}

// ---------------------------------------------------------------------------
// IServiceEventListener — route View operation broadcasts from SessionService
// (MCP/WS API) to the active View. In Headless mode there is no MainWindow,
// so these events are simply not consumed.
// Extracted from MainWindow::on_service_event during Phase 2 modernization.
// ---------------------------------------------------------------------------
void SessionEventDispatcher::on_service_event(const pv::api::ServiceEventData &data) {
  pv::view::View *view = safe_current_view();
  if (!view)
    return;

  const auto &params = data.params;

  switch (data.event) {
  case pv::api::ServiceEvent::ViewShowRegion: {
    auto it_start = params.find("start");
    auto it_end = params.find("end");
    if (it_start != params.end() && it_end != params.end()) {
      uint64_t start = std::stoull(it_start->second);
      uint64_t end = std::stoull(it_end->second);
      view->show_region(start, end, true);
    }
    break;
  }
  case pv::api::ServiceEvent::ViewZoomFit: {
    // TODO: View has no zoom_fit() method yet; approximate with zoom out.
    // A proper fit-to-screen implementation should be added to View.
    view->zoom(-1.0);
    break;
  }
  case pv::api::ServiceEvent::ViewZoomIn: {
    view->zoom(1.0);
    break;
  }
  case pv::api::ServiceEvent::ViewZoomOut: {
    view->zoom(-1.0);
    break;
  }
  case pv::api::ServiceEvent::ViewCursorAdded: {
    auto it = params.find("sample_pos");
    if (it != params.end()) {
      uint64_t sample_pos = std::stoull(it->second);
      view->add_cursor(sample_pos);
    }
    break;
  }
  case pv::api::ServiceEvent::ViewCursorRemoved: {
    // Cursor removal by index is handled by View internally;
    // no direct public API to remove by index from outside.
    // TODO: Add View::remove_cursor(int index) if needed.
    break;
  }
  case pv::api::ServiceEvent::ViewCursorsCleared: {
    view->clear_cursors();
    break;
  }
  case pv::api::ServiceEvent::DecoderAdded:
  case pv::api::ServiceEvent::DecoderRemoved:
  case pv::api::ServiceEvent::SignalsChanged: {
    // Core data changed via MCP/API (decoder added/removed or signals
    // changed). Trigger lazy sync so View creates/removes the
    // corresponding DecodeTrace by Core Stack identity comparison.
    view->mark_derived_traces_dirty();
    view->signals_changed(nullptr);
    break;
  }
  default:
    // Not a View event; ignore.
    break;
  }
}
