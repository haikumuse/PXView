/*
 * This file is part of the PXView project.
 * PXView is based on DSView.
 * PXView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include "widgets/searchpatterninput.h"
#include "widgets/sidebar.h"
#include "widgets/smoothscrollarea.h"
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopServices>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>
#include <algorithm>
#include <functional>
#include <libusb-1.0/libusb.h>
#include <stdexcept>

#include "log.h"
#include "mainwindow.h"
#include "mainwindow_config_io.h"
#include "mainwindow_dock_manager.h"
#include "mainwindow_event_dispatcher.h"
#include "mainwindow_tab_manager.h"
#include "mainwindow_theme_manager.h"
#include "mainwindow_status_bar.h"
#include "mainwindow_shortcut_manager.h"

#include "data/analogsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/logicsnapshot.h"

#include "dialogs/about.h"
#include "dialogs/deviceoptions.h"
#include "dialogs/regionoptions.h"
#include "dialogs/storeprogress.h"

#include "toolbars/filebar.h"
#include "toolbars/logobar.h"
#include "toolbars/samplingbar.h"
#include "toolbars/titlebar.h"
#include "toolbars/trigbar.h"

#include "dock/deviceoptionsdock.h"
#include "dock/logdock.h"
#include "dock/mcpcontroldock.h"
#include "dock/functiondock.h"
#include "dock/measuredock.h"
#include "dock/protocoldock.h"
#include "dock/searchdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/triggerdock.h"


#include "data/decoderstack.h"
#include "data/sessiondocument.h"
#include "core/documentregistry.h"
#include "interface/icontextaware.h"
#include "sessionmanager.h"
#include "tabcontext.h"
#include "ui/draggabletabwidget.h"
#include "view/analogsignal.h"
#include "view/dsosignal.h"
#include "view/logicsignal.h"
#include "view/signal.h"
#include "view/trace.h"
#include "view/view.h"
#include "view/viewstatus.h"
#include "view/viewport.h"

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#include "ZipMaker.h"
#include "api/app_service.h"
#include "appcontrol.h"
#include "config/appconfig.h"
#include "config/shortcutdefs.h"
#include "deviceagent.h"
#include "pxvdef.h"
#include "log.h"
#include "mainframe.h"
#include "sigsession.h"
#include "ui/langresource.h"
#include "ui/msgbox.h"
#include "ui/uimanager.h"
#include "utility/encoding.h"
#include "utility/path.h"
#include <glib.h>
#include <inttypes.h>
#include <list>
#include <stdarg.h>
#include <cstdint>
#include <cstdlib>
#include <thread>

#ifdef ENABLE_DEBUG_HELPER
#include "ui/widgetinspector.h"
#endif

#include <QShortcut>
#include <QWidgetAction>

#include <QLabel>
#include <QScrollArea>
#include <QTabBar>
#include <map>

// The Windows SDK (pulled in transitively via mainframe.h -> wintaskbarprogress.h
// -> shobjidl.h, included below mainwindow.h) defines `interface` as a
// preprocessor macro. events.h (included via mainwindow.h) clears it, but only
// if it was defined at that point — in this TU mainframe.h is included AFTER
// mainwindow.h, so the macro is defined after events.h's #undef runs. Clear it
// again here so `pv::interface::` qualified names in the code below parse
// correctly. PXView does not use the `interface` COM macro anywhere.
#ifdef interface
#  undef interface
#endif

namespace pv {

namespace {
QString tmp_file;

/** Build a channel-index → ChannelLayoutState map from the View's signal list.
 * Task 7 (unify-signal-layout-state): persists per-signal UI layout so the
 * session can restore view_index / v_offset / own_height after reload. */
std::map<int, pv::data::ChannelLayoutState>
make_channel_layout(pv::view::View *view) {
  std::map<int, pv::data::ChannelLayoutState> layout;
  if (view) {
    for (auto *sig : view->get_own_signals()) {
      pv::data::ChannelLayoutState s;
      s.view_index = sig->get_view_index();
      s.v_offset = sig->get_v_offset();
      s.own_height = sig->get_own_height();
      layout[sig->get_index()] = s;
    }
  }
  return layout;
}

// Phase 2: Renamed to avoid collision with MainWindow::build_channel_layout
// The anonymous-namespace version is now called by the public wrapper.

/** Build a channel-index → colour-string map from the View's signal list.
 * Task 3 (purify-architecture-concepts): collects per-signal colour so
 * SignalConfigStore can serialize it as the single .pxc channel config path
 * (replaces the old MainWindow::gen_config_json direct view::Signal access).
 * Returns QColor::name() (hex "#RRGGBB") or "default" when invalid. */
std::map<int, std::string>
build_channel_colours(pv::view::View *view) {
  std::map<int, std::string> colours;
  if (view) {
    for (auto *sig : view->get_own_signals()) {
      QColor c = sig->get_colour();
      colours[sig->get_index()] = c.isValid() ? c.name().toStdString() : "default";
    }
  }
  return colours;
}
} // namespace

void MainWindow::MainWindowRibbonHelper() {
  _category_file_index = _title_bar->addCategory(
      L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FILE), "File"));
  _category_display_index = _title_bar->addCategory(
      L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY), "Settings"));
  _category_help_index = _title_bar->addCategory(
      L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_HELP), "Help"));
}

void MainWindow::Ribbon_setupUi() {
  setupFileCategory();
  setupDisplayCategory();
  setupHelpCategory();
}
// void MainWindow::setupQuickAccessBar()
// {

// }

void MainWindow::setupFileCategory() {
  _title_bar->addAction(_category_file_index, _file_bar->_action_load);
  _title_bar->addAction(_category_file_index, _file_bar->_action_store);
  _title_bar->addAction(_category_file_index, _file_bar->_action_default);

  _title_bar->addSeparator(_category_file_index);

  _title_bar->addAction(_category_file_index, _file_bar->_action_open);
  _title_bar->addAction(_category_file_index, _file_bar->_action_save);
  _title_bar->addSeparator(_category_file_index);

  _title_bar->addAction(_category_file_index, _file_bar->_action_export);
  _title_bar->addAction(_category_file_index, _file_bar->_action_import);
  _title_bar->addAction(_category_file_index, _file_bar->_action_capture);
}

void MainWindow::setupDisplayCategory() {
  _title_bar->addAction(_category_display_index, _logo_bar->_action_cn);
  _title_bar->addAction(_category_display_index,
                        _logo_bar->_action_traditional);
  _title_bar->addAction(_category_display_index, _logo_bar->_action_en);

  _title_bar->addSeparator(_category_display_index);

  _title_bar->addAction(_category_display_index,
                        _trig_bar->_action_dispalyOptions);
}

void MainWindow::setupHelpCategory() {
  _title_bar->addAction(_category_help_index, _logo_bar->_about);
  _title_bar->addAction(_category_help_index, _logo_bar->_manual);
  _title_bar->addAction(_category_help_index, _logo_bar->_issue);
  _title_bar->addAction(_category_help_index, _logo_bar->_update);
}

void MainWindow::Ribbon_retranslateUi() {
  if (_title_bar) {
    _title_bar->retranslateUi(
        _category_file_index,
        L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FILE), "File"));
    _title_bar->retranslateUi(
        _category_display_index,
        L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY), "Settings"));
    _title_bar->retranslateUi(
        _category_help_index,
        L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_HELP), "Help"));
  }
}

MainWindow::MainWindow(toolbars::TitleBar *title_bar, QWidget *parent)
    : QMainWindow(parent) {
  pxv_info("DBG MainWindow::MainWindow() START");
  _msg = nullptr;
  _frame = parent;
  _category_file_index = -1;
  _category_display_index = -1;
  _category_help_index = -1;

  if (!title_bar) {
    pxv_warn("%s", "MainWindow::MainWindow: title_bar is nullptr");
    throw std::invalid_argument("MainWindow: title_bar is nullptr");
  }
  if (!_frame) {
    pxv_warn("%s", "MainWindow::MainWindow: _frame is nullptr");
    throw std::invalid_argument("MainWindow: _frame is nullptr");
  }
  assert(title_bar);
  assert(_frame);

  _title_bar = title_bar;

  _session = ::AppControl::Instance()->GetSession();
  _session->add_callback(this);
  _device_agent = _session->get_device();
// Phase 2: initialise the config I/O delegate.
_config_io = std::make_unique<MainWindowConfigIO>(this);
_event_dispatcher = std::make_unique<SessionEventDispatcher>(this);
_tab_manager = std::make_unique<TabManager>(this);
_dock_manager = std::make_unique<DockManager>(this);
_theme_manager = std::make_unique<MainWindowThemeManager>(this);
_status_bar = std::make_unique<MainWindowStatusBar>(this);
_shortcut_manager = std::make_unique<MainWindowShortcutManager>(this);
  // Register as a typed event listener for all notification events.
  _session->add_event_listener(this);

  _is_auto_switch_device = false;
  _is_save_confirm_msg = false;
  _disk_cache_status_label = nullptr;
  _trig_time_label = nullptr;
  _sample_period_label = nullptr;

  _pattern_mode = "random";
  setup_ui();
  setMenuBar(nullptr);

  setContextMenuPolicy(Qt::NoContextMenu);

  _key_vaild = false;
  _last_key_press_time = high_resolution_clock::now();

  update_title_bar_text();

  // Register new-tab callback with AppService so MCP API can create tabs
  auto *app_svc = ::AppControl::Instance()->GetAppService();
  if (app_svc) {
    auto *concrete = dynamic_cast<pv::api::AppService *>(app_svc);
    if (concrete) {
      concrete->set_new_tab_callback([this]() { on_new_tab_requested(); });
    }
  }
}

MainWindow::~MainWindow() {
  // B1.2: unregister the typed event listener before destruction. The
  // SigSession outlives this MainWindow (it is owned by AppControl), so
  // failing to unregister would leave a dangling pointer in the listener
  // vector.
  if (_session) {
    _session->remove_event_listener(this);
  }
}

void MainWindow::setup_ui() {
  setObjectName(QString::fromUtf8("MainWindow"));
  setContentsMargins(0, 0, 0, 0);
  layout()->setSpacing(0);

  // Setup the central widget
  _central_widget = new QWidget(this);
  _vertical_layout = new QVBoxLayout(_central_widget);
  _vertical_layout->setSpacing(0);
  _vertical_layout->setContentsMargins(0, 0, 0, 0);
  setCentralWidget(_central_widget);

  // Setup the sampling bar
  _sampling_bar = new toolbars::SamplingBar(_session, this);
  _sampling_bar->setObjectName("sampling_bar");
  _trig_bar = new toolbars::TrigBar(_session, this);
  _trig_bar->setObjectName("trig_bar");
  _file_bar = new toolbars::FileBar(_session, this);
  _file_bar->setObjectName("file_bar");
  _logo_bar = new toolbars::LogoBar(_session, this);
  _logo_bar->setObjectName("logo_bar");

  _sampling_bar->setAllowedAreas(Qt::RightToolBarArea);
  _sampling_bar->hide();
  _trig_bar->setFloatable(false);
  _trig_bar->hide();
  _file_bar->setFloatable(false);
  _file_bar->hide();
  _logo_bar->setFloatable(false);
  _logo_bar->hide();

  _tab_manager->create_tab_widget(this, _vertical_layout);
  _tab_manager->init_initial_tab();

  // setIconSize(QSize(40, 40));
  // addToolBar(Qt::TopToolBarArea, _sampling_bar);  // moved into
  // device_options_dock addToolBar(_trig_bar); addToolBar(_file_bar);
  // addToolBar(_logo_bar);

  MainWindowRibbonHelper();
  Ribbon_setupUi();
  setIconSize(QSize(16, 16));
  // addToolBar(Qt::TopToolBarArea,_sampling_bar);
  // addToolBar(Qt::LeftToolBarArea,_trig_bar);
  // addToolBar(Qt::LeftToolBarArea,_file_bar);
  // addToolBar(Qt::LeftToolBarArea, _logo_bar);

  // Phase 2: dock creation, sliding drawer, sidebar, and connections
  // are all handled by the DockManager delegate.
  pv::view::View *initial_view = _tab_manager->current_view();
  _dock_manager->create_docks(initial_view);
  _dock_manager->setup_drawer(_central_widget, _vertical_layout);
  _dock_manager->setup_side_bar();
  _dock_manager->setup_connections();

  // event filter (non-dock widgets)
  initial_view->installEventFilter(this);
  _sampling_bar->installEventFilter(this);
  _trig_bar->installEventFilter(this);
  _file_bar->installEventFilter(this);
  _logo_bar->installEventFilter(this);
  _dock_manager->install_event_filters(this);

  // defaut language
  AppConfig &app = AppConfig::Instance();
  switchLanguage(app.frameOptions.language);
  switchTheme(app.frameOptions.style);

  _sampling_bar->set_view(initial_view);

  // event
  connect(&_event, &EventObject::session_error, this,
          &MainWindow::on_session_error);
  connect(&_event, &EventObject::signals_changed, this,
          &MainWindow::on_signals_changed);
  connect(&_event, &EventObject::signals_changed, _dock_manager->search_widget(),
          &dock::SearchDock::on_device_updated);
  connect(&_event, &EventObject::frame_ended, _dock_manager->search_widget(),
          &dock::SearchDock::on_frame_ended);
  connect(&_event, &EventObject::receive_trigger, this,
          &MainWindow::on_receive_trigger);
  connect(&_event, &EventObject::frame_ended, this, &MainWindow::on_frame_ended,
          Qt::QueuedConnection);
  connect(&_event, &EventObject::frame_began, this, &MainWindow::on_frame_began,
          Qt::QueuedConnection);
  connect(&_event, &EventObject::decode_done, this,
          &MainWindow::on_decode_done);
  // C5 fix: on_data_updated is the no-arg Qt slot connected to
  // EventObject::data_updated. Use QOverload<>::of to select it.
  connect(&_event, &EventObject::data_updated, this,
          QOverload<>::of(&MainWindow::on_data_updated));
  connect(&_event, &EventObject::cur_snap_samplerate_changed, this,
          &MainWindow::on_cur_snap_samplerate_changed);
  connect(&_event, &EventObject::receive_data_len, this,
          &MainWindow::on_receive_data_len);
  // Task 1.3: ICaptureCallback signals are emitted from Core capture thread;
  // route through Qt::QueuedConnection so the on_* slots touch View on GUI
  // thread.
  connect(&_event, &EventObject::update_capture_sig, this,
          &MainWindow::on_update_capture, Qt::QueuedConnection);
  connect(&_event, &EventObject::show_region_sig, this,
          &MainWindow::on_show_region, Qt::QueuedConnection);
  connect(&_event, &EventObject::show_wait_trigger_sig, this,
          &MainWindow::on_show_wait_trigger, Qt::QueuedConnection);
  connect(&_event, &EventObject::repeat_hold_sig, this,
          &MainWindow::on_repeat_hold, Qt::QueuedConnection);

  // view
  connect(initial_view, &view::View::prgRate, this, &MainWindow::prgRate);
  connect(initial_view, &view::View::auto_trig, _dock_manager->dso_trigger_widget(),
          &dock::DsoTriggerDock::auto_trig);

  // trig_bar
  connect(_trig_bar, &toolbars::TrigBar::sig_setTheme, this,
          &MainWindow::switchTheme);
  connect(_trig_bar, &toolbars::TrigBar::sig_show_lissajous, initial_view,
          &view::View::show_lissajous);

  // file toolbar
  connect(_file_bar, &toolbars::FileBar::sig_load_file, this,
          &MainWindow::on_load_file);
  connect(_file_bar, &toolbars::FileBar::sig_save, this, &MainWindow::on_save);
  connect(_file_bar, &toolbars::FileBar::sig_export, this,
          &MainWindow::on_export);
  connect(_file_bar, &toolbars::FileBar::sig_import_file, this,
          &MainWindow::on_import_file);
  connect(_file_bar, &toolbars::FileBar::sig_screenShot, this,
          &MainWindow::on_screenShot, Qt::QueuedConnection);
  connect(_file_bar, &toolbars::FileBar::sig_load_session, this,
          &MainWindow::on_load_session);
  connect(_file_bar, &toolbars::FileBar::sig_store_session, this,
          &MainWindow::on_store_session);

  // logobar
  connect(_logo_bar, &toolbars::LogoBar::sig_open_doc, this,
          &MainWindow::on_open_doc);

  connect(_dock_manager->protocol_widget(), &dock::ProtocolDock::protocol_updated, this,
          &MainWindow::on_signals_changed);

  // SamplingBar
  connect(_sampling_bar, &toolbars::SamplingBar::sig_store_session_data, this,
          &MainWindow::on_save);

  //
  connect(_dock_manager->dso_trigger_widget(), &dock::DsoTriggerDock::set_trig_pos,
          initial_view, &view::View::set_trig_pos);

  _delay_prop_msg_timer.SetCallback(
      std::bind(&MainWindow::on_delay_prop_msg, this));

  _logo_bar->set_mainform_callback(this);

  // Bind initial context to docks
  pv::TabContext *initial_ctx = _tab_manager->current_context();
  _sampling_bar->bind_context(initial_ctx);
  _dock_manager->bind_context(initial_ctx);

  _tab_manager->setup_connections();

  // Try load from file.
  QString ldFileName(::AppControl::Instance()->_open_file_name.c_str());
  if (ldFileName != "") {
    std::string file_name = pv::path::ToUnicodePath(ldFileName);

    if (QFile::exists(ldFileName)) {
      pxv_info("Auto load file:%s", file_name.c_str());
      tmp_file = ldFileName;
    } else {
      pxv_err("file is not exists:%s", file_name.c_str());
      MsgBox::Show(
          L_S(STR_PAGE_MSG, S_ID(IDS_MSG_OPEN_FILE_ERROR), "Open file error!"),
          ldFileName, nullptr);
    }
  }

  on_load_device_first();

  _disk_cache_status_label = new QLabel(this);
  statusBar()->addWidget(_disk_cache_status_label);
  _disk_cache_status_label->hide();

  _sample_period_label = new QLabel(this);
  _sample_period_label->setText("采样周期: --");
  statusBar()->addPermanentWidget(_sample_period_label);
  _sample_period_label->show();

  _trig_time_label = new QLabel(this);
  statusBar()->addPermanentWidget(_trig_time_label);
  _trig_time_label->hide();

  _fps_label = new QLabel(this);
  _fps_label->setText("UI: --ms | Dock: --ms");
  statusBar()->addPermanentWidget(_fps_label);
  _fps_label->show();

  _acq_count = 0;
  _status_bar->init(_disk_cache_status_label, _trig_time_label,
                    _sample_period_label, _fps_label);
  connect(&_fps_timer, &QTimer::timeout, this, [this]() { _status_bar->update_fps(); });
  _fps_timer.start(1000);

  connect(&_disk_cache_status_timer, &QTimer::timeout, this,
          [this]() { _status_bar->update_disk_cache_status(); });
  _disk_cache_status_timer.start(500);

  if (!_tab_manager->contexts().isEmpty()) {
    _tab_manager->contexts()[0]->activate();
  }
}

void MainWindow::on_load_device_first() {
  if (tmp_file != "") {
    on_load_file(tmp_file);
    tmp_file = "";
  } else {
    _session->set_default_device();
  }
}

void MainWindow::retranslateUi() {
  _dock_manager->retranslateUi();

  Ribbon_retranslateUi();
}

void MainWindow::on_load_file(QString file_name) {
  pv::view::View *new_view = new pv::view::View(_session, _sampling_bar, this);
  // phase 2: document owned by DocumentRegistry.
  size_t new_doc_idx = _session->document_registry()->take_document(
      std::make_unique<pv::data::SessionDocument>(_session));
  pv::data::SessionDocument *new_doc =
      _session->document_registry()->get_document_by_index(new_doc_idx);
  pv::TabContext *ctx =
      SessionManager::instance()->create_context(new_view, _session, new_doc,
                                                 new_doc_idx,
                                                 _session->document_registry());

  QFileInfo fi(file_name);
  ctx->set_title(fi.baseName());
  ctx->set_file_path(file_name);

  add_tab(ctx);

  try {
    if (_device_agent->is_hardware()) {
      save_config();
    }

    // 架构修复：检查 set_file 返回值，失败时不创建空白 tab
    if (!_session->set_file(file_name)) {
      QString strMsg(
          L_S(STR_PAGE_MSG, S_ID(IDS_MSG_FAIL_TO_LOAD), "Failed to load "));
      strMsg += file_name;
      MsgBox::Show(strMsg);
      // 回滚已创建的 tab
      int idx = _tab_manager->contexts().indexOf(ctx);
      if (idx >= 0)
        remove_tab(idx);
      _session->set_default_device();
      return;
    }
    ctx->make_live();
    ctx->activate();
    update_tab_style(_tab_manager->contexts().indexOf(ctx));
  } catch (QString e) {
    QString strMsg(
        L_S(STR_PAGE_MSG, S_ID(IDS_MSG_FAIL_TO_LOAD), "Failed to load "));
    strMsg += file_name;
    MsgBox::Show(strMsg);
    _session->set_default_device();
  }
}

void MainWindow::on_import_file(QString file_name) {
  pv::view::View *new_view = new pv::view::View(_session, _sampling_bar, this);
  // phase 2: document owned by DocumentRegistry.
  size_t new_doc_idx = _session->document_registry()->take_document(
      std::make_unique<pv::data::SessionDocument>(_session));
  pv::data::SessionDocument *new_doc =
      _session->document_registry()->get_document_by_index(new_doc_idx);
  pv::TabContext *ctx =
      SessionManager::instance()->create_context(new_view, _session, new_doc,
                                                 new_doc_idx,
                                                 _session->document_registry());

  QFileInfo fi(file_name);
  ctx->set_title(fi.baseName());
  ctx->set_file_path(file_name);

  add_tab(ctx);

  try {
    // Import external data file using libsigrok input modules
    // (VCD, CSV, binary, Saleae, etc.) — aligned with PulseView.
    if (!_session->import_file(file_name)) {
      QString strMsg(
          L_S(STR_PAGE_MSG, S_ID(IDS_MSG_FAIL_TO_LOAD), "Failed to load "));
      strMsg += file_name;
      MsgBox::Show(strMsg);
      // 回滚已创建的 tab
      int idx = _tab_manager->contexts().indexOf(ctx);
      if (idx >= 0)
        remove_tab(idx);
      _session->set_default_device();
      return;
    }
    ctx->make_live();
    ctx->activate();
    update_tab_style(_tab_manager->contexts().indexOf(ctx));
  } catch (QString e) {
    QString strMsg(
        L_S(STR_PAGE_MSG, S_ID(IDS_MSG_FAIL_TO_LOAD), "Failed to load "));
    strMsg += file_name;
    MsgBox::Show(strMsg);
    _session->set_default_device();
  }
}

void MainWindow::session_error() { _event.session_error(); }

void MainWindow::session_save() { save_config(); }

void MainWindow::on_session_error() {
  QString title;
  QString details;
  QString ch_status = "";

  switch (_session->get_error()) {
  case SigSession::Hw_err:
    pxv_info("MainWindow::on_session_error(),Hw_err, stop capture");
    _session->stop_capture();
    title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_HARDWARE_ERROR),
                "Hardware Operation Failed");
    details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_HARDWARE_ERROR_DET),
                  "Please replug device to refresh hardware configuration!");
    break;
  case SigSession::Malloc_err:
    pxv_info("MainWindow::on_session_error(),Malloc_err, stop capture");
    _session->stop_capture();
    title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_MALLOC_ERROR), "Malloc Error");
    details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_MALLOC_ERROR_DET),
                  "Memory is not enough for this sample!\nPlease reduce the "
                  "sample depth!");
    break;
  case SigSession::Pkt_data_err:
    title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PACKET_ERROR), "Packet Error");
    details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PACKET_ERROR_DET),
                  "the content of received packet are not expected!");
    _session->refresh(0);
    break;
  case SigSession::Data_overflow:
    pxv_info("MainWindow::on_session_error(),Data_overflow, stop capture");
    _session->stop_capture();
    title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DATA_OVERFLOW), "Data Overflow");
    details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DATA_OVERFLOW_DET),
                  "USB bandwidth can not support current sample rate! \nPlease "
                  "reduce the sample rate!");
    break;
  default:
    title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_UNDEFINED_ERROR), "Undefined Error");
    details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_UNDEFINED_ERROR_DET),
                  "Not expected error!");
    break;
  }

  pv::dialogs::DSMessageBox msg(this, title);
  msg.mBox()->setText(details);
  msg.mBox()->setStandardButtons(QMessageBox::Ok);
  msg.mBox()->setIcon(QMessageBox::Warning);
  connect(_session->device_event_object(), &DeviceEventObject::device_updated,
          &msg, &QDialog::accept);
  _msg = &msg;
  msg.exec();
  _msg = nullptr;

  _session->clear_error();
}

void MainWindow::save_config() { _config_io->save_config(); }

QString MainWindow::gen_config_file_path(bool isNewFormat) { return _config_io->gen_config_file_path(isNewFormat); }

bool MainWindow::able_to_close() {
  // Only commit UI settings to device when the device has no prior capture
  // data. If the device has data, the settings were already committed during
  // capture setup. Calling commit_settings() unconditionally would overwrite
  // device values (e.g., sample limit loaded from .pxc) with UI dropdown
  // values, which may not have the exact same option (e.g., 200M vs 1G).
  if (_device_agent->is_hardware() && _session->have_hardware_data() == false) {
    _sampling_bar->commit_settings();
  }

  _tab_manager->close_detached_windows();

  save_config();

  // Check if the user has disabled the save prompt on exit
  if (!AppConfig::Instance().appOptions.promptSaveOnExit) {
    return true;
  }

  if (confirm_to_store_data()) {
    on_save();
    return false;
  }
  return true;
}

void MainWindow::on_side_bar_dock_clicked(int index) { _dock_manager->on_side_bar_dock_clicked(index); }

void MainWindow::on_side_bar_action_clicked(int index) { _dock_manager->on_side_bar_action_clicked(index); }

void MainWindow::on_screenShot() {
  AppConfig &app = AppConfig::Instance();
  QString default_name =
      app.userHistory.screenShotPath + "/" + APP_NAME +
      QDateTime::currentDateTime().toString("-yyMMdd-hhmmss");

  int x = parentWidget()->pos().x();
  int y = parentWidget()->pos().y();
  int w = parentWidget()->frameGeometry().width();
  int h = parentWidget()->frameGeometry().height();

  (void)h;
  (void)w;
  (void)x;
  (void)y;

#ifdef _WIN32
  QPixmap pixmap = parentWidget()->grab();
#elif __APPLE__
  x += MainFrame::Margin;
  y += MainFrame::Margin;
  w -= MainFrame::Margin * 2;
  h -= MainFrame::Margin * 2;

  QPixmap pixmap =
      QGuiApplication::primaryScreen()->grabWindow(winId(), x, y, w, h);
#else
  QPixmap pixmap = parentWidget()->grab();
#endif

  QString format = "png";
  QString fileName = QFileDialog::getSaveFileName(
      this, L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVE_AS), "Save As"), default_name,
      "png file(*.png);;jpeg file(*.jpeg)", &format);

  if (!fileName.isEmpty()) {
    QStringList list = format.split('.').last().split(')');
    QString suffix = list.first();

    QFileInfo f(fileName);
    if (f.suffix().compare(suffix)) {
      // tr
      fileName += "." + suffix;
    }

    pixmap.save(fileName, suffix.toLatin1());

    fileName = path::GetDirectoryName(fileName);

    if (app.userHistory.screenShotPath != fileName) {
      app.userHistory.screenShotPath = fileName;
      app.SaveHistory();
    }
  }
}

// save file
void MainWindow::on_save() {
  using pv::dialogs::StoreProgress;

  if (_device_agent->have_instance() == false) {
    pxv_info("Have no device, can't to save data.");
    return;
  }

  if (_session->is_working()) {
    pxv_info("Save data: stop the current device.");
    _session->stop_capture();
  }

  _session->set_saving(true);

  StoreProgress *dlg = new StoreProgress(_session, this);
  dlg->SetView(current_view());
  dlg->save_run(this);
}

void MainWindow::on_export() {
  using pv::dialogs::StoreProgress;

  if (_session->is_working()) {
    pxv_info("Export data: stop the current device.");
    _session->stop_capture();
  }

  StoreProgress *dlg = new StoreProgress(_session, this);
  dlg->SetView(current_view());
  dlg->export_run();
}

bool MainWindow::on_load_session(QString name) {
  return load_config_from_file(name);
}

bool MainWindow::load_config_from_file(QString file) { return _config_io->load_config_from_file(file); }

bool MainWindow::gen_config_json(QJsonObject &sessionVar) { return _config_io->gen_config_json(sessionVar); }

bool MainWindow::load_config_from_json(QJsonDocument &doc, bool &haveDecoder) { return _config_io->load_config_from_json(doc, haveDecoder); }

bool MainWindow::on_store_session(QString name) {
  return save_config_to_file(name);
}

bool MainWindow::save_config_to_file(QString name) { return _config_io->save_config_to_file(name); }

bool MainWindow::genSessionData(std::string &str) { return _config_io->genSessionData(str); }

::DockOptions *MainWindow::getDockOptions() { return _dock_manager->getDockOptions(); }
void MainWindow::restore_dock() { _dock_manager->restore_dock(); }

int MainWindow::resolveShortcutAction(int key, int modifiers) {
  return _shortcut_manager->resolveShortcutAction(key, modifiers);
}

bool MainWindow::eventFilter(QObject *object, QEvent *event) {
  if (event->type() == QEvent::KeyPress)
    return _shortcut_manager->handleKeyPress(object, event);
  return false;
}

void MainWindow::switchLanguage(int language) {
  if (language == 0)
    return;

  AppConfig &app = AppConfig::Instance();

  if (app.frameOptions.language != language && language > 0) {
    app.frameOptions.language = language;
    app.SaveFrame();
    LangResource::Instance()->Load(language);
  }

  if (language == LAN_CN) {
    (void)_qtTrans.load(":/qt_" + QString::number(language));
    qApp->installTranslator(&_qtTrans);
    (void)_myTrans.load(":/my_" + QString::number(language));
    qApp->installTranslator(&_myTrans);
  } else if (language == LAN_EN) {
    qApp->removeTranslator(&_qtTrans);
    qApp->removeTranslator(&_myTrans);
  }

  retranslateUi();

  UiManager::Instance()->Update(UI_UPDATE_ACTION_LANG);
  _session->update_lang_text();
}

void MainWindow::switchTheme(QString style) {
  _theme_manager->switchTheme(style);
}

void MainWindow::data_updated() {
  _event.data_updated(); // safe call
}

void MainWindow::on_data_updated() {
  _dock_manager->measure_widget()->reCalc();
  current_view()->data_updated();
}

void MainWindow::on_open_doc() { openDoc(); }

void MainWindow::openDoc() {
  QDir dir(GetAppDataDir());
  AppConfig &app = AppConfig::Instance();
  int lan = app.frameOptions.language;
  QDesktopServices::openUrl(QUrl("file:///" + dir.absolutePath() + "/ug" +
                                 QString::number(lan) + ".pdf"));
}

void MainWindow::update_capture() { _event.update_capture_sig(); }

void MainWindow::on_update_capture() { current_view()->update_hori_res(); }

void MainWindow::cur_snap_samplerate_changed() {
  _event.cur_snap_samplerate_changed(); // safe call
}

void MainWindow::on_cur_snap_samplerate_changed() {
  _dock_manager->measure_widget()->reCalc();
  update_sample_period();
}

/*------------------on event end-------*/

void MainWindow::signals_changed() {
  _event.signals_changed(); // safe call
}

void MainWindow::on_signals_changed() {
  // Rebuild View signals from current SignalModels
  // (SignalFactory::update_signals with AllReplaced preserves UI state), then
  // refresh layout. This ensures LogicSignals pick up new SignalModel pointers
  // and Qt signal/slot connections are re-established after
  // init_signals()/reload() recreates models.
  current_view()->on_signals_changed();
}

void MainWindow::receive_trigger(quint64 trigger_pos) {
  _event.receive_trigger(trigger_pos); // save call
}

void MainWindow::on_receive_trigger(quint64 trigger_pos) {
  current_view()->receive_trigger(trigger_pos);
}

void MainWindow::frame_ended() {
  _event.frame_ended(); // save call
}

void MainWindow::on_frame_ended() {
  pxv_info("MainWindow::on_frame_ended() [UI-only: Core handles copy+decode+guard]");
  _acq_count++;
  _dock_manager->side_bar()->setItemRunning(SIDEBAR_RUNSTOP, false);
  _dock_manager->side_bar()->setItemRunning(SIDEBAR_INSTANT, false);

  // CRITICAL FIX (fork 迁移遗漏): 采集结束时更新所有 UI 组件的 enabled
  // 状态。is_working() 此时已为 false（action_stop_capture 或 SR_DF_END 路径
  // 设置），update_capture_ui_status() 会据此启用 toolbar/sidebar 按钮、
  // protocol dock 和 device options dock。
  //
  // 之前的问题：single 模式手动停止时，EndCollectWork 不被广播（只在 repeat
  // 模式广播，见 capturemanager.cpp:496-498），而 on_event(EndCollectWorkPrev)
  // 在 GUI 模式下是空操作。所以 UI 状态永远不会被更新，按钮保持禁用状态。
  //
  // 使用统一的 update_capture_ui_status() 而非单独调用各个 update 方法，
  // 确保所有采集状态相关的 UI 组件同步更新，避免遗漏。
  update_capture_ui_status();

  pv::TabContext *ctx = current_context();
  if (ctx && ctx->document()) {
    // CRITICAL FIX: copy_data_to_document + start_all_decode_tasks are now
    // handled exclusively by Core layer:
    //   LOGIC mode: SigSession::on_event(RevEndPacket) → bg copy thread →
    //               CopyToDocDone handler (or ELSE branch: direct decode +
    //               guard release for stream mode).
    //   non-LOGIC mode: DataFeedParser SR_DF_END else branch.
    // MainWindow previously did a DUPLICATE synchronous copy here, which raced
    // with the background copy thread and never released the CaptureOwnerGuard,
    // causing wait_capture_complete to time out forever.
    ctx->document()->save_signal_config(
        _session->get_signal_models(), make_channel_layout(current_view()));
  }
  current_view()->receive_end();
}

void MainWindow::frame_began() {
  _event.frame_began(); // save call
}

void MainWindow::on_frame_began() {
  if (_session->is_instant()) {
    _dock_manager->side_bar()->setItemRunning(SIDEBAR_INSTANT, true);
  } else {
    _dock_manager->side_bar()->setItemRunning(SIDEBAR_RUNSTOP, true);
  }
  pv::TabContext *ctx = current_context();
  if (ctx) {
    ctx->make_live();
    if (ctx->document()) {
      ctx->document()->clear();
      // Task 11.3 (R6 对称): is_working 时跳过 set_active_document，
      // 避免覆盖 capture owner——后台采集进行中切换 active 会造成数据归属错乱。
      // END_COLLECT_WORK 时会显式恢复当前 tab 的 active_document 归属。
      if (!_session->is_working()) {
        _session->set_active_document(ctx->document());
      }
    }
    current_view()->set_signal_data_from_source(_session);
  }
  current_view()->frame_began();
}

void MainWindow::show_region(uint64_t start, uint64_t end, bool keep) {
  _event.show_region_sig((quint64)start, (quint64)end, keep);
}

void MainWindow::on_show_region(quint64 start, quint64 end, bool keep) {
  current_view()->show_region((uint64_t)start, (uint64_t)end, keep);
}

void MainWindow::show_wait_trigger() { _event.show_wait_trigger_sig(); }

void MainWindow::on_show_wait_trigger() { current_view()->show_wait_trigger(); }

void MainWindow::repeat_hold(int percent) { _event.repeat_hold_sig(percent); }

void MainWindow::on_repeat_hold(int percent) {
  (void)percent;
  current_view()->repeat_show();
}

void MainWindow::decode_done() {
  _event.decode_done(); // safe call
}

void MainWindow::on_decode_done() { _dock_manager->protocol_widget()->update_model(); }

void MainWindow::receive_data_len(quint64 len) {
  _event.receive_data_len(len); // safe call
}

void MainWindow::on_receive_data_len(quint64 len) {
  current_view()->set_receive_len(len);
}

void MainWindow::receive_header() {}

void MainWindow::check_usb_device_speed() {
  // USB device speed check
  if (_device_agent->is_hardware()) {
    // SR_CONF_USB_SPEED/USB30_SUPPORT fork keys were deleted from pxlogic.c.
    // The link speed is now read directly from libusb via the typed wrapper
    // DeviceAgent::get_usb_speed() (calls sr_dev_inst_usb_speed_get).
    int usb_speed = _device_agent->get_usb_speed();
    if (usb_speed == LIBUSB_SPEED_UNKNOWN) {
      // Non-USB or speed undeterminable — nothing to check.
      return;
    }

    // is_usb30() returns true only for SUPER/SUPER_PLUS. For UNKNOWN we
    // conservatively treat as USB 2.0 (no warning shown).
    bool usb30_support = _device_agent->is_usb30();
    pxv_info("The device's USB module version: %d.0", usb30_support ? 3 : 2);

    int cable_ver = 1;
    if (usb_speed == LIBUSB_SPEED_HIGH)
      cable_ver = 2;
    else if (usb_speed == LIBUSB_SPEED_SUPER)
      cable_ver = 3;

    pxv_info("The cable's USB port version: %d.0", cable_ver);

    if (usb30_support && usb_speed == LIBUSB_SPEED_HIGH) {
      QString str_err(
          L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHECK_USB_SPEED_ERROR),
              "Plug the device into a USB 2.0 port will seriously affect its "
              "performance.\nPlease replug it into a USB 3.0 port."));
      delay_prop_msg(str_err);
    }
  }
}

void MainWindow::reset_all_view() {
  _sampling_bar->reload();
  current_view()->status_clear();
  current_view()->reload();
  current_view()->set_device();
  _dock_manager->trigger_widget()->update_view();
  _dock_manager->trigger_widget()->device_updated();
  _trig_bar->reload();
  _dock_manager->dso_trigger_widget()->update_view();
  _dock_manager->measure_widget()->reload();
  // DeviceOptionsDock refresh is handled by the caller:
  //   - DeviceModeChanged  → on_mode_changed() (lightweight, preserves scaffolding)
  //   - CurrentDeviceChanged → update_view() (full rebuild, called explicitly at line ~3160)
  // if (_sliding_drawer->isOpen())
  //   _sliding_drawer->close();
  // _side_bar->clearAllChecked();

  if (_device_agent->get_work_mode() == ANALOG)
    current_view()->get_viewstatus()->setVisible(false);
  else
    current_view()->get_viewstatus()->setVisible(true);
}

bool MainWindow::confirm_to_store_data() {
  bool ret = false;
  _is_save_confirm_msg = true;

  if (_session->have_hardware_data() && _session->is_first_store_confirm()) {
    // Only popup one time.
    ret = MsgBox::Confirm(
        L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SAVE_CAPDATE), "Save captured data?"));

    if (!ret && _is_auto_switch_device) {
      pxv_info("The data save confirm end, auto switch to the new device.");
      _is_auto_switch_device = false;

      if (_session->is_working())
        _session->stop_capture();

      _session->set_default_device();
    }
  }

  _is_save_confirm_msg = false;
  return ret;
}

void MainWindow::check_config_file_version() { _config_io->check_config_file_version(); }

void MainWindow::load_device_config() { _config_io->load_device_config(); }

QJsonDocument MainWindow::get_config_json_from_data_file(QString file,
                                                         bool &bSucesss) { return _config_io->get_config_json_from_data_file(file, bSucesss); }

QJsonArray MainWindow::get_decoder_json_from_data_file(QString file,
                                                       bool &bSucesss) { return _config_io->get_decoder_json_from_data_file(file, bSucesss); }

void MainWindow::update_capture_ui_status() {
  update_toolbar_view_status();
  _dock_manager->protocol_widget()->update_view_status();
  _dock_manager->device_options_widget()->update_widgets_status();
}

void MainWindow::update_toolbar_view_status() {
  _sampling_bar->update_view_status();
  _file_bar->update_view_status();
  _trig_bar->update_view_status();

  bool bEnable = _session->is_working() == false;
  int mode = _device_agent->get_work_mode();

  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_TRIGGER, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_DECODE, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_MEASURE, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_SEARCH, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_FUNCTION, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_OPTIONS, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_MCP, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_LOG, bEnable);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_RUNSTOP, true);
  _dock_manager->side_bar()->setItemEnabled(SIDEBAR_INSTANT, true);

  if (_session->is_working() && mode == DSO) {
    if (_session->is_instant() == false) {
      _dock_manager->side_bar()->setItemEnabled(SIDEBAR_TRIGGER, true);
      _dock_manager->side_bar()->setItemEnabled(SIDEBAR_MEASURE, true);
      _dock_manager->side_bar()->setItemEnabled(SIDEBAR_FUNCTION, true);
      _dock_manager->side_bar()->setItemEnabled(SIDEBAR_OPTIONS, true);
    }
  }

  if (mode == LOGIC) {
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_TRIGGER, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_DECODE, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_MEASURE, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_SEARCH, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_FUNCTION, false);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_OPTIONS, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_MCP, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_LOG, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_RUNSTOP, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_INSTANT, true);
  } else if (mode == ANALOG) {
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_TRIGGER, false);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_DECODE, false);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_MEASURE, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_SEARCH, false);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_FUNCTION, false);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_OPTIONS, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_MCP, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_LOG, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_RUNSTOP, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_INSTANT, false);
  } else if (mode == DSO) {
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_TRIGGER, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_DECODE, false);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_MEASURE, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_SEARCH, false);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_FUNCTION, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_OPTIONS, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_MCP, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_LOG, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_RUNSTOP, true);
    _dock_manager->side_bar()->setItemVisible(SIDEBAR_INSTANT, true);
  }

  /* If the currently-open drawer page belongs to a sidebar item that is
   * now invisible (e.g. switching DSO→ANALOG hides SIDEBAR_TRIGGER while
   * the DsoTriggerDock drawer is still open), close the drawer so the user
   * doesn't see stale content from the previous mode. Without this, the
   * drawer remains open but the sidebar button to close it is invisible. */
  if (_dock_manager->sliding_drawer() && _dock_manager->sliding_drawer()->isOpen()) {
    int cp = _dock_manager->drawer_current_page();
    bool should_close = false;
    if (cp == _dock_manager->drawer_page_trigger() || cp == _dock_manager->drawer_page_dso_trigger())
      should_close = !_dock_manager->side_bar()->isItemVisible(SIDEBAR_TRIGGER);
    else if (cp == _dock_manager->drawer_page_protocol())
      should_close = !_dock_manager->side_bar()->isItemVisible(SIDEBAR_DECODE);
    else if (cp == _dock_manager->drawer_page_search())
      should_close = !_dock_manager->side_bar()->isItemVisible(SIDEBAR_SEARCH);
    else if (cp == _dock_manager->drawer_page_function())
      should_close = !_dock_manager->side_bar()->isItemVisible(SIDEBAR_FUNCTION);
    if (should_close) {
      _dock_manager->sliding_drawer()->close();
      _dock_manager->side_bar()->clearAllChecked();
      _dock_manager->set_drawer_current_page(-1);
    }
  }
}

// ---------------------------------------------------------------------------
// IEventListener::on_event overrides (Task 12 — fully typed event dispatch).
//
// Each override corresponds to one of the 41 event structs in events.h and
// contains its handler body directly (no int dispatch, no switch). The former
// per-responsibility (int,int) helpers and the legacy IMessageListener /
// DSV_MSG_* / broadcast_msg / trigger_message infrastructure have been
// removed. broadcast<T>() / broadcast_sync<T>() / broadcast_async<T>() are
// the only dispatch paths: broadcast<T>() is synchronous and is invoked from
// within the async-dispatched handler, so these overrides already run on
// qApp's thread (main thread) — no GUI-thread marshal is needed.
//
// Empty-body overrides:
//   * CaptureOwnerChanged — uses ev.new_owner directly (no int param race).
//   * CopyToDocDone / DecodeDone / SignalsChanged / DataUpdated /
//     DeviceConfigUpdated — these events have no GUI work to do in
//     MainWindow.
// ---------------------------------------------------------------------------

// Phase 2: Public wrapper for SessionEventDispatcher
std::map<int, pv::data::ChannelLayoutState>
MainWindow::build_channel_layout(pv::view::View *view) {
  return make_channel_layout(view);
}

// ---------------------------------------------------------------------------
// IEventListener forwarding — Phase 2: all 45 on_event overrides delegate to
// _event_dispatcher (SessionEventDispatcher). The actual handler bodies live
// in mainwindow_event_dispatcher.cpp.
// ---------------------------------------------------------------------------

void MainWindow::on_event(const pv::interface::CaptureStateChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CaptureOwnerChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::TriggerConfigChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SampleCountUpdated &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DeviceOptionsUpdated &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DsoViewOptionChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::ActiveDocumentChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CopyToDocDone &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DecodeDone &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SignalsChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DataUpdated &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DeviceModeChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CollectModeChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DeviceListUpdated &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CurrentDeviceChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DeviceOpenFailed &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::UsbDeviceArrived &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DeviceDetached &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SampleRateChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SaveComplete &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::StartCollectWork &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CollectStart &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CollectEnd &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::EndCollectWork &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::EndDeviceOptions &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DeviceConfigUpdated &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DemoModeChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::DataPoolChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SimpleTriggerChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::GlitchFilterStarted &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::GlitchFilterProgress &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::GlitchFilterCompleted &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::GlitchFilterCleared &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SignalInvertStarted &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SignalInvertCompleted &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::SignalInvertCleared &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CopyInProgressChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::TrigNextCollect &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::ClearDecodeData &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::AppOptionsChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::FontOptionsChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::ShortcutChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::StyleChanged &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::StoreConfPrev &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::CurrentDeviceChangePrev &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::StartCollectWorkPrev &e) { _event_dispatcher->on_event(e); }
void MainWindow::on_event(const pv::interface::EndCollectWorkPrev &e) { _event_dispatcher->on_event(e); }

// ---------------------------------------------------------------------------
// IServiceEventListener — route View operation broadcasts from SessionService
// (MCP/WS API) to the active View. In Headless mode there is no MainWindow,
// so these events are simply not consumed.
// ---------------------------------------------------------------------------
void MainWindow::on_service_event(const pv::api::ServiceEventData &data) {
  pv::view::View *view = current_view();
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
    // signals_changed(nullptr) internally calls mark_derived_traces_dirty()
    // then get_traces() -> get_own_decode_traces() -> sync_derived_traces(),
    // which performs the Stack-pointer-identity-based reconciliation.
    // The explicit mark_derived_traces_dirty() is kept for clarity and
    // defensive purposes (idempotent).
    view->mark_derived_traces_dirty();
    view->signals_changed(nullptr);
    break;
  }
  default:
    // Not a View event; ignore.
    break;
  }
}

void MainWindow::calc_min_height() {
  if (_frame != nullptr) {
    if (_device_agent->get_work_mode() == LOGIC) {
      int ch_num = _session->get_ch_num(-1);
      int win_height = Base_Height + Per_Chan_Height * ch_num;

      if (win_height < Min_Height)
        _frame->setMinimumHeight(win_height);
      else
        _frame->setMinimumHeight(Min_Height);
    } else {
      _frame->setMinimumHeight(Min_Height);
    }
  }
}

void MainWindow::delay_prop_msg(QString strMsg) {
  _strMsg = strMsg;
  if (_strMsg != "") {
    _delay_prop_msg_timer.Start(500);
  }
}

void MainWindow::on_delay_prop_msg() {
  _delay_prop_msg_timer.Stop();

  if (_strMsg != "") {
    MsgBox::Show("", _strMsg, this, &_msg);
    _msg = nullptr;
  }
}

void MainWindow::update_title_bar_text() {
  // Set the title
  QString title = QApplication::applicationName() + " v" +
                  QApplication::applicationVersion();
  AppConfig &app = AppConfig::Instance();

  if (_title_ext_string != "" && app.appOptions.displayProfileInBar) {
    title += " [" + _title_ext_string + "]";
  }

  if (_lst_title_string != title) {
    _lst_title_string = title;

    setWindowTitle(
        QApplication::translate("MainWindow", title.toLocal8Bit().data(), 0));
    _title_bar->setTitle(this->windowTitle());
  }
}

void MainWindow::load_demo_decoder_config(QString optname) { _config_io->load_demo_decoder_config(optname); }

QWidget *MainWindow::GetBodyView() { return current_view(); }

pv::view::View *MainWindow::current_view() { return _tab_manager->current_view(); }
pv::TabContext *MainWindow::current_context() { return _tab_manager->current_context(); }
void MainWindow::add_tab(pv::TabContext *ctx) { _tab_manager->add_tab(ctx); }
void MainWindow::remove_tab(int index) { _tab_manager->remove_tab(index); }
void MainWindow::update_tab_style(int index) { _tab_manager->update_tab_style(index); }
void MainWindow::on_tab_changed(int index) { _tab_manager->on_tab_changed(index); }
void MainWindow::on_tab_moved(int from, int to) { _tab_manager->on_tab_moved(from, to); }
void MainWindow::on_tab_detach(int index, QWidget *widget, const QString &title) { _tab_manager->on_tab_detach(index, widget, title); }
void MainWindow::on_tab_attached(QWidget *widget, const QString &title) { _tab_manager->on_tab_attached(widget, title); }
void MainWindow::on_new_tab_requested() { _tab_manager->on_new_tab_requested(); }
void MainWindow::update_disk_cache_status() {
  _status_bar->update_disk_cache_status();
}

void MainWindow::update_fps() {
  _status_bar->update_fps();
}

void MainWindow::update_sample_period() {
  _status_bar->update_sample_period();
}

} // namespace pv
