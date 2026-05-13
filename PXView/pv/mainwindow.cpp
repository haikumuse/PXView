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

#include <QAction>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QStatusBar>
#include <QComboBox>
#include <QAbstractButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QFrame>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QEvent>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QtGlobal>
#include <QApplication>
#include <QStandardPaths>
#include <QScreen>
#include <QTimer>
#include <libusb-1.0/libusb.h>
#include <QGuiApplication>
#include <QTextStream>
#include <QRegularExpression>
#include <QHash>
#include <QList>
#include <algorithm>
#include <QJsonValue>
#include <QJsonArray>
#include <functional>
#include "widgets/searchpatterninput.h"
#include "widgets/sidebar.h"

//include with qt5
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QDesktopWidget>
#endif

#include "log.h"
#include "mainwindow.h"

#include "data/logicsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/analogsnapshot.h"

#include "dialogs/about.h"
#include "dialogs/deviceoptions.h"
#include "dialogs/storeprogress.h"
#include "dialogs/waitingdialog.h"
#include "dialogs/regionoptions.h"

#include "toolbars/samplingbar.h"
#include "toolbars/trigbar.h"
#include "toolbars/filebar.h"
#include "toolbars/logobar.h"
#include "toolbars/titlebar.h"

#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/searchdock.h"
#include "dock/protocoldock.h"
#include "dock/deviceoptionsdock.h"

#include "view/view.h"
#include "view/trace.h"
#include "view/signal.h"
#include "view/dsosignal.h"
#include "view/logicsignal.h"
#include "view/analogsignal.h"
#include "data/sessiondocument.h"
#include "sessionmanager.h"
#include "interface/icontextaware.h"
#include "ui/draggabletabwidget.h"
#include "tabcontext.h"

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <list>
#include "ui/msgbox.h"
#include "config/appconfig.h"
#include "appcontrol.h"
#include "dsvdef.h"
#include "appcontrol.h"
#include "utility/encoding.h"
#include "utility/path.h"
#include "log.h"
#include "sigsession.h"
#include "deviceagent.h"
#include <stdlib.h>
#include "ZipMaker.h"
#include "ui/langresource.h"
#include "mainframe.h"
#include "dsvdef.h"
#include <thread>
#include "ui/uimanager.h"
#ifdef ENABLE_DEBUG_HELPER
#include "ui/debughelper.h"
#endif

#include <QWidgetAction>
#include<QShortcut>

#include <QTabBar>
#include <QScrollArea>

namespace pv
{

    namespace{
        QString tmp_file;
    }


    void MainWindow::MainWindowRibbonHelper()
    {
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

    void MainWindow::setupSideBar()
    {
        _side_bar = new widgets::SideBar(this);

        _side_bar->addItem("trigger.svg", S_ID(IDS_TOOLBAR_TRIGGER), "Trigger",
                            widgets::SideBar::DockItem, _drawer_page_trigger);
        _side_bar->addItem("protocol.svg", S_ID(IDS_TOOLBAR_DECODE), "Decode",
                            widgets::SideBar::DockItem, _drawer_page_protocol);
        _side_bar->addItem("measure.svg", S_ID(IDS_TOOLBAR_MEASURE), "Measure",
                            widgets::SideBar::DockItem, _drawer_page_measure);
        _side_bar->addItem("search-bar.svg", S_ID(IDS_TOOLBAR_SEARCH), "Search",
                            widgets::SideBar::DockItem, _drawer_page_search);
        _side_bar->addItem("function.svg", S_ID(IDS_TOOLBAR_FUNCTION), "Function",
                            widgets::SideBar::DockItem);
        _side_bar->addItem("params.svg", S_ID(IDS_TOOLBAR_DEVICE_OPTION), "Options",
                            widgets::SideBar::DockItem, _drawer_page_device_options);
        _side_bar->addSeparator();
        _side_bar->addItem("start.svg", S_ID(IDS_TOOLBAR_RUN_START), "Start",
                            widgets::SideBar::ActionItem);
        _side_bar->addItem("single.svg", S_ID(IDS_TOOLBAR_ONE_INSTANT), "Instant",
                            widgets::SideBar::ActionItem);

        addToolBar(Qt::RightToolBarArea, _side_bar);

        connect(_side_bar, &widgets::SideBar::dockItemClicked,
                this, &MainWindow::on_side_bar_dock_clicked);
        connect(_side_bar, &widgets::SideBar::actionItemClicked,
                this, &MainWindow::on_side_bar_action_clicked);
    }

    void MainWindow::setupFileCategory()
    {
        _title_bar->addAction(_category_file_index, _file_bar->_action_load);
        _title_bar->addAction(_category_file_index, _file_bar->_action_store);
        _title_bar->addAction(_category_file_index, _file_bar->_action_default);

        _title_bar->addSeparator(_category_file_index);

        _title_bar->addAction(_category_file_index, _file_bar->_action_open);
        _title_bar->addAction(_category_file_index, _file_bar->_action_save);
        _title_bar->addSeparator(_category_file_index);

        _title_bar->addAction(_category_file_index, _file_bar->_action_export);
        _title_bar->addAction(_category_file_index, _file_bar->_action_capture);
    }

    void MainWindow::setupDisplayCategory()
    {
        _title_bar->addAction(_category_display_index, _logo_bar->_action_cn);
        _title_bar->addAction(_category_display_index, _logo_bar->_action_en);

        _title_bar->addSeparator(_category_display_index);

        _title_bar->addAction(_category_display_index, _trig_bar->_light_style);
        _title_bar->addAction(_category_display_index, _trig_bar->_dark_style);
        _title_bar->addSeparator(_category_display_index);

        _title_bar->addAction(_category_display_index, _trig_bar->_action_dispalyOptions);
    }

    void MainWindow::setupHelpCategory()
    {
        _title_bar->addAction(_category_help_index, _logo_bar->_about);
        _title_bar->addAction(_category_help_index, _logo_bar->_manual);
        _title_bar->addAction(_category_help_index, _logo_bar->_issue);
        _title_bar->addAction(_category_help_index, _logo_bar->_update);
        _title_bar->addAction(_category_help_index, _logo_bar->_log);
    }

    void MainWindow::Ribbon_retranslateUi()
    {
        if (_title_bar) {
            _title_bar->retranslateUi(_category_file_index,
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FILE), "File"));
            _title_bar->retranslateUi(_category_display_index,
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_DISPLAY), "Settings"));
            _title_bar->retranslateUi(_category_help_index,
                L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_HELP), "Help"));
        }
    }

    MainWindow::MainWindow(toolbars::TitleBar *title_bar, QWidget *parent)
        : QMainWindow(parent)
    {
        dsv_info("DBG MainWindow::MainWindow() START");
        _msg = NULL;
        _frame = parent;
        _category_file_index = -1;
        _category_display_index = -1;
        _category_help_index = -1;

        assert(title_bar);
        assert(_frame);

        _title_bar = title_bar;

        _session = AppControl::Instance()->GetSession();
        _session->set_callback(this);
        _device_agent = _session->get_device();
        _session->add_msg_listener(this);

        _is_auto_switch_device = false;
        _is_save_confirm_msg = false;

        _pattern_mode = "random";
        setup_ui();
        setMenuBar(nullptr);

        setContextMenuPolicy(Qt::NoContextMenu);

        _key_vaild = false;
        _last_key_press_time = high_resolution_clock::now();

        update_title_bar_text();
#ifdef ENABLE_DEBUG_HELPER
        _debug_helper = pv::ui::DebugHelper::Instance();
        _debug_helper->setEnabled(true);
        DEBUG_INSTALL_TREE(this);
#endif
    }

    MainWindow::~MainWindow()
    {
#ifdef ENABLE_DEBUG_HELPER
        if (_debug_helper) {
            _debug_helper->setEnabled(false);
            _debug_helper = nullptr;
        }
#endif
    }

    void MainWindow::setup_ui()
    {
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


        // trigger dock
        _trigger_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."), this);
        _trigger_dock->setObjectName("trigger_dock");
        _trigger_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _trigger_dock->setVisible(false);
        _trigger_widget = new dock::TriggerDock(_trigger_dock, _session);        
        _trigger_dock->setWidget(_trigger_widget);

        _dso_trigger_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."), this);
        _dso_trigger_dock->setObjectName("dso_trigger_dock");
        _dso_trigger_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _dso_trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _dso_trigger_dock->setVisible(false);
        _dso_trigger_widget = new dock::DsoTriggerDock(_dso_trigger_dock, _session);
        _dso_trigger_dock->setWidget(_dso_trigger_widget);

        _tab_widget = new pv::ui::DraggableTabWidget(this);
        _vertical_layout->addWidget(_tab_widget);

        pv::view::View *initial_view = new pv::view::View(_session, _sampling_bar, this);
        pv::data::SessionDocument *initial_doc = new pv::data::SessionDocument();

        if (_device_agent && _device_agent->have_instance()) {
            initial_doc->save_signal_config(_device_agent);
            dsv_info("MainWindow::setup_ui() saved initial signal config, mode=%d ch_count=%d",
                initial_doc->get_signal_config().work_mode,
                (int)initial_doc->get_signal_config().channels.size());
        }

        pv::TabContext *initial_ctx = SessionManager::instance()->create_context(initial_view, _session, initial_doc);
        _session->register_document(initial_doc);
        initial_ctx->set_title(L_S(STR_PAGE_TOOLBAR, S_ID(IDS_TOOLBAR_FILE), "File"));
        _tab_contexts.append(initial_ctx);
        qDebug() << "MainWindow::setup_ui() before addTab, initial_doc=" << initial_doc << "has_config=" << initial_doc->has_signal_config();
        dsv_info("DBG before addTab has_config=%d", initial_doc->has_signal_config());
        _tab_widget->addTab(initial_view, initial_ctx->title());
        dsv_info("DBG after addTab");
        fprintf(stderr, "DBG MainWindow::setup_ui() after addTab\n"); fflush(stderr);
        _current_tab_index = 0;

        initial_ctx->activate();


        // setIconSize(QSize(40, 40));
        // addToolBar(Qt::TopToolBarArea, _sampling_bar);  // moved into device_options_dock
        // addToolBar(_trig_bar);
        // addToolBar(_file_bar);
        // addToolBar(_logo_bar);

        MainWindowRibbonHelper();
        Ribbon_setupUi();
        setIconSize(QSize(16, 16));
        // addToolBar(Qt::TopToolBarArea,_sampling_bar);
        // addToolBar(Qt::LeftToolBarArea,_trig_bar);
        // addToolBar(Qt::LeftToolBarArea,_file_bar);
        addToolBar(Qt::LeftToolBarArea,_logo_bar);

        // Setup the dockWidget
        _protocol_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PROTOCOL_DOCK_TITLE), "Decode Protocol"), this);
        _protocol_dock->setObjectName("protocol_dock");
        _protocol_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _protocol_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _protocol_dock->setVisible(false);
        _protocol_widget = new dock::ProtocolDock(_protocol_dock, initial_view, _session);
        _protocol_dock->setWidget(_protocol_widget);

        _session->set_decoder_pannel(_protocol_widget);

        // measure dock
        _measure_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASURE_DOCK_TITLE), "Measurement"), this);
        _measure_dock->setObjectName("measure_dock");
        _measure_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _measure_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _measure_dock->setVisible(false);
        _measure_widget = new dock::MeasureDock(_measure_dock, initial_view, _session);
        _measure_dock->setWidget(_measure_widget);

        // search dock
        _search_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_DOCK_TITLE), "Search..."), this);
        _search_dock->setObjectName("search_dock");
        // _search_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        _search_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _search_dock->setTitleBarWidget(new QWidget(_search_dock));
        // _search_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
        _search_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _search_dock->setVisible(false);

        _search_widget = new dock::SearchDock(_search_dock, initial_view, _session);
        _search_dock->setWidget(_search_widget);

        _device_options_dock = new QDockWidget(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DEVICE_OPTIONS), "Device Options"), this);
        _device_options_dock->setObjectName("device_options_dock");
        _device_options_dock->setFeatures(QDockWidget::DockWidgetMovable);
        _device_options_dock->setAllowedAreas(Qt::RightDockWidgetArea);
        _device_options_dock->setVisible(false);
        _device_options_widget = new dock::DeviceOptionsDock(_device_options_dock, _session);

        QWidget *dock_container = new QWidget();
        QVBoxLayout *dock_lay = new QVBoxLayout(dock_container);
        dock_lay->setContentsMargins(0, 0, 0, 0);
        dock_lay->setSpacing(0);
        QWidget *sampling_widget = _sampling_bar->createSamplingSettingsWidget(dock_container);
        dock_lay->addWidget(sampling_widget);
        _device_options_widget->set_sampling_widget(sampling_widget);

        QFrame *sep = new QFrame(dock_container);
        sep->setObjectName("dock_section_separator");
        sep->setFrameShape(QFrame::HLine);
        dock_lay->addWidget(sep);

        dock_lay->addWidget(_device_options_widget);

        // Wrap the entire dock_container (sampling bar + device options) in a ScrollArea.
        // This is the ONLY page that needs an external scroll wrapper to prevent "crushing".
        QScrollArea *dock_scroll = new QScrollArea();
        dock_scroll->setWidgetResizable(true);
        dock_scroll->setFrameShape(QFrame::NoFrame);
        dock_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        
        // VITAL: Ensure the container doesn't shrink, triggering the scrollbar instead.
        dock_container->setMinimumHeight(1000); 
        dock_scroll->setWidget(dock_container);

        // Note: dock_scroll will be added to SlidingDrawer below
        connect(_device_options_widget, &dock::DeviceOptionsDock::settings_applied, this, [this](){
            if (_session->have_view_data() == false)
                _sampling_bar->commit_settings();
            _sampling_bar->update_sample_rate_list();
            _sampling_bar->reload();
        });

        // Do NOT add dock widgets to the main window layout.
        // They are hidden containers; content is shown via SlidingDrawer instead.
        _protocol_dock->setVisible(false);
        _trigger_dock->setVisible(false);
        _dso_trigger_dock->setVisible(false);
        _measure_dock->setVisible(false);
        _search_dock->setVisible(false);
        _device_options_dock->setVisible(false);

        // --- Create SlidingDrawer (overlay child of _central_widget, push via margin) ---
        _sliding_drawer = new widgets::SlidingDrawer(_central_widget);
        _sliding_drawer->setDrawerWidth(350);
        _sliding_drawer->setAnimationDuration(300);
        _sliding_drawer->setPushLayout(_vertical_layout);

        // Take content widgets out of QDockWidget and add to SlidingDrawer
        // Protocol
        _protocol_dock->setWidget(nullptr);
        _drawer_page_protocol = _sliding_drawer->addPage(
            _protocol_widget,
            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PROTOCOL_DOCK_TITLE), "Decode Protocol"));

        // Trigger (logic analyzer)
        _trigger_dock->setWidget(nullptr);
        _drawer_page_trigger = _sliding_drawer->addPage(
            _trigger_widget,
            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));

        // DSO Trigger
        _dso_trigger_dock->setWidget(nullptr);
        _drawer_page_dso_trigger = _sliding_drawer->addPage(
            _dso_trigger_widget,
            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));

        // Measure
        _measure_dock->setWidget(nullptr);
        _drawer_page_measure = _sliding_drawer->addPage(
            _measure_widget,
            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASURE_DOCK_TITLE), "Measurement"));

        // Search
        _search_dock->setWidget(nullptr);
        _drawer_page_search = _sliding_drawer->addPage(
            _search_widget,
            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_DOCK_TITLE), "Search..."));

        // Device Options (includes sampling settings)
        _device_options_dock->setWidget(nullptr);
        _drawer_page_device_options = _sliding_drawer->addPage(
            dock_scroll,
            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DEVICE_OPTIONS), "Device Options"));

        _drawer_current_page = -1;

        setupSideBar();

        // When drawer closes, update toolbar state
        connect(_sliding_drawer, &widgets::SlidingDrawer::drawerClosed, this, [this]() {
            _drawer_current_page = -1;
            _side_bar->clearAllChecked();
            current_view()->show_search_cursor(false);
            ::DockOptions *opt = getDockOptions();
            if (opt) {
                opt->decodeDock = false;
                opt->triggerDock = false;
                opt->measureDock = false;
                opt->searchDock = false;
                opt->deviceOptionsDock = false;
                AppConfig::Instance().SaveFrame();
            }
            current_view()->setFocus();
        });

        connect(_sliding_drawer, &widgets::SlidingDrawer::drawerOpened, this, [this](int page) {
            QWidget *content = _sliding_drawer->page(page);
            if (content) {
                QWidget *focus_target = content;
                QScrollArea *scroll = qobject_cast<QScrollArea*>(content);
                if (scroll && scroll->widget()) {
                    focus_target = scroll->widget();
                }
                QWidget *first_focusable = nullptr;
                QWidget *prev = focus_target;
                while (prev) {
                    QWidget *next = prev->nextInFocusChain();
                    if (!next || next == focus_target)
                        break;
                    if (next->isVisible() && next->isEnabled() &&
                        next->focusPolicy() & Qt::TabFocus) {
                        if (_sliding_drawer->isAncestorOf(next)) {
                            first_focusable = next;
                            break;
                        }
                    }
                    prev = next;
                }
                if (first_focusable)
                    first_focusable->setFocus();
                else
                    content->setFocus();
            }
        });

        // event filter
        initial_view->installEventFilter(this);
        _sampling_bar->installEventFilter(this);
        _trig_bar->installEventFilter(this);
        _file_bar->installEventFilter(this);
        _logo_bar->installEventFilter(this);
        _dso_trigger_dock->installEventFilter(this);
        _trigger_dock->installEventFilter(this);
        _protocol_dock->installEventFilter(this);
        _measure_dock->installEventFilter(this);
        _search_dock->installEventFilter(this);
        _device_options_dock->installEventFilter(this);
        _sliding_drawer->installEventFilter(this);

        // defaut language
        AppConfig &app = AppConfig::Instance();
        switchLanguage(app.frameOptions.language);
        switchTheme(app.frameOptions.style);

        _sampling_bar->set_view(initial_view);

        // event
        connect(&_event, SIGNAL(session_error()), this, SLOT(on_session_error()));
        connect(&_event, SIGNAL(signals_changed()), this, SLOT(on_signals_changed()));
        connect(&_event, SIGNAL(signals_changed()), _search_widget, SLOT(on_device_updated()));
        connect(&_event, SIGNAL(frame_ended()), _search_widget, SLOT(on_frame_ended()));
        connect(&_event, SIGNAL(receive_trigger(quint64)), this, SLOT(on_receive_trigger(quint64)));
        connect(&_event, SIGNAL(frame_ended()), this, SLOT(on_frame_ended()), Qt::DirectConnection);
        connect(&_event, SIGNAL(frame_began()), this, SLOT(on_frame_began()), Qt::DirectConnection);
        connect(&_event, SIGNAL(decode_done()), this, SLOT(on_decode_done()));
        connect(&_event, SIGNAL(data_updated()), this, SLOT(on_data_updated()));
        connect(&_event, SIGNAL(cur_snap_samplerate_changed()), this, SLOT(on_cur_snap_samplerate_changed()));
        connect(&_event, SIGNAL(receive_data_len(quint64)), this, SLOT(on_receive_data_len(quint64)));
        connect(&_event, SIGNAL(trigger_message(int)), this, SLOT(on_trigger_message(int)));

        // view
        connect(initial_view, SIGNAL(prgRate(int)), this, SIGNAL(prgRate(int)));
        connect(initial_view, SIGNAL(auto_trig(int)), _dso_trigger_widget, SLOT(auto_trig(int)));

        // trig_bar
        connect(_trig_bar, SIGNAL(sig_setTheme(QString)), this, SLOT(switchTheme(QString)));
        connect(_trig_bar, SIGNAL(sig_show_lissajous(bool)), initial_view, SLOT(show_lissajous(bool)));

        // file toolbar
        connect(_file_bar, SIGNAL(sig_load_file(QString)), this, SLOT(on_load_file(QString)));
        connect(_file_bar, SIGNAL(sig_save()), this, SLOT(on_save()));
        connect(_file_bar, SIGNAL(sig_export()), this, SLOT(on_export()));
        connect(_file_bar, SIGNAL(sig_screenShot()), this, SLOT(on_screenShot()), Qt::QueuedConnection);
        connect(_file_bar, SIGNAL(sig_load_session(QString)), this, SLOT(on_load_session(QString)));
        connect(_file_bar, SIGNAL(sig_store_session(QString)), this, SLOT(on_store_session(QString)));

        // logobar
        connect(_logo_bar, SIGNAL(sig_open_doc()), this, SLOT(on_open_doc()));

        connect(_protocol_widget, SIGNAL(protocol_updated()), this, SLOT(on_signals_changed()));

        // SamplingBar
        connect(_sampling_bar, SIGNAL(sig_store_session_data()), this, SLOT(on_save()));

        //
        connect(_dso_trigger_widget, SIGNAL(set_trig_pos(int)), initial_view, SLOT(set_trig_pos(int)));

        _delay_prop_msg_timer.SetCallback(std::bind(&MainWindow::on_delay_prop_msg, this));
 
        _logo_bar->set_mainform_callback(this);

        // Bind initial context to docks
        _sampling_bar->bind_context(initial_ctx);
        _measure_widget->bind_context(initial_ctx);
        _search_widget->bind_context(initial_ctx);
        _protocol_widget->bind_context(initial_ctx);
        _device_options_widget->bind_context(initial_ctx);
        _trigger_widget->bind_context(initial_ctx);
        _dso_trigger_widget->bind_context(initial_ctx);

        connect(_tab_widget, &pv::ui::DraggableTabWidget::currentChanged, this, &MainWindow::on_tab_changed);
        connect(_tab_widget, &pv::ui::DraggableTabWidget::tabDetached, this, &MainWindow::on_tab_detach);
        connect(_tab_widget, &pv::ui::DraggableTabWidget::tabAttached, this, &MainWindow::on_tab_attached);
        connect(_tab_widget, &pv::ui::DraggableTabWidget::newTabRequested, this, &MainWindow::on_new_tab_requested);
        connect(_tab_widget, &pv::ui::DraggableTabWidget::tabCloseRequested, this, &MainWindow::remove_tab);
        connect(_tab_widget, &pv::ui::DraggableTabWidget::tabRenamed, this, [this](int index, const QString &title) {
            if (index >= 0 && index < _tab_contexts.size()) {
                _tab_contexts[index]->set_title(title);
                update_tab_style(index);
            }
        });
        connect(_tab_widget, &pv::ui::DraggableTabWidget::tabAttached, this, [this](QWidget *widget, const QString &title) {
            pv::view::View *view = qobject_cast<pv::view::View*>(widget);
            if (view) {
                pv::TabContext *existing_ctx = nullptr;
                for (auto c : _tab_contexts) {
                    if (c->view() == view) {
                        existing_ctx = c;
                        break;
                    }
                }
                if (!existing_ctx) {
                    QVariant var = view->property("detached_ctx");
                    if (var.isValid()) {
                        existing_ctx = (pv::TabContext*)(var.value<quintptr>());
                        if (existing_ctx) {
                            existing_ctx->set_title(title);
                            _tab_contexts.append(existing_ctx);
                            view->setProperty("detached_ctx", QVariant());
                        }
                    }
                    if (!existing_ctx) {
                        pv::data::SessionDocument *doc = new pv::data::SessionDocument();
                        pv::TabContext *ctx = SessionManager::instance()->create_context(view, _session, doc);
                        _session->register_document(doc);
                        ctx->set_title(title);
                        _tab_contexts.append(ctx);
                    }
                }
            }
        });

        // Try load from file.
        QString ldFileName(AppControl::Instance()->_open_file_name.c_str());
        if (ldFileName != "")
        {   
            std::string file_name = pv::path::ToUnicodePath(ldFileName);

            if (QFile::exists(ldFileName))
            {              
                dsv_info("Auto load file:%s", file_name.c_str());
                tmp_file = ldFileName;
            }
            else
            {
                dsv_err("file is not exists:%s", file_name.c_str());
                MsgBox::Show(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_OPEN_FILE_ERROR), "Open file error!"), ldFileName, NULL);
            }
        }

        on_load_device_first();

        if (!_tab_contexts.isEmpty()) {
            _tab_contexts[0]->activate();
        }
    }

    void MainWindow::on_load_device_first()
    {
        if (tmp_file != ""){
            on_load_file(tmp_file);
            tmp_file = "";
        }
        else{
            _session->set_default_device();
        }
    }

    void MainWindow::retranslateUi()
    {
        _trigger_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));
        _dso_trigger_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));
        _protocol_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PROTOCOL_DOCK_TITLE), "Decode Protocol"));
        _measure_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASURE_DOCK_TITLE), "Measurement"));
        _search_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_DOCK_TITLE), "Search..."));
        _device_options_dock->setWindowTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DEVICE_OPTIONS), "Device Options"));

        // Update drawer page titles
        if (_sliding_drawer) {
            _sliding_drawer->setPageTitle(_drawer_page_protocol,
                L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PROTOCOL_DOCK_TITLE), "Decode Protocol"));
            _sliding_drawer->setPageTitle(_drawer_page_trigger,
                L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));
            _sliding_drawer->setPageTitle(_drawer_page_dso_trigger,
                L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_DOCK_TITLE), "Trigger Setting..."));
            _sliding_drawer->setPageTitle(_drawer_page_measure,
                L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASURE_DOCK_TITLE), "Measurement"));
            _sliding_drawer->setPageTitle(_drawer_page_search,
                L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SEARCH_DOCK_TITLE), "Search..."));
            _sliding_drawer->setPageTitle(_drawer_page_device_options,
                L_S(STR_PAGE_DLG, S_ID(IDS_DLG_DEVICE_OPTIONS), "Device Options"));
        }

        Ribbon_retranslateUi();
    }

    void MainWindow::on_load_file(QString file_name)
    {
        pv::view::View *new_view = new pv::view::View(_session, _sampling_bar, this);
        pv::data::SessionDocument *new_doc = new pv::data::SessionDocument();
        pv::TabContext *ctx = SessionManager::instance()->create_context(new_view, _session, new_doc);
        _session->register_document(new_doc);

        QFileInfo fi(file_name);
        ctx->set_title(fi.baseName());
        ctx->set_file_path(file_name);

        add_tab(ctx);

        try
        {
            if (_device_agent->is_hardware()){
                save_config();
            }

            _session->set_file(file_name);
            ctx->make_live();
            ctx->activate();
            update_tab_style(_tab_contexts.indexOf(ctx));
        }
        catch (QString e)
        {   
            QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_FAIL_TO_LOAD), "Failed to load "));
            strMsg += file_name;
            MsgBox::Show(strMsg);
            _session->set_default_device();
        }
    }

    void MainWindow::session_error()
    {
        _event.session_error();
    }

    void MainWindow::session_save()
    {
        save_config();
    }

    void MainWindow::on_session_error()
    {
        QString title;
        QString details;
        QString ch_status = "";

        switch (_session->get_error())
        {
        case SigSession::Hw_err:
            dsv_info("MainWindow::on_session_error(),Hw_err, stop capture");
            _session->stop_capture();
            title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_HARDWARE_ERROR), "Hardware Operation Failed");
            details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_HARDWARE_ERROR_DET), 
                      "Please replug device to refresh hardware configuration!");
            break;
        case SigSession::Malloc_err:
            dsv_info("MainWindow::on_session_error(),Malloc_err, stop capture");
            _session->stop_capture();
            title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_MALLOC_ERROR), "Malloc Error");
            details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_MALLOC_ERROR_DET), 
                      "Memory is not enough for this sample!\nPlease reduce the sample depth!");
            break;
        case SigSession::Pkt_data_err:
            title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PACKET_ERROR), "Packet Error");
            details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PACKET_ERROR_DET), 
            "the content of received packet are not expected!");
            _session->refresh(0);
            break;
        case SigSession::Data_overflow:
            dsv_info("MainWindow::on_session_error(),Data_overflow, stop capture");
            _session->stop_capture();
            title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DATA_OVERFLOW), "Data Overflow");
            details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DATA_OVERFLOW_DET), 
                      "USB bandwidth can not support current sample rate! \nPlease reduce the sample rate!");
            break;
        default:
            title = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_UNDEFINED_ERROR), "Undefined Error");
            details = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_UNDEFINED_ERROR_DET), "Not expected error!");
            break;
        }

        pv::dialogs::DSMessageBox msg(this, title);
        msg.mBox()->setText(details);
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        connect(_session->device_event_object(), SIGNAL(device_updated()), &msg, SLOT(accept()));
        _msg = &msg;
        msg.exec();
        _msg = NULL;

        _session->clear_error();
    }

    void MainWindow::save_config()
    { 
        if (_device_agent->have_instance() == false)
        {
            dsv_info("There is no need to save the configuration");
            return;
        }

        AppConfig &app = AppConfig::Instance();        

        if (_device_agent->is_hardware()){
            QString sessionFile = gen_config_file_path(true);
            save_config_to_file(sessionFile);
        }

        app.frameOptions.windowState = saveState();
        app.SaveFrame();
    }

    QString MainWindow::gen_config_file_path(bool isNewFormat)
    { 
        AppConfig &app = AppConfig::Instance();

        QString file = GetProfileDir();
        QDir dir(file);
        if (dir.exists() == false){
            dir.mkpath(file);
        } 

        QString driver_name = _device_agent->driver_name();
        QString mode_name = QString::number(_device_agent->get_work_mode()); 
        QString lang_name;
        QString base_path = dir.absolutePath() + "/" + driver_name + mode_name;

        if (!isNewFormat){
            lang_name = QString::number(app.frameOptions.language);           
        }

        return base_path + ".ses" + lang_name + ".dsc";
    }

    bool MainWindow::able_to_close()
    {
        if (_device_agent->is_hardware() && _session->have_hardware_data() == false){
            _sampling_bar->commit_settings();
        }

        _tab_widget->closeAllDetachedWindows();

        save_config();
        
        if (confirm_to_store_data()){
            on_save();
            return false; 
        } 
        return true;
    }

    void MainWindow::on_side_bar_dock_clicked(int index)
    {
        bool isChecked = _side_bar->getItem(index)->button->isChecked();

        if (!isChecked) {
            if (_sliding_drawer->isOpen())
                _sliding_drawer->close();
            current_view()->show_search_cursor(false);
            ::DockOptions *opt = getDockOptions();
            if (opt) {
                opt->decodeDock = false;
                opt->triggerDock = false;
                opt->measureDock = false;
                opt->searchDock = false;
                opt->deviceOptionsDock = false;
                AppConfig::Instance().SaveFrame();
            }
            current_view()->setFocus();
            return;
        }

        int drawerPage = -1;

        switch (index) {
        case SIDEBAR_TRIGGER:
            if (_device_agent->get_work_mode() != DSO) {
                _trigger_widget->update_view();
                drawerPage = _drawer_page_trigger;
            } else {
                _dso_trigger_widget->update_view();
                drawerPage = _drawer_page_dso_trigger;
            }
            break;
        case SIDEBAR_DECODE:
            drawerPage = _drawer_page_protocol;
            break;
        case SIDEBAR_MEASURE:
            drawerPage = _drawer_page_measure;
            break;
        case SIDEBAR_SEARCH:
            current_view()->show_search_cursor(true);
            drawerPage = _drawer_page_search;
            break;
        case SIDEBAR_FUNCTION:
            break;
        case SIDEBAR_OPTIONS:
            _device_options_widget->update_view();
            drawerPage = _drawer_page_device_options;
            break;
        }

        if (drawerPage >= 0) {
            _sliding_drawer->open(drawerPage);
            _drawer_current_page = drawerPage;
        } else if (_sliding_drawer->isOpen()) {
            _sliding_drawer->close();
        }

        ::DockOptions *opt = getDockOptions();
        if (opt) {
            opt->decodeDock = (index == SIDEBAR_DECODE);
            opt->triggerDock = (index == SIDEBAR_TRIGGER);
            opt->measureDock = (index == SIDEBAR_MEASURE);
            opt->searchDock = (index == SIDEBAR_SEARCH);
            opt->deviceOptionsDock = (index == SIDEBAR_OPTIONS);
            AppConfig::Instance().SaveFrame();
        }

        current_view()->setFocus();
    }

    void MainWindow::on_side_bar_action_clicked(int index)
    {
        switch (index) {
        case SIDEBAR_RUNSTOP:
            _sampling_bar->run_or_stop();
            break;
        case SIDEBAR_INSTANT:
            _sampling_bar->run_or_stop_instant();
            break;
        }
    }

    void MainWindow::on_screenShot()
    {
        AppConfig &app = AppConfig::Instance();
        QString default_name = app.userHistory.screenShotPath + "/" + APP_NAME + QDateTime::currentDateTime().toString("-yyMMdd-hhmmss");

        int x = parentWidget()->pos().x();
        int y = parentWidget()->pos().y();
        int w = parentWidget()->frameGeometry().width();
        int h = parentWidget()->frameGeometry().height();

        (void)h;
        (void)w;
        (void)x;
        (void)y;

#ifdef _WIN32 
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(QApplication::desktop->winId(), x, y, w, h);
    #else
        QPixmap pixmap = QPixmap::grabWidget(parentWidget());
    #endif
#elif __APPLE__ 
        x += MainFrame::Margin;
        y += MainFrame::Margin;
        w -= MainFrame::Margin * 2;
        h -= MainFrame::Margin * 2;

    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(winId(), x, y, w, h);
    #else
        QDesktopWidget *desktop = QApplication::desktop();
        int curMonitor = desktop->screenNumber(this);
        QPixmap pixmap = QGuiApplication::screens().at(curMonitor)->grabWindow(winId(), x, y, w, h);
    #endif       
#else       
        QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(winId());
#endif

        QString format = "png";
        QString fileName = QFileDialog::getSaveFileName(
            this,
            L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVE_AS), "Save As"),
            default_name,
            "png file(*.png);;jpeg file(*.jpeg)",
            &format);

        if (!fileName.isEmpty())
        {
            QStringList list = format.split('.').last().split(')');
            QString suffix = list.first();

            QFileInfo f(fileName);
            if (f.suffix().compare(suffix))
            {
                //tr
                fileName += "." + suffix;
            }

            pixmap.save(fileName, suffix.toLatin1());

            fileName = path::GetDirectoryName(fileName);

            if (app.userHistory.screenShotPath != fileName)
            {
                app.userHistory.screenShotPath = fileName;
                app.SaveHistory();
            }
        }
    }

    // save file
    void MainWindow::on_save()
    {
        using pv::dialogs::StoreProgress;

        if (_device_agent->have_instance() == false)
        {
            dsv_info("Have no device, can't to save data.");
            return;
        }

        if (_session->is_working()){
            dsv_info("Save data: stop the current device."); 
            _session->stop_capture();
        }

        _session->set_saving(true);

        StoreProgress *dlg = new StoreProgress(_session, this);
        dlg->SetView(current_view());
        dlg->save_run(this);
    }

    void MainWindow::on_export()
    {
        using pv::dialogs::StoreProgress;

        if (_session->is_working()){
            dsv_info("Export data: stop the current device."); 
            _session->stop_capture();
        }

        StoreProgress *dlg = new StoreProgress(_session, this);
        dlg->SetView(current_view());
        dlg->export_run();
    }

    bool MainWindow::on_load_session(QString name)
    {
        return load_config_from_file(name);
    }

    bool MainWindow::load_config_from_file(QString file)
    {
        if (file == ""){
            dsv_err("File name is empty.");
            assert(false);
        }

        _protocol_widget->del_all_protocol();

        std::string file_name = pv::path::ToUnicodePath(file);
        dsv_info("Load device profile: \"%s\"", file_name.c_str());
        
        QFile sf(file);

        if (!sf.exists()){ 
            dsv_warn("Warning: device profile is not exists: \"%s\"", file_name.c_str());
            return false;
        }

        if (!sf.open(QIODevice::ReadOnly))
        {
            dsv_warn("Warning: Couldn't open device profile to load!");
            return false;
        }

        QString data = QString::fromUtf8(sf.readAll());
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
        sf.close();

        bool bDecoder = false;
        int ret = load_config_from_json(doc, bDecoder);

        if (ret && _device_agent->get_work_mode() == DSO)
        {
            _dso_trigger_widget->update_view();
        }

        if (_device_agent->is_hardware()){
            _title_ext_string = file;
            update_title_bar_text();
        }

        return ret;
    }

    bool MainWindow::gen_config_json(QJsonObject &sessionVar)
    {
        AppConfig &app = AppConfig::Instance();

        GVariant *gvar_opts;
        GVariant *gvar;
        gsize num_opts;

        QString title = QApplication::applicationName() + " v" + QApplication::applicationVersion();

        QJsonArray channelVar;
        sessionVar["Version"] = QJsonValue::fromVariant(SESSION_FORMAT_VERSION);
        sessionVar["Device"] = QJsonValue::fromVariant(_device_agent->driver_name());
        sessionVar["DeviceMode"] = QJsonValue::fromVariant(_device_agent->get_work_mode());
        sessionVar["Language"] = QJsonValue::fromVariant(app.frameOptions.language);
        sessionVar["Title"] = QJsonValue::fromVariant(title);

        if (_device_agent->is_hardware() && _device_agent->get_work_mode() == LOGIC)
        {
            sessionVar["CollectMode"] = _session->get_collect_mode();
        }

        gvar_opts = _device_agent->get_config_list(NULL, SR_CONF_DEVICE_SESSIONS);
        if (gvar_opts == NULL)
        {
            dsv_warn("Device config list is empty. id:SR_CONF_DEVICE_SESSIONS");
            /* Driver supports no device instance sessions. */
            return false;
        }

        const int *const options = (const int32_t *)g_variant_get_fixed_array(
                                        gvar_opts, &num_opts, sizeof(int32_t));

        for (unsigned int i = 0; i < num_opts; i++)
        {
            const struct sr_config_info *const info = _device_agent->get_config_info(options[i]);
            gvar = _device_agent->get_config(info->key);
            if (gvar != NULL)
            {
                if (info->datatype == SR_T_BOOL)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_boolean(gvar));
                else if (info->datatype == SR_T_UINT64)
                    sessionVar[info->name] = QJsonValue::fromVariant(QString::number(g_variant_get_uint64(gvar)));
                else if (info->datatype == SR_T_UINT8)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_byte(gvar));
                 else if (info->datatype == SR_T_INT16)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_int16(gvar));
                else if (info->datatype == SR_T_FLOAT) //save as string format
                    sessionVar[info->name] = QJsonValue::fromVariant(QString::number(g_variant_get_double(gvar)));
                else if (info->datatype == SR_T_CHAR)
                    sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_string(gvar, NULL));
                else if (info->datatype == SR_T_LIST)
                    sessionVar[info->name] =  QJsonValue::fromVariant(g_variant_get_int16(gvar));
                else{
                    dsv_err("Unkown config info type:%d", info->datatype);
                    assert(false);
                }
                g_variant_unref(gvar);                
            }
        }

        for (auto s : _session->get_signals())
        {
            QJsonObject s_obj;
            s_obj["index"] = s->get_index();
            s_obj["view_index"] = s->get_view_index();
            s_obj["type"] = s->get_type();
            s_obj["enabled"] = s->enabled();
            s_obj["name"] = s->get_name();

            if (s->get_colour().isValid())
                s_obj["colour"] = QJsonValue::fromVariant(s->get_colour());
            else
                s_obj["colour"] = QJsonValue::fromVariant("default");

            view::LogicSignal *logicSig = NULL;
            if ((logicSig = dynamic_cast<view::LogicSignal *>(s)))
            {
                s_obj["strigger"] = logicSig->get_trig();
            }
            
            if (s->signal_type() == SR_CHANNEL_DSO)
            {
                view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                s_obj["vdiv"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_vDialValue()));
                s_obj["vfactor"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_factor()));
                s_obj["coupling"] = dsoSig->get_acCoupling();
                s_obj["trigValue"] = dsoSig->get_trig_vrate();
                s_obj["zeroPos"] = dsoSig->get_zero_ratio();
            }
 
            if (s->signal_type() == SR_CHANNEL_ANALOG)
            {
                view::AnalogSignal *analogSig = (view::AnalogSignal*)s;
                s_obj["vdiv"] = QJsonValue::fromVariant(static_cast<qulonglong>(analogSig->get_vdiv()));
                s_obj["vfactor"] = QJsonValue::fromVariant(static_cast<qulonglong>(analogSig->get_factor()));
                s_obj["coupling"] = analogSig->get_acCoupling();
                s_obj["zeroPos"] = analogSig->get_zero_ratio();
                s_obj["mapUnit"] = analogSig->get_mapUnit();
                s_obj["mapMin"] = analogSig->get_mapMin();
                s_obj["mapMax"] = analogSig->get_mapMax();
                s_obj["mapDefault"] = analogSig->get_mapDefault();
            }
            channelVar.append(s_obj);
        }
        sessionVar["channel"] = channelVar;

        if (_device_agent->get_work_mode() == LOGIC)
        {
            sessionVar["trigger"] = _trigger_widget->get_session();
        }

        StoreSession ss(_session);
        QJsonArray decodeJson;
        ss.gen_decoders_json(decodeJson);
        sessionVar["decoder"] = decodeJson;

        if (_device_agent->get_work_mode() == DSO)
        {
            sessionVar["measure"] = current_view()->get_viewstatus()->get_session();
        }

        if (gvar_opts != NULL)
            g_variant_unref(gvar_opts);

        return true;
    }

    bool MainWindow::load_config_from_json(QJsonDocument &doc, bool &haveDecoder)
    {
        haveDecoder = false;

        QJsonObject sessionObj = doc.object();

        int mode = _device_agent->get_work_mode();

        // check config file version
        if (!sessionObj.contains("Version"))
        {
            dsv_dbg("Profile version is not exists!");
            return false;
        }

        int format_ver = sessionObj["Version"].toInt();

        if (format_ver < 2)
        {
            dsv_err("Profile version is error!");
            return false;
        }

        if (sessionObj.contains("CollectMode") && _device_agent->is_hardware()){
            int collect_mode = sessionObj["CollectMode"].toInt();
            _session->set_collect_mode((DEVICE_COLLECT_MODE)collect_mode);
        }

        int conf_dev_mode = sessionObj["DeviceMode"].toInt();

        if (_device_agent->is_hardware())
        {
            QString driverName = _device_agent->driver_name();
            QString sessionDevice = sessionObj["Device"].toString();            
            // check device and mode
            if (driverName != sessionDevice || mode != conf_dev_mode)
            {
                MsgBox::Show(NULL, L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PROFILE_NOT_COMPATIBLE), "Profile is not compatible with current device or mode!"), this);
                return false;
            }
        }

        // load device settings
        GVariant *gvar_opts = _device_agent->get_config_list(NULL, SR_CONF_DEVICE_SESSIONS);
        gsize num_opts;

        if (gvar_opts != NULL)
        {
            const int *const options = (const int32_t *)g_variant_get_fixed_array(
                gvar_opts, &num_opts, sizeof(int32_t));

            for (unsigned int i = 0; i < num_opts; i++)
            {
                const struct sr_config_info *info = _device_agent->get_config_info(options[i]);

                if (!sessionObj.contains(info->name))
                    continue;

                GVariant *gvar = NULL;
                int id = 0;

                if (info->datatype == SR_T_BOOL){
                    gvar = g_variant_new_boolean(sessionObj[info->name].toInt());
                }
                else if (info->datatype == SR_T_UINT64){
                    //from string text.
                    gvar = g_variant_new_uint64(sessionObj[info->name].toString().toULongLong());         
                }
                else if (info->datatype == SR_T_UINT8){
                    if (sessionObj[info->name].toString() != "")
                        gvar = g_variant_new_byte(sessionObj[info->name].toString().toUInt());
                    else
                        gvar = g_variant_new_byte(sessionObj[info->name].toInt());                       
                }
                else if (info->datatype == SR_T_INT16){
                    gvar = g_variant_new_int16(sessionObj[info->name].toInt());
                }
                else if (info->datatype == SR_T_FLOAT){
                    if (sessionObj[info->name].toString() != "")
                        gvar = g_variant_new_double(sessionObj[info->name].toString().toDouble());
                    else
                        gvar = g_variant_new_double(sessionObj[info->name].toDouble()); 
                }
                else if (info->datatype == SR_T_CHAR){
                    gvar = g_variant_new_string(sessionObj[info->name].toString().toLocal8Bit().data());
                }
                else if (info->datatype == SR_T_LIST)
                { 
                    id = 0;

                    if (format_ver > 2){
                        // Is new version format.
                        id = sessionObj[info->name].toInt();
                    }
                    else{
                        const char *fd_key = sessionObj[info->name].toString().toLocal8Bit().data();
                        id = ds_dsl_option_value_to_code(conf_dev_mode, info->key, fd_key);
                        if (id == -1){
                            dsv_err("Convert failed, key:\"%s\", value:\"%s\""
                                ,info->name, fd_key);
                            id = 0; //set default value.
                        }
                        else{
                            dsv_info("Convert success, key:\"%s\", value:\"%s\", get code:%d"
                                ,info->name, fd_key, id);
                        }
                    }            
                    gvar = g_variant_new_int16(id);
                }

                if (gvar == NULL)
                {
                    dsv_warn("Warning: Profile failed to parse key:'%s'", info->name);
                    continue;
                }

                bool bFlag = _device_agent->set_config(info->key, gvar);
                if (!bFlag){
                    dsv_err("Set device config option failed, id:%d, code:%d", info->key, id);
                }   
            }
        }

        // load channel settings
        if (mode == DSO)
        {
            for (const GSList *l = _device_agent->get_channels(); l; l = l->next)
            {
                sr_channel *const probe = (sr_channel *)l->data;
                assert(probe);

                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();
                    if (QString(probe->name) == obj["name"].toString() &&
                        probe->type == obj["type"].toDouble())
                    {
                        probe->vdiv = obj["vdiv"].toDouble();
                        probe->coupling = obj["coupling"].toDouble();
                        probe->vfactor = obj["vfactor"].toDouble();
                        probe->trig_value = obj["trigValue"].toDouble();
                        probe->map_unit = g_strdup(obj["mapUnit"].toString().toStdString().c_str());
                        probe->map_min = obj["mapMin"].toDouble();
                        probe->map_max = obj["mapMax"].toDouble();
                        probe->enabled = obj["enabled"].toBool();
                        break;
                    }
                }
            }
        }
        else
        {
            for (const GSList *l = _device_agent->get_channels(); l; l = l->next)
            {
                sr_channel *const probe = (sr_channel *)l->data;
                assert(probe);
                bool isEnabled = false;

                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();

                    if ((probe->index == obj["index"].toInt()) &&
                        (probe->type == obj["type"].toInt()))
                    {
                        isEnabled = true;
                        QString chan_name = obj["name"].toString().trimmed();
                        if (chan_name == ""){
                            chan_name = QString::number(probe->index);
                        }
                        
                        probe->enabled = obj["enabled"].toBool();
                        probe->name = g_strdup(chan_name.toStdString().c_str());
                        probe->vdiv = obj["vdiv"].toDouble();
                        probe->coupling = obj["coupling"].toDouble();
                        probe->vfactor = obj["vfactor"].toDouble();
                        probe->trig_value = obj["trigValue"].toDouble();
                        probe->map_unit = g_strdup(obj["mapUnit"].toString().toStdString().c_str());
                        probe->map_min = obj["mapMin"].toDouble();
                        probe->map_max = obj["mapMax"].toDouble();

                        if (obj.contains("mapDefault"))
                        {
                            probe->map_default = obj["mapDefault"].toBool();
                        }

                        break;
                    }
                }
                if (!isEnabled)
                    probe->enabled = false;
            }
        }

        _session->reload();

        // load signal setting
        if (mode == DSO)
        {
            for (auto s : _session->get_signals())
            {
                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();

                    if (s->get_name() ==  obj["name"].toString() &&
                        s->get_type() ==  obj["type"].toDouble())
                    {
                        s->set_colour(QColor(obj["colour"].toString()));
                       
                        if (s->signal_type() == SR_CHANNEL_DSO)
                        {   
                            view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                            dsoSig->load_settings();
                            dsoSig->set_zero_ratio(obj["zeroPos"].toDouble());
                            dsoSig->set_trig_ratio(obj["trigValue"].toDouble());
                            dsoSig->commit_settings();
                        }
                        break;
                    }
                }
            }
        }
        else
        {
            for (auto s : _session->get_signals())
            {
                for (const QJsonValue &value : sessionObj["channel"].toArray())
                {
                    QJsonObject obj = value.toObject();
                    if ((s->get_index() == obj["index"].toInt()) &&
                        (s->get_type() == obj["type"].toInt()))
                    {
                        QString chan_name = obj["name"].toString().trimmed();
                        if (chan_name == ""){
                            chan_name = QString::number(s->get_index());
                        }

                        s->set_colour(QColor(obj["colour"].toString()));
                        s->set_name(chan_name);

                        view::LogicSignal *logicSig = NULL;
                        if ((logicSig = dynamic_cast<view::LogicSignal *>(s)))
                        {
                            logicSig->set_trig(obj["strigger"].toDouble());
                        }
 
                        if (s->signal_type() == SR_CHANNEL_DSO)
                        {
                            view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                            dsoSig->load_settings();
                            dsoSig->set_zero_ratio(obj["zeroPos"].toDouble());
                            dsoSig->set_trig_ratio(obj["trigValue"].toDouble());
                            dsoSig->commit_settings();
                        }
 
                        if (s->signal_type() == SR_CHANNEL_ANALOG)
                        {   
                            view::AnalogSignal *analogSig = (view::AnalogSignal*)s;
                            analogSig->set_zero_ratio(obj["zeroPos"].toDouble());
                            analogSig->commit_settings();
                        }
                        
                        break;
                    }
                }
            }
        }

        // update UI settings
        _sampling_bar->update_sample_rate_list();
        _trigger_widget->device_updated();
        current_view()->header_updated();

        // load trigger settings
        if (sessionObj.contains("trigger"))
        {
            _trigger_widget->set_session(sessionObj["trigger"].toObject());
        }

        // load decoders
        if (sessionObj.contains("decoder"))
        {
            QJsonArray deArray = sessionObj["decoder"].toArray();
            if (deArray.empty() == false)
            {
                haveDecoder = true;
                StoreSession ss(_session);
                ss.load_decoders(_protocol_widget, deArray);
                current_view()->update_all_trace_postion();
            }
        }

        // load measure
        if (sessionObj.contains("measure"))
        {
            auto *bottom_bar = current_view()->get_viewstatus();
            bottom_bar->load_session(sessionObj["measure"].toArray(), format_ver);
        }

        if (gvar_opts != NULL)
            g_variant_unref(gvar_opts);

        load_channel_view_indexs(doc);

        return true;
    }

    void MainWindow::load_channel_view_indexs(QJsonDocument &doc)
    {
        QJsonObject sessionObj = doc.object();

        int mode = _device_agent->get_work_mode();
        if (mode != LOGIC)
            return;

        std::vector<int> view_indexs;

        for (const QJsonValue &value : sessionObj["channel"].toArray()){
            QJsonObject obj = value.toObject();

            if (obj.contains("view_index")){  
                view_indexs.push_back(obj["view_index"].toInt());
            }
        }

         if (view_indexs.size()){
            int i = 0;

            for (auto s : _session->get_signals()){
                s->set_view_index(view_indexs[i]);
                i++;
            }

            current_view()->update_all_trace_postion();
        }
    }
    
    bool MainWindow::on_store_session(QString name)
    {
        return save_config_to_file(name);
    }

    bool MainWindow::save_config_to_file(QString name)
    {
        if (name == ""){
            dsv_err("Session file name is empty.");
            assert(false);
        }

        std::string file_name = pv::path::ToUnicodePath(name);
        dsv_info("Store session to file: \"%s\"", file_name.c_str());

        QFile sf(name);
        if (!sf.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            dsv_warn("Warning: Couldn't open profile to write!");
            return false;
        }

        QTextStream outStream(&sf);
        encoding::set_utf8(outStream);

        QJsonObject sessionVar;
        if (!gen_config_json(sessionVar)){
            return false;
        }

        QJsonDocument sessionDoc(sessionVar);
        outStream << QString::fromUtf8(sessionDoc.toJson());
        sf.close();
        return true;
    }

    bool MainWindow::genSessionData(std::string &str)
    {
        QJsonObject sessionVar;
        if (!gen_config_json(sessionVar))
        {
            return false;
        }

        QJsonDocument sessionDoc(sessionVar);
        QString data = QString::fromUtf8(sessionDoc.toJson());
        str.append(data.toLocal8Bit().data());
        return true;
    }

    ::DockOptions* MainWindow::getDockOptions()
    {
        AppConfig &app = AppConfig::Instance();
        int mode = _device_agent->get_work_mode();
        if (mode == LOGIC)
            return &app.frameOptions._logicDock;
        else if (mode == DSO)
            return &app.frameOptions._dsoDock;
        else
            return &app.frameOptions._analogDock;
    }

    void MainWindow::restore_dock()
    { 
        if (_device_agent->have_instance())
            _trig_bar->reload();

        _side_bar->clearAllChecked();

        ::DockOptions *opt = getDockOptions();
        if (opt) {
            if (opt->decodeDock) {
                _side_bar->setItemChecked(SIDEBAR_DECODE, true);
                _sliding_drawer->open(_drawer_page_protocol);
                _drawer_current_page = _drawer_page_protocol;
            } else if (opt->triggerDock) {
                _side_bar->setItemChecked(SIDEBAR_TRIGGER, true);
                int mode = _device_agent->get_work_mode();
                if (mode != DSO) {
                    _trigger_widget->update_view();
                    _sliding_drawer->open(_drawer_page_trigger);
                    _drawer_current_page = _drawer_page_trigger;
                } else {
                    _dso_trigger_widget->update_view();
                    _sliding_drawer->open(_drawer_page_dso_trigger);
                    _drawer_current_page = _drawer_page_dso_trigger;
                }
            } else if (opt->measureDock) {
                _side_bar->setItemChecked(SIDEBAR_MEASURE, true);
                _sliding_drawer->open(_drawer_page_measure);
                _drawer_current_page = _drawer_page_measure;
            } else if (opt->searchDock) {
                _side_bar->setItemChecked(SIDEBAR_SEARCH, true);
                current_view()->show_search_cursor(true);
                _sliding_drawer->open(_drawer_page_search);
                _drawer_current_page = _drawer_page_search;
            } else if (opt->deviceOptionsDock) {
                _side_bar->setItemChecked(SIDEBAR_OPTIONS, true);
                _device_options_widget->update_view();
                _sliding_drawer->open(_drawer_page_device_options);
                _drawer_current_page = _drawer_page_device_options;
            }
        }
    }

    bool MainWindow::eventFilter(QObject *object, QEvent *event)
    {
        (void)object;
    
        if (event->type() == QEvent::KeyPress)
        {
            static bool in_filter = false;
            if (in_filter) return false;

            QKeyEvent *ke = (QKeyEvent *)event;
            QWidget *focused = qApp->focusWidget();

            dsv_info("MainWindow::eventFilter key=%d, object=%p (%s), focused=%p (%s)", 
                     ke->key(), object, object->metaObject()->className(), 
                     focused, focused ? focused->metaObject()->className() : "NULL");

            if (focused && qobject_cast<pv::widgets::SearchPatternInput*>(focused)) {
                in_filter = true;
                qApp->sendEvent(focused, event);
                in_filter = false;
                return true; 
            }

            // Manually forward events to focus widget if it's an input or in the drawer
            if (focused && (
                qobject_cast<QLineEdit*>(focused) ||
                qobject_cast<QAbstractSpinBox*>(focused) ||
                qobject_cast<QComboBox*>(focused) ||
                qobject_cast<QAbstractButton*>(focused) ||
                (_sliding_drawer && _sliding_drawer->isAncestorOf(focused)) ||
                (_device_options_widget && _device_options_widget->isAncestorOf(focused)) ||
                (_search_widget && _search_widget->isAncestorOf(focused)) ||
                (_trigger_widget && _trigger_widget->isAncestorOf(focused)) ||
                (_protocol_widget && _protocol_widget->isAncestorOf(focused)) ||
                (_dso_trigger_widget && _dso_trigger_widget->isAncestorOf(focused)) ||
                (_measure_widget && _measure_widget->isAncestorOf(focused))
            )) {
                QWidget *target = focused;
                if (focused->focusProxy()) {
                    target = focused->focusProxy();
                } else if (qobject_cast<QAbstractSpinBox*>(focused) || qobject_cast<QComboBox*>(focused)) {
                    QLineEdit *le = focused->findChild<QLineEdit*>();
                    if (le) {
                        target = le;
                    }
                }
                
                QString text = ke->text();
                uint key = ke->key();
                
                // Fix for WinNativeWidget's raw VK codes
                if (key == 0x08) key = Qt::Key_Backspace;
                else if (key == 0x0D) key = Qt::Key_Return;
                else if (key == 0x25) key = Qt::Key_Left;
                else if (key == 0x26) key = Qt::Key_Up;
                else if (key == 0x27) key = Qt::Key_Right;
                else if (key == 0x28) key = Qt::Key_Down;
                else if (key == 0x2E) key = Qt::Key_Delete;
                else if (key == 0x24) key = Qt::Key_Home;
                else if (key == 0x23) key = Qt::Key_End;

                if (text.isEmpty() && target->inherits("QLineEdit")) {
                    if (ke->key() >= Qt::Key_Space && ke->key() <= Qt::Key_AsciiTilde) {
                        char c = (char)ke->key();
                        bool shift = (ke->modifiers() & Qt::ShiftModifier);
                        if (c >= 'A' && c <= 'Z' && !shift) {
                            c += 32; 
                        } else if (c >= 'a' && c <= 'z' && shift) {
                            c -= 32;
                        }
                        text = QString(QChar(c));
                    }
                }

                QKeyEvent newEvent(ke->type(), key, ke->modifiers(), text, ke->isAutoRepeat(), ke->count());

                dsv_info("  Forwarding event to focused widget: %s (target: %s, text: %s, mapped_key: %d)", 
                         focused->metaObject()->className(), target->metaObject()->className(), text.toStdString().c_str(), key);
                in_filter = true;
                qApp->sendEvent(target, &newEvent);
                in_filter = false;
                return true; 
            }




            const auto &sigs = _session->get_signals();
            
            int modifier = ke->modifiers(); 
 
            if(modifier & Qt::ControlModifier || 
               modifier & Qt::ShiftModifier || 
               modifier & Qt::AltModifier)
            {
                return true;
            }

            high_resolution_clock::time_point key_press_time = high_resolution_clock::now();
            milliseconds timeInterval = std::chrono::duration_cast<milliseconds>(key_press_time - _last_key_press_time);
            int64_t time_keep =  timeInterval.count();
            if (time_keep < 200){
                return true;
            }
            _last_key_press_time = key_press_time;           
            
            switch (ke->key())
            {
            
            // case Qt::Key_F:
            //     _category_file->popup();
            //     break;
            case Qt::Key_S:
                _sampling_bar->run_or_stop();
                break;

            case Qt::Key_I:
                _sampling_bar->run_or_stop_instant();
                break;

            case Qt::Key_T:
                _side_bar->getItem(SIDEBAR_TRIGGER)->button->click();
                break;

            case Qt::Key_D:
                _side_bar->getItem(SIDEBAR_DECODE)->button->click();
                break;

            case Qt::Key_M:
                _side_bar->getItem(SIDEBAR_MEASURE)->button->click();
                break;

            case Qt::Key_R:
                _side_bar->getItem(SIDEBAR_SEARCH)->button->click();
                break;

            case Qt::Key_O:
                _side_bar->getItem(SIDEBAR_OPTIONS)->button->click();
                break;

            case Qt::Key_PageUp:
                current_view()->set_scale_offset(current_view()->scale(),
                                        current_view()->offset() - current_view()->get_view_width());
                break;
            case Qt::Key_PageDown:
                current_view()->set_scale_offset(current_view()->scale(),
                                        current_view()->offset() + current_view()->get_view_width());

                break;

            case Qt::Key_Left:
                current_view()->zoom(1);
                break;

            case Qt::Key_Right:
                current_view()->zoom(-1);
                break;

            case Qt::Key_0:
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO)
                    {
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_index() == 0)
                            dsoSig->set_vDialActive(!dsoSig->get_vDialActive());
                        else
                            dsoSig->set_vDialActive(false);
                    }
                }
                current_view()->setFocus();
                update();
                break;

            case Qt::Key_1:
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO)
                    {
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_index() == 1)
                            dsoSig->set_vDialActive(!dsoSig->get_vDialActive());
                        else
                            dsoSig->set_vDialActive(false);
                    }
                }
                current_view()->setFocus();
                update();
                break;

            case Qt::Key_Up: 
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO){
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_vDialActive())
                        {
                            dsoSig->go_vDialNext(true);
                            update();
                            break;
                        }
                    }
                }
                break;

            case Qt::Key_Down:
                for (auto s : sigs)
                {
                    if (s->signal_type() == SR_CHANNEL_DSO){
                        view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                        if (dsoSig->get_vDialActive())
                        {
                            dsoSig->go_vDialPre(true);
                            update();
                            break;
                        }
                    }
                }
                break;
            case Qt::Key_E:
                _sampling_bar->device_selected();
                break;
            default:
                return false;
            }
            return true;
        }
        return false;
    }

    void MainWindow::switchLanguage(int language)
    {
        if (language == 0)
            return;
        
        AppConfig &app = AppConfig::Instance();

        if (app.frameOptions.language != language && language > 0)
        {
            app.frameOptions.language = language;
            app.SaveFrame();
            LangResource::Instance()->Load(language);     
        }        

        if (language == LAN_CN)
        {
            _qtTrans.load(":/qt_" + QString::number(language));
            qApp->installTranslator(&_qtTrans);
            _myTrans.load(":/my_" + QString::number(language));
            qApp->installTranslator(&_myTrans);
        }
        else if (language == LAN_EN)
        {
            qApp->removeTranslator(&_qtTrans);
            qApp->removeTranslator(&_myTrans);
        }

        retranslateUi();

        UiManager::Instance()->Update(UI_UPDATE_ACTION_LANG);
        _session->update_lang_text();
    }

    void MainWindow::switchTheme(QString style)
    {
        AppConfig &app = AppConfig::Instance();

        if (app.frameOptions.style != style)
        {
            app.frameOptions.style = style;
            app.SaveFrame();
        }

        QString qssRes = ":/" + style + ".qss";
        QFile qss(qssRes);
        if (!qss.open(QFile::ReadOnly | QFile::Text)) {
            return;
        }
        QString qssContent = qss.readAll();
        qss.close();

        QHash<QString, QString> tokens;
        QRegularExpression tokenRe("@([\\w-]+):\\s*([^\\r\\n]+?)\\s*(?:\\*/|\\r|\\n)");
        QRegularExpressionMatchIterator it = tokenRe.globalMatch(qssContent);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString tokenName = "@" + match.captured(1);
            QString tokenValue = match.captured(2).trimmed();
            tokens[tokenName] = tokenValue;
        }

        QList<QString> keys = tokens.keys();
        std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
            return a.length() > b.length();
        });

        for (const QString &key : keys) {
            qssContent.replace(key, tokens[key]);
        }

        app.SetThemeTokens(tokens);

        qApp->setStyleSheet(qssContent);

        UiManager::Instance()->Update(UI_UPDATE_ACTION_THEME);
        UiManager::Instance()->Update(UI_UPDATE_ACTION_FONT);

        data_updated();
        Ribbon_retranslateUi();
    }

    void MainWindow::data_updated()
    {
        _event.data_updated(); // safe call
    }

    void MainWindow::on_data_updated()
    {
        _measure_widget->reCalc();
        current_view()->data_updated();
    }

    void MainWindow::on_open_doc()
    {
        openDoc();
    }

    void MainWindow::openDoc()
    {
        QDir dir(GetAppDataDir());
        AppConfig &app = AppConfig::Instance();
        int lan = app.frameOptions.language;
        QDesktopServices::openUrl(
            QUrl("file:///" + dir.absolutePath() + "/ug" + QString::number(lan) + ".pdf"));
    }

    void MainWindow::update_capture()
    {
        current_view()->update_hori_res();
    }

    void MainWindow::cur_snap_samplerate_changed()
    {
        _event.cur_snap_samplerate_changed(); // safe call
    }

    void MainWindow::on_cur_snap_samplerate_changed()
    {
        _measure_widget->reCalc();
    }

    /*------------------on event end-------*/

    void MainWindow::signals_changed()
    {
        _event.signals_changed(); // safe call
    }

    void MainWindow::on_signals_changed()
    {
        current_view()->signals_changed(NULL);
    }

    void MainWindow::receive_trigger(quint64 trigger_pos)
    {
        _event.receive_trigger(trigger_pos); // save call
    }

    void MainWindow::on_receive_trigger(quint64 trigger_pos)
    {
        current_view()->receive_trigger(trigger_pos);
    }

    void MainWindow::frame_ended()
    {
        _event.frame_ended(); // save call
    }

    void MainWindow::on_frame_ended()
    {
        dsv_info("MainWindow::on_frame_ended()");
        pv::TabContext *ctx = current_context();
        if (ctx && ctx->document()) {
            _session->copy_data_to_document(ctx->document());
            ctx->document()->save_signal_config(_session->get_device());
            ctx->activate();
        }
        current_view()->receive_end();
    }

    void MainWindow::frame_began()
    {
        _event.frame_began(); // save call
    }

    void MainWindow::on_frame_began()
    {
        pv::TabContext *ctx = current_context();
        if (ctx) {
            ctx->make_live();
            if (ctx->document()) {
                ctx->document()->clear();
                _session->set_active_document(ctx->document());
            }
            current_view()->set_signal_data_from_source(_session);
        }
        current_view()->frame_began();
    }

    void MainWindow::show_region(uint64_t start, uint64_t end, bool keep)
    {
        current_view()->show_region(start, end, keep);
    }

    void MainWindow::show_wait_trigger()
    {
        current_view()->show_wait_trigger();
    }

    void MainWindow::repeat_hold(int percent)
    {
        (void)percent;
        current_view()->repeat_show();
    }

    void MainWindow::decode_done()
    {
        _event.decode_done(); // safe call
    }

    void MainWindow::on_decode_done()
    {
        _protocol_widget->update_model();
    }

    void MainWindow::receive_data_len(quint64 len)
    {
        _event.receive_data_len(len); // safe call
    }

    void MainWindow::on_receive_data_len(quint64 len)
    {
        current_view()->set_receive_len(len);
    }

    void MainWindow::receive_header()
    {
    }

    void MainWindow::check_usb_device_speed()
    {
        // USB device speed check
        if (_device_agent->is_hardware())
        {
            int usb_speed = LIBUSB_SPEED_HIGH;
            _device_agent->get_config_int32(SR_CONF_USB_SPEED, usb_speed);

            bool usb30_support = false;

            if (_device_agent->get_config_bool(SR_CONF_USB30_SUPPORT, usb30_support))
            {
                dsv_info("The device's USB module version: %d.0", usb30_support ? 3 : 2);

                int cable_ver = 1;
                if (usb_speed == LIBUSB_SPEED_HIGH)
                    cable_ver = 2;
                else if (usb_speed == LIBUSB_SPEED_SUPER)
                    cable_ver = 3;

                dsv_info("The cable's USB port version: %d.0", cable_ver);

                if (usb30_support && usb_speed == LIBUSB_SPEED_HIGH){
                    QString str_err(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHECK_USB_SPEED_ERROR),
                        "Plug the device into a USB 2.0 port will seriously affect its performance.\nPlease replug it into a USB 3.0 port."));
                    delay_prop_msg(str_err);
                }
            }
        }
    }

    void MainWindow::trigger_message(int msg)
    {
        _event.trigger_message(msg);
    }

    void MainWindow::on_trigger_message(int msg)
    {
        _session->broadcast_msg(msg);
    }

    void MainWindow::reset_all_view()
    {
        _sampling_bar->reload();
        current_view()->status_clear();
        current_view()->reload();
        current_view()->set_device();
        _trigger_widget->update_view();
        _trigger_widget->device_updated();
        _trig_bar->reload(); 
        _dso_trigger_widget->update_view();
        _measure_widget->reload();
        _device_options_widget->update_view();
        if (_sliding_drawer->isOpen())
            _sliding_drawer->close();
        _side_bar->clearAllChecked();

        if (_device_agent->get_work_mode() == ANALOG)
            current_view()->get_viewstatus()->setVisible(false);
        else
            current_view()->get_viewstatus()->setVisible(true);
    }

    bool MainWindow::confirm_to_store_data()
    {   
        bool ret = false;
        _is_save_confirm_msg = true;       

        if (_session->have_hardware_data() && _session->is_first_store_confirm())
        {   
            // Only popup one time.
            ret =  MsgBox::Confirm(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SAVE_CAPDATE), "Save captured data?"));

            if (!ret && _is_auto_switch_device)
            {
                dsv_info("The data save confirm end, auto switch to the new device.");
                _is_auto_switch_device = false;

                if (_session->is_working())
                    _session->stop_capture();

                _session->set_default_device();
            }
        }

        _is_save_confirm_msg = false;
        return ret;
    }

    void MainWindow::check_config_file_version()
    {
        auto device_agent = _session->get_device();
        if (device_agent->is_file() && device_agent->is_new_device())
        {
            if (device_agent->get_work_mode() == LOGIC)
            {   
                int version = -1; 
                if (device_agent->get_config_int16(SR_CONF_FILE_VERSION, version))
                {
                    if (version == 1)
                    {
                        QString strMsg(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHECK_SESSION_FILE_VERSION_ERROR), 
                        "Current loading file has an old format. \nThis will lead to a slow loading speed. \nPlease resave it after loaded."));
                        MsgBox::Show(strMsg);
                    }
                }
            }
        }
    }

    void MainWindow::load_device_config()
    {   
        _title_ext_string = "";        
        int mode = _device_agent->get_work_mode();
        QString file;

        if (_device_agent->is_hardware())
        { 
            QString ses_name = gen_config_file_path(true);

            bool bExist = false;

            QFile sf(ses_name);
            if (!sf.exists()){
                dsv_info("Try to load the low version profile.");
                ses_name =  gen_config_file_path(false);
            }
            else{
                bExist = true;
            }

            if (!bExist)
            {
                QFile sf2(ses_name);
                if (!sf2.exists()){
                    dsv_info("Try to load the default profile.");
                    ses_name = _file_bar->genDefaultSessionFile();
                }
            } 

            file =  ses_name;
        }
        else if (_device_agent->is_demo())
        {
            QDir dir(GetFirmwareDir());
            if (dir.exists())
            {
                QString ses_name = dir.absolutePath() + "/" 
                            + _device_agent->driver_name() + QString::number(mode) + ".dsc";

                QFile sf(ses_name);
                if (sf.exists()){
                    file = ses_name;
                }
            }
        }

        if (file != ""){
            bool ret = load_config_from_file(file);
            if (ret && _device_agent->is_hardware()){
                _title_ext_string = file;
            }
        }
    }

    QJsonDocument MainWindow::get_config_json_from_data_file(QString file, bool &bSucesss)
    {
        QJsonDocument sessionDoc;
        QJsonParseError error;
        bSucesss = false;

        if (file == ""){
            dsv_err("File name is empty.");
            assert(false);
        }

        auto f_name = pv::path::ConvertPath(file);
        ZipReader rd(f_name.c_str());
        auto *data = rd.GetInnterFileData("session");

        if (data != NULL)
        {
            QByteArray raw_bytes = QByteArray::fromRawData(data->data(), data->size());
            QString jsonStr(raw_bytes.data());
            QByteArray qbs = jsonStr.toUtf8();
            sessionDoc = QJsonDocument::fromJson(qbs, &error);

            if (error.error != QJsonParseError::NoError)
            {
                QString estr = error.errorString();
                dsv_err("File::get_session(), parse json error:\"%s\"!", estr.toUtf8().data());
            }
            else{
                bSucesss = true;
            }

            rd.ReleaseInnerFileData(data);
        }

        return sessionDoc;
    }

    QJsonArray MainWindow::get_decoder_json_from_data_file(QString file, bool &bSucesss)
    {
        QJsonArray dec_array;
        QJsonParseError error;

        bSucesss = false;

        if (file == ""){
            dsv_err("File name is empty.");
            assert(false);
        }

        /* read "decoders" */
        auto f_name = path::ConvertPath(file);
        ZipReader rd(f_name.c_str());
        auto *data = rd.GetInnterFileData("decoders");

        if (data != NULL)
        {
            QByteArray raw_bytes = QByteArray::fromRawData(data->data(), data->size());
            QString jsonStr(raw_bytes.data());
            QByteArray qbs = jsonStr.toUtf8();
            QJsonDocument sessionDoc = QJsonDocument::fromJson(qbs, &error);

            if (error.error != QJsonParseError::NoError)
            {
                QString estr = error.errorString();
                dsv_err("MainWindow::get_decoder_json_from_file(), parse json error:\"%s\"!", estr.toUtf8().data());
            }
            else{
                bSucesss = true;
            }

            dec_array = sessionDoc.array();
            rd.ReleaseInnerFileData(data);
        }

        return dec_array;
    }

    void MainWindow::update_toolbar_view_status()
    {
        _sampling_bar->update_view_status();
        _file_bar->update_view_status();
        _trig_bar->update_view_status();

        bool bEnable = _session->is_working() == false;
        int mode = _device_agent->get_work_mode();

        _side_bar->setItemEnabled(SIDEBAR_TRIGGER, bEnable);
        _side_bar->setItemEnabled(SIDEBAR_DECODE, bEnable);
        _side_bar->setItemEnabled(SIDEBAR_MEASURE, bEnable);
        _side_bar->setItemEnabled(SIDEBAR_SEARCH, bEnable);
        _side_bar->setItemEnabled(SIDEBAR_FUNCTION, bEnable);
        _side_bar->setItemEnabled(SIDEBAR_OPTIONS, bEnable);
        _side_bar->setItemEnabled(SIDEBAR_RUNSTOP, true);
        _side_bar->setItemEnabled(SIDEBAR_INSTANT, true);

        if (_session->is_working() && mode == DSO) {
            if (_session->is_instant() == false) {
                _side_bar->setItemEnabled(SIDEBAR_TRIGGER, true);
                _side_bar->setItemEnabled(SIDEBAR_MEASURE, true);
                _side_bar->setItemEnabled(SIDEBAR_FUNCTION, true);
                _side_bar->setItemEnabled(SIDEBAR_OPTIONS, true);
            }
        }

        if (mode == LOGIC) {
            _side_bar->setItemVisible(SIDEBAR_TRIGGER, true);
            _side_bar->setItemVisible(SIDEBAR_DECODE, true);
            _side_bar->setItemVisible(SIDEBAR_MEASURE, true);
            _side_bar->setItemVisible(SIDEBAR_SEARCH, true);
            _side_bar->setItemVisible(SIDEBAR_FUNCTION, false);
            _side_bar->setItemVisible(SIDEBAR_OPTIONS, true);
            _side_bar->setItemVisible(SIDEBAR_RUNSTOP, true);
            _side_bar->setItemVisible(SIDEBAR_INSTANT, true);
        } else if (mode == ANALOG) {
            _side_bar->setItemVisible(SIDEBAR_TRIGGER, false);
            _side_bar->setItemVisible(SIDEBAR_DECODE, false);
            _side_bar->setItemVisible(SIDEBAR_MEASURE, true);
            _side_bar->setItemVisible(SIDEBAR_SEARCH, false);
            _side_bar->setItemVisible(SIDEBAR_FUNCTION, false);
            _side_bar->setItemVisible(SIDEBAR_OPTIONS, true);
            _side_bar->setItemVisible(SIDEBAR_RUNSTOP, true);
            _side_bar->setItemVisible(SIDEBAR_INSTANT, false);
        } else if (mode == DSO) {
            _side_bar->setItemVisible(SIDEBAR_TRIGGER, true);
            _side_bar->setItemVisible(SIDEBAR_DECODE, false);
            _side_bar->setItemVisible(SIDEBAR_MEASURE, true);
            _side_bar->setItemVisible(SIDEBAR_SEARCH, false);
            _side_bar->setItemVisible(SIDEBAR_FUNCTION, true);
            _side_bar->setItemVisible(SIDEBAR_OPTIONS, true);
            _side_bar->setItemVisible(SIDEBAR_RUNSTOP, true);
            _side_bar->setItemVisible(SIDEBAR_INSTANT, true);
        }
    }

    void MainWindow::OnMessage(int msg)
    {
        switch (msg)
        {
            case DSV_MSG_DEVICE_LIST_UPDATED:
            {
                _sampling_bar->update_device_list();
                break;
            }
            case DSV_MSG_START_COLLECT_WORK_PREV:
            {
                if (_device_agent->get_work_mode() == LOGIC)
                    _trigger_widget->try_commit_trigger();
                else if (_device_agent->get_work_mode() == DSO)
                    _dso_trigger_widget->check_setting();

                current_view()->capture_init();
                current_view()->on_state_changed(false);
                break;
            }
            case DSV_MSG_START_COLLECT_WORK:
            {
                update_toolbar_view_status();
                current_view()->on_state_changed(false);
                _protocol_widget->update_view_status();
                _device_options_widget->update_widgets_status();
                break;
            }        
            case DSV_MSG_CAPTURE_STATE_CHANGED:
            {
                update_toolbar_view_status();
                _device_options_widget->update_widgets_status();
                break;
            }
            case DSV_MSG_COLLECT_END:
            {
                prgRate(0);
                current_view()->repeat_unshow();
                current_view()->on_state_changed(true);                 
                break;
            }
            case DSV_MSG_END_COLLECT_WORK:
            {
                update_toolbar_view_status();
                _protocol_widget->update_view_status();

                pv::TabContext *ctx = current_context();
                if (ctx && ctx->document() && ctx->document()->has_pending_config()) {
                    ctx->document()->apply_pending_config(_session->get_device());
                    _device_options_widget->update_view();
                } else {
                    _device_options_widget->update_widgets_status();
                }
                break;
            }
            case DSV_MSG_CURRENT_DEVICE_CHANGE_PREV:
            {
                if (_msg != NULL){
                    _msg->close();
                    _msg = NULL;
                }
                current_view()->hide_calibration();

                _protocol_widget->del_all_protocol();
                current_view()->reload();
                break;
            }
            case DSV_MSG_CURRENT_DEVICE_CHANGED:
            {
                reset_all_view();
                load_device_config();
                update_title_bar_text();
                _sampling_bar->update_device_list();

                _logo_bar->dsl_connected(_session->get_device()->is_hardware());
                update_toolbar_view_status();
                _session->device_event_object()->device_updated();

                // Save signal config for current tab and rebuild signals
                {
                    pv::TabContext *ctx = current_context();
                    if (ctx && ctx->document()) {
                        ctx->document()->save_signal_config(_session->get_device());
                        current_view()->rebuild_signals();
                        dsv_info("DSV_MSG_CURRENT_DEVICE_CHANGED: saved config and rebuilt signals for current tab");
                    }
                }

                if (_device_agent->is_hardware())
                {
                    _session->on_load_config_end();
                }

                if (_device_agent->get_work_mode() == LOGIC && _device_agent->is_file() == false)
                    current_view()->auto_set_max_scale();

                if (_device_agent->is_file())
                {
                    check_config_file_version();

                    bool bDoneDecoder = false;
                    bool bLoadSuccess = false;
                    QJsonDocument doc = get_config_json_from_data_file(_device_agent->path(), bLoadSuccess);

                    if (bLoadSuccess){
                        load_config_from_json(doc, bDoneDecoder);
                    }

                    if (!bDoneDecoder && _device_agent->get_work_mode() == LOGIC)
                    {                    
                        QJsonArray deArray = get_decoder_json_from_data_file(_device_agent->path(), bLoadSuccess);

                        if (bLoadSuccess){
                            StoreSession ss(_session);
                            ss.load_decoders(_protocol_widget, deArray);
                        }                    
                    }

                    current_view()->update_all_trace_postion();                
                    QTimer::singleShot(100, this, [this](){
                        _session->start_capture(true);
                    });
                }
                else if (_device_agent->is_demo())
                {
                    if(_device_agent->get_work_mode() == LOGIC)
                    {
                        _pattern_mode = _device_agent->get_demo_operation_mode();
                        _protocol_widget->del_all_protocol();
                        current_view()->auto_set_max_scale();

                        if(_pattern_mode != "random"){
                        load_demo_decoder_config(_pattern_mode);
                        }
                    }
                }
        
                calc_min_height();

                if (_device_agent->is_hardware() && _device_agent->is_new_device()){
                    check_usb_device_speed();
                }

                break;
            }
            case DSV_MSG_DEVICE_OPTIONS_UPDATED:
            {
                _trigger_widget->device_updated();
                _device_options_widget->device_updated();
                _measure_widget->reload();
                current_view()->check_calibration();

                pv::TabContext *ctx = current_context();
                if (ctx && ctx->document()) {
                    ctx->document()->save_signal_config(_session->get_device());
                }

                current_view()->rebuild_signals();
                current_view()->signals_changed(NULL);
                break;
            }
            case DSV_MSG_DEVICE_DURATION_UPDATED:
            {
                _trigger_widget->device_updated();
                current_view()->timebase_changed();
                break;
            }
            case DSV_MSG_DEVICE_MODE_CHANGED:
            {
                current_view()->mode_changed(); 
                reset_all_view();
                load_device_config();
                update_title_bar_text();
                current_view()->hide_calibration();

                update_toolbar_view_status();
                _sampling_bar->update_sample_rate_list();

                if (_device_agent->is_hardware())
                    _session->on_load_config_end();
                
                if (_device_agent->get_work_mode() == LOGIC)
                    current_view()->auto_set_max_scale();

                if(_device_agent->is_demo())
                {
                    _pattern_mode = _device_agent->get_demo_operation_mode();
                    _protocol_widget->del_all_protocol();

                    if(_device_agent->get_work_mode() == LOGIC)
                    {
                        if(_pattern_mode != "random"){
                            _device_agent->update();
                            load_demo_decoder_config(_pattern_mode);
                        }
                    }
                }

                calc_min_height();           
                break;
            }
            case DSV_MSG_NEW_USB_DEVICE:
            {
                if (_msg != NULL){
                    _msg->close();
                    _msg = NULL;
                }

                _sampling_bar->update_device_list();

                //If the current device is working, do not remind to switch new device.
                if (_session->get_device()->is_hardware() && _session->is_working()){
                    return;
                }

                // If a saving task is running, not need to remind to switch device, 
                // when the task end, the new device will be selected.
                if (_session->get_device()->is_demo() == false && !_is_save_confirm_msg)
                {
                    QString msgText = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_TO_SWITCH_DEVICE), "To switch the new device?");
                    
                    if (MsgBox::Confirm(msgText, "", &_msg, NULL) == false){ 
                        _msg = NULL;
                        return;
                    }
                    _msg = NULL;
                }

                // The store confirm is not processed.
                if (_is_save_confirm_msg){
                    dsv_info("New device attached:Waitting for the confirm box be closed.");
                    _is_auto_switch_device = true; 
                    return;
                }

                if (_session->is_saving()){
                    dsv_info("New device attached:Waitting for store the data. and will switch to new device.");
                    _is_auto_switch_device = true;
                    return;
                }

                int mode = _device_agent->get_work_mode();

                if (mode != DSO && confirm_to_store_data())
                {
                    _is_auto_switch_device = true;

                    if (_session->is_working())
                        _session->stop_capture();

                    on_save();
                }
                else
                {   
                    if (_session->is_working())
                        _session->stop_capture();
                    
                    _session->set_default_device();
                }

                break;
            }
            case DSV_MSG_CURRENT_DEVICE_DETACHED:
            {
                if (_msg != NULL){
                    _msg->close();
                    _msg = NULL;
                }

                // Save current config, and switch to the last device.
                _session->device_event_object()->device_updated();
                save_config();
                current_view()->hide_calibration();

                if (_session->is_saving()){
                    dsv_info("Device detached:Waitting for store the data. and will switch to new device.");
                    _is_auto_switch_device = true;
                    return;
                }

                if (confirm_to_store_data()){
                    _is_auto_switch_device = true;
                    on_save();
                }
                else{
                    _session->set_default_device();
                }
                break;
            }
            case DSV_MSG_SAVE_COMPLETE:
            {
                _session->clear_store_confirm_flag();

                if (_is_auto_switch_device)
                {
                    _is_auto_switch_device = false;
                    _session->set_default_device();
                }
                else
                {
                    ds_device_handle devh = _sampling_bar->get_next_device_handle();
                    if (devh != NULL_HANDLE)
                    {
                        dsv_info("Auto switch to the selected device.");
                        _session->set_device(devh);
                    }
                }
                break;
            }
            case DSV_MSG_CLEAR_DECODE_DATA:
            {
                if (_device_agent->get_work_mode() == LOGIC)
                    _protocol_widget->reset_view();
                break;
            }            
            case DSV_MSG_STORE_CONF_PREV:
            {
                if (_device_agent->is_hardware() && _session->have_hardware_data() == false){
                    _sampling_bar->commit_settings();
                }
                break;
            }
            case DSV_MSG_BEGIN_DEVICE_OPTIONS:
            case DSV_MSG_COLLECT_MODE_CHANGED:
            {
                if(_device_agent->is_demo()){
                    _pattern_mode = _device_agent->get_demo_operation_mode();
                }
                if (msg == DSV_MSG_COLLECT_MODE_CHANGED){
                    _trigger_widget->device_updated();
                    current_view()->update();
                }           
                break;
            }
            case DSV_MSG_END_DEVICE_OPTIONS:
            case DSV_MSG_DEMO_OPERATION_MODE_CHNAGED:
            {
                if(_device_agent->is_demo() &&_device_agent->get_work_mode() == LOGIC){                
                    QString pattern_mode = _device_agent->get_demo_operation_mode();       
                    
                    if(pattern_mode != _pattern_mode)
                    {
                        _pattern_mode = pattern_mode; 

                        _device_agent->update();
                        _session->clear_view_data();
                        _session->init_signals();
                        update_toolbar_view_status();
                        _sampling_bar->update_sample_rate_list();
                        _protocol_widget->del_all_protocol();
                            
                        if(_pattern_mode != "random"){
                            _session->set_collect_mode(COLLECT_SINGLE);
                            load_demo_decoder_config(_pattern_mode);

                            if (msg == DSV_MSG_END_DEVICE_OPTIONS)
                                _session->start_capture(false); // Auto load data.
                        }
                    }                
                }
                calc_min_height();            
                break;
            }
            case DSV_MSG_APP_OPTIONS_CHANGED:
            {
                update_title_bar_text();
                break;
            }
            case DSV_MSG_FONT_OPTIONS_CHANGED:
            {
                UiManager::Instance()->Update(UI_UPDATE_ACTION_FONT);          
                break;
            }
            case DSV_MSG_DATA_POOL_CHANGED:
            {
                current_view()->check_measure();
                break;
            }            
        }
    }

    void MainWindow::calc_min_height()
    {
        if (_frame != NULL)
        {
            if (_device_agent->get_work_mode() == LOGIC)
            {
                int ch_num = _session->get_ch_num(-1);
                int win_height = Base_Height + Per_Chan_Height * ch_num;

                if (win_height < Min_Height)
                    _frame->setMinimumHeight(win_height);
                else
                    _frame->setMinimumHeight(Min_Height);
            }
            else{
                _frame->setMinimumHeight(Min_Height);
            }
        }  
    }

    void MainWindow::delay_prop_msg(QString strMsg)
    {
        _strMsg = strMsg;
        if (_strMsg != ""){
            _delay_prop_msg_timer.Start(500);
        }
    }

    void MainWindow::on_delay_prop_msg()
    {
        _delay_prop_msg_timer.Stop();

        if (_strMsg != ""){
            MsgBox::Show("", _strMsg, this, &_msg);
            _msg = NULL;
        }            
    }

    void MainWindow::update_title_bar_text()
    {
          // Set the title
        QString title = QApplication::applicationName() + " v" + QApplication::applicationVersion();
        AppConfig &app = AppConfig::Instance();

        if (_title_ext_string != "" && app.appOptions.displayProfileInBar){
            title += " [" + _title_ext_string + "]";
        }

        if (_lst_title_string != title){
            _lst_title_string = title;

            setWindowTitle(QApplication::translate("MainWindow", title.toLocal8Bit().data(), 0));
            _title_bar->setTitle(this->windowTitle());
        }        
    }

    void MainWindow::load_demo_decoder_config(QString optname)
    { 
        QString file = GetAppDataDir() + "/demo/logic/" + optname + ".demo";
        bool bLoadSurccess = false;

        QJsonArray deArray = get_decoder_json_from_data_file(file, bLoadSurccess);

        if (bLoadSurccess){
            StoreSession ss(_session);
            ss.load_decoders(_protocol_widget, deArray);
        }

        QJsonDocument doc = get_config_json_from_data_file(file, bLoadSurccess);
        if (bLoadSurccess){
            load_channel_view_indexs(doc);
        }
        
        current_view()->update_all_trace_postion();
    }

    QWidget* MainWindow::GetBodyView()
    {
        return current_view();
    }

    pv::view::View* MainWindow::current_view()
    {
        if (_current_tab_index >= 0 && _current_tab_index < _tab_contexts.size()) {
            return _tab_contexts[_current_tab_index]->view();
        }
        return nullptr;
    }

    pv::TabContext* MainWindow::current_context()
    {
        if (_current_tab_index >= 0 && _current_tab_index < _tab_contexts.size()) {
            return _tab_contexts[_current_tab_index];
        }
        return nullptr;
    }

    void MainWindow::add_tab(pv::TabContext *ctx)
    {
        pv::view::View *view = ctx->view();
        _tab_contexts.append(ctx);
        _tab_widget->addTab(view, ctx->title());
        _tab_widget->setCurrentIndex(_tab_widget->count() - 1);
        update_tab_style(_tab_widget->count() - 1);
    }

    void MainWindow::remove_tab(int index)
    {
        if (index < 0 || index >= _tab_contexts.size())
            return;

        if (_tab_contexts.size() <= 1)
            return;

        pv::TabContext *ctx = _tab_contexts[index];
        if (ctx->is_live() && _session->is_working()) {
            _session->stop_capture();
        }

        if (_session->get_active_document() == ctx->document()) {
            _session->set_active_document(nullptr);
        }

        _tab_contexts.removeAt(index);
        disconnect(_tab_widget, &pv::ui::DraggableTabWidget::currentChanged, this, &MainWindow::on_tab_changed);
        _tab_widget->removeTab(index);
        _session->unregister_document(ctx->document());
        ctx->view()->deleteLater();
        SessionManager::instance()->destroy_context(ctx);

        if (_current_tab_index >= _tab_contexts.size()) {
            _current_tab_index = _tab_contexts.size() - 1;
        } else if (index < _current_tab_index) {
            _current_tab_index--;
        }

        _tab_contexts[_current_tab_index]->activate();
        _tab_widget->setCurrentIndex(_current_tab_index);
        update_tab_style(_current_tab_index);

        pv::TabContext *new_ctx = _tab_contexts[_current_tab_index];
        _sampling_bar->bind_context(new_ctx);
        _measure_widget->bind_context(new_ctx);
        _search_widget->bind_context(new_ctx);
        _protocol_widget->bind_context(new_ctx);
        _device_options_widget->bind_context(new_ctx);
        _trigger_widget->bind_context(new_ctx);
        _dso_trigger_widget->bind_context(new_ctx);

        pv::view::View *view = current_view();
        if (view) {
            _sampling_bar->set_context(_session, view);
            _sampling_bar->set_readonly(false);
            _sampling_bar->set_view(view);
            _measure_widget->set_view(view);
            _search_widget->set_view(view);
            _protocol_widget->set_view(view);
            view->installEventFilter(this);
        }

        connect(_tab_widget, &pv::ui::DraggableTabWidget::currentChanged, this, &MainWindow::on_tab_changed);
    }

    void MainWindow::update_tab_style(int index)
    {
        if (index < 0 || index >= _tab_contexts.size())
            return;

        pv::TabContext *ctx = _tab_contexts[index];
        QString title = ctx->title();

        QColor liveColor = AppConfig::Instance().GetThemeColor("@tab-status-live");
        QColor dataColor = AppConfig::Instance().GetThemeColor("@tab-status-data");
        QColor emptyColor = AppConfig::Instance().GetThemeColor("@tab-status-empty");
        if (!liveColor.isValid()) liveColor = QColor(76, 175, 80);
        if (!dataColor.isValid()) dataColor = QColor(200, 200, 200);
        if (!emptyColor.isValid()) emptyColor = QColor(120, 120, 120);

        if (ctx->is_live()) {
            _tab_widget->setTabText(index, title);
            _tab_widget->tabBar()->setTabTextColor(index, liveColor);
        } else if (ctx->has_data()) {
            _tab_widget->setTabText(index, title);
            _tab_widget->tabBar()->setTabTextColor(index, dataColor);
        } else {
            _tab_widget->setTabText(index, title);
            _tab_widget->tabBar()->setTabTextColor(index, emptyColor);
        }
    }

    void MainWindow::on_tab_changed(int index)
    {
        if (index < 0 || index >= _tab_contexts.size())
            return;

        int old_index = _current_tab_index;
        dsv_info("MainWindow::on_tab_changed(%d) old=%d", index, old_index);

        if (old_index >= 0 && old_index < _tab_contexts.size() && old_index != index) {
            _tab_contexts[old_index]->deactivate();
            update_tab_style(old_index);
        }

        _current_tab_index = index;
        _tab_contexts[index]->activate();
        update_tab_style(index);

        pv::view::View *view = current_view();
        if (view) {
            if (old_index >= 0 && old_index < _tab_contexts.size() && old_index != index) {
                _sampling_bar->unbind_context();
                _measure_widget->unbind_context();
                _search_widget->unbind_context();
                _protocol_widget->unbind_context();
                _device_options_widget->unbind_context();
                _trigger_widget->unbind_context();
                _dso_trigger_widget->unbind_context();
            }

            pv::TabContext *new_ctx = _tab_contexts[index];
            _sampling_bar->bind_context(new_ctx);
            _measure_widget->bind_context(new_ctx);
            _search_widget->bind_context(new_ctx);
            _protocol_widget->bind_context(new_ctx);
            _device_options_widget->bind_context(new_ctx);
            _trigger_widget->bind_context(new_ctx);
            _dso_trigger_widget->bind_context(new_ctx);

            view->installEventFilter(this);
        }

        update_title_bar_text();
        SessionManager::instance()->set_active_context(_tab_contexts[index]);
    }

    void MainWindow::on_tab_detach(int index, QWidget *widget, const QString &title)
    {
        (void)index;
        (void)title;

        pv::TabContext *ctx = nullptr;
        for (auto c : _tab_contexts) {
            if (c->view() == widget) {
                ctx = c;
                break;
            }
        }

        if (ctx) {
            if (ctx->is_live()) {
                ctx->deactivate();
            }
            _tab_contexts.removeOne(ctx);
            if (_current_tab_index >= _tab_contexts.size()) {
                _current_tab_index = _tab_contexts.size() - 1;
            }
            if (!_tab_contexts.isEmpty()) {
                _tab_contexts[_current_tab_index]->activate();
                update_tab_style(_current_tab_index);
            }
            SessionManager::instance()->detach_context(ctx);
            ctx->view()->setProperty("detached_ctx", QVariant::fromValue((quintptr)ctx));
        }
    }

    void MainWindow::on_tab_attached(QWidget *widget, const QString &title)
    {
        (void)title;
        QVariant prop = widget->property("detached_ctx");
        if (!prop.isValid() || prop.isNull())
            return;

        pv::TabContext *ctx = reinterpret_cast<pv::TabContext*>(prop.value<quintptr>());
        if (!ctx)
            return;

        _tab_contexts.append(ctx);
        SessionManager::instance()->attach_context(ctx);
        widget->setProperty("detached_ctx", QVariant());
    }

    void MainWindow::on_new_tab_requested()
    {
        pv::view::View *new_view = new pv::view::View(_session, _sampling_bar, this);
        pv::data::SessionDocument *new_doc = new pv::data::SessionDocument();

        if (_device_agent && _device_agent->have_instance()) {
            new_doc->save_signal_config(_device_agent);
            dsv_info("MainWindow::on_new_tab_requested() saved signal config, mode=%d ch_count=%d",
                new_doc->get_signal_config().work_mode,
                (int)new_doc->get_signal_config().channels.size());
        }

        pv::TabContext *new_ctx = SessionManager::instance()->create_context(new_view, _session, new_doc);
        _session->register_document(new_doc);
        new_ctx->set_title(QString::fromUtf8("标签%1").arg(_tab_contexts.size() + 1));
        add_tab(new_ctx);
    }   
  
} // namespace pv
