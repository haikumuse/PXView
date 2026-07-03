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


#ifndef PXVIEW_PV_MAINWINDOW_H
#define PXVIEW_PV_MAINWINDOW_H

#include <list>
#include <vector>
#include <QMainWindow>
#include <QTranslator> 
#include "dialogs/dsmessagebox.h"
#include "interface/icallbacks.h"
#include "interface/events.h"
#include "eventobject.h"
#include <QJsonDocument>
#include <chrono>
#include <QTimer>
#include "dstimer.h"

#include <QWidgetAction>
#include<QShortcut>
#include "ui/draggabletabwidget.h"
#include "tabcontext.h"
#include "widgets/slidingdrawer.h"
#include "widgets/sidebar.h"
#include "config/appconfig.h"
#include "api/types.h"

class QAction;
class QMenu;
class QMenuBar;
class QVBoxLayout;
class QStatusBar;
class QToolBar;
class QWidget;
class QDockWidget;
class QLabel;
class AppControl;
class DeviceAgent;

using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

namespace pv {

class SigSession;

namespace toolbars {
class SamplingBar;
class TrigBar;
class FileBar;
class LogoBar;
class TitleBar;
}

namespace dock{
class ProtocolDock;
class TriggerDock;
class DsoTriggerDock;
class MeasureDock;
class SearchDock;
class DeviceOptionsDock;
class LogDock;
class SignalProcessingDock;
class McpControlDock;
}

namespace view {
class View;
}

// removed WidgetInspector fwd decl

//The mainwindow,referenced by MainFrame
//TODO: create graph view,toolbar,and show device list
class MainWindow :
    public QMainWindow,
    public IDataCallback,
    public ICaptureCallback,
    public ITriggerCallback,
    public ISessionStateCallback,
    public IMainForm,
    public ISessionDataGetter,
    public IMessageListener,
    public pv::api::IServiceEventListener,
    public pv::interface::IEventListener
{
	Q_OBJECT

public:
    static const int Min_Width  = 350;
    static const int Min_Height = 300;
    static const int Base_Height = 150;
    static const int Per_Chan_Height = 35;
     
public:
    explicit MainWindow(toolbars::TitleBar *title_bar, QWidget *parent = 0);
    ~MainWindow();

    void openDoc();

public slots: 
    void switchTheme(QString style);
    void restore_dock();

private slots:
	void on_load_file(QString file_name);
    void on_open_doc();  
    void on_side_bar_dock_clicked(int index);
    void on_side_bar_action_clicked(int index);
    void on_screenShot();
    void on_save();

    void on_export();
    bool on_load_session(QString name);  
    bool on_store_session(QString name); 
    void on_data_updated();
 
    void on_session_error();
    void on_signals_changed();
    void on_receive_trigger(quint64 trigger_pos);
    void on_frame_ended();
    void on_frame_began();
    void on_decode_done();
    void on_receive_data_len(quint64 len);
    void on_cur_snap_samplerate_changed();
    void on_trigger_message(int msg);
    void on_delay_prop_msg();
    void on_load_device_first();
    void on_tab_changed(int index);
    void on_tab_moved(int from, int to);
    void on_tab_detach(int index, QWidget *widget, const QString &title);
    void on_tab_attached(QWidget *widget, const QString &title);
    void on_new_tab_requested();

    // Task 1.3: ICaptureCallback methods now emit EventObject signals (cross-
    // thread safe); these on_* slots run on the GUI thread to touch the View.
    void on_update_capture();
    void on_show_region(quint64 start, quint64 end, bool keep);
    void on_show_wait_trigger();
    void on_repeat_hold(int percent);
  
signals:
    void prgRate(int progress);

public:
    //IMainForm
    void switchLanguage(int language) override;
    bool able_to_close();    
    QWidget* GetBodyView();
    
private: 
	void setup_ui();
    void retranslateUi(); 
    bool eventFilter(QObject *object, QEvent *event);
    int resolveShortcutAction(int key, int modifiers);
    void check_usb_device_speed();
    void reset_all_view();
    bool confirm_to_store_data();
    void update_toolbar_view_status();
    void calc_min_height();    
    void update_title_bar_text();
    void update_disk_cache_status();
    void update_fps();

    pv::view::View* current_view();
    pv::TabContext* current_context();
    void add_tab(pv::TabContext *ctx);
    void remove_tab(int index);
    void update_tab_style(int index);

    //json operation
private:
    QString gen_config_file_path(bool isNewFormat);
    bool load_config_from_file(QString file);
    bool load_config_from_json(QJsonDocument &doc, bool &haveDecoder);
    void load_device_config();
    bool gen_config_json(QJsonObject &sessionVar);
    void save_config();
    bool save_config_to_file(QString file);
    QJsonDocument get_config_json_from_data_file(QString file, bool &bSucesss);
    QJsonArray get_decoder_json_from_data_file(QString file, bool &bSucesss);
    void check_config_file_version(); 
    void load_demo_decoder_config(QString optname);

  
private:
    //IDataCallback
    void data_updated() override;
    void receive_data_len(quint64 len) override;
    void receive_header() override;
    void cur_snap_samplerate_changed() override;

    //ICaptureCallback
    void frame_began() override;
    void frame_ended() override;
    void update_capture() override;
    void show_region(uint64_t start, uint64_t end, bool keep) override;
    void repeat_hold(int percent) override;

    //ITriggerCallback
    void receive_trigger(quint64 trigger_pos) override;
    void show_wait_trigger() override;
    void trigger_message(int msg) override;

    //ISessionStateCallback
    void session_error() override;
    void session_save() override;
    void signals_changed() override;
    void decode_done() override;
    void delay_prop_msg(QString strMsg) override;

    //ISessionDataGetter
    bool genSessionData(std::string &str) override;

    //IMessageListener
    void OnMessage(int msg, int param = 0) override;

    // C5 fix: per-responsibility handlers dispatched by OnMessage. Each handler
    // owns the switch cases for its message group; OnMessage is now a thin
    // router. The (int msg, int param) signature is preserved so handlers can
    // differentiate fall-through cases (e.g. END_DEVICE_OPTIONS vs
    // DEMO_OPERATION_MODE_CHNAGED) and use the param payload (e.g.
    // CAPTURE_OWNER_CHANGED's is_working flag).
    // NOTE: on_data_updated(int,int) intentionally overloads the no-arg Qt slot
    // on_data_updated() (connected to EventObject::data_updated); the connect
    // site uses QOverload<>::of(...) to disambiguate.
    void on_device_changed(int msg, int param);
    void on_capture_state(int msg, int param);
    void on_device_options(int msg, int param);
    void on_ui_options(int msg, int param);
    void on_data_updated(int msg, int param);
    void on_filter_completed(int msg, int param);
    void on_trigger_changed(int msg, int param);

    //IServiceEventListener — receive View operation broadcasts from SessionService
    //(show_region, zoom_fit, zoom_in/out, cursor operations). In GUI mode these are
    //routed to the active View; in Headless mode there is no MainWindow so these
    //events are simply not consumed.
    void on_service_event(const pv::api::ServiceEventData &data) override;

    //IEventListener (Task 8) — typed event bus consumer. All 41 event structs
    //have a typed override below. Overrides that correspond to a former OnMessage
    //case dispatch to the per-responsibility handler (on_device_changed /
    //on_capture_state / on_device_options / on_ui_options / on_data_updated /
    //on_filter_completed / on_trigger_changed). Overrides for events with no
    //OnMessage counterpart (CopyToDocDone/DecodeDone/SignalsChanged/DataUpdated/
    //DeviceConfigUpdated) are empty. CaptureOwnerChanged is also empty because
    //its is_working flag is carried by the legacy (int param) path and is not
    //present in the typed event struct — that case is retained in OnMessage.
    void on_event(const pv::interface::CaptureStateChanged &) override;
    void on_event(const pv::interface::CaptureOwnerChanged &) override;
    void on_event(const pv::interface::TriggerConfigChanged &) override;
    void on_event(const pv::interface::SampleCountUpdated &) override;
    void on_event(const pv::interface::DeviceOptionsUpdated &) override;
    void on_event(const pv::interface::ActiveDocumentChanged &) override;
    void on_event(const pv::interface::CopyToDocDone &) override;
    void on_event(const pv::interface::DecodeDone &) override;
    void on_event(const pv::interface::SignalsChanged &) override;
    void on_event(const pv::interface::DataUpdated &) override;
    void on_event(const pv::interface::DeviceModeChanged &) override;
    void on_event(const pv::interface::CollectModeChanged &) override;
    void on_event(const pv::interface::DeviceListUpdated &) override;
    void on_event(const pv::interface::CurrentDeviceChanged &) override;
    void on_event(const pv::interface::UsbDeviceArrived &) override;
    void on_event(const pv::interface::DeviceDetached &) override;
    void on_event(const pv::interface::SampleRateChanged &) override;
    void on_event(const pv::interface::SaveComplete &) override;
    void on_event(const pv::interface::StartCollectWork &) override;
    void on_event(const pv::interface::CollectStart &) override;
    void on_event(const pv::interface::CollectEnd &) override;
    void on_event(const pv::interface::EndCollectWork &) override;
    void on_event(const pv::interface::EndDeviceOptions &) override;
    void on_event(const pv::interface::DeviceConfigUpdated &) override;
    void on_event(const pv::interface::DemoModeChanged &) override;
    void on_event(const pv::interface::DataPoolChanged &) override;
    void on_event(const pv::interface::SimpleTriggerChanged &) override;
    void on_event(const pv::interface::GlitchFilterStarted &) override;
    void on_event(const pv::interface::GlitchFilterProgress &) override;
    void on_event(const pv::interface::GlitchFilterCompleted &) override;
    void on_event(const pv::interface::GlitchFilterCleared &) override;
    void on_event(const pv::interface::SignalInvertStarted &) override;
    void on_event(const pv::interface::SignalInvertCompleted &) override;
    void on_event(const pv::interface::SignalInvertCleared &) override;
    void on_event(const pv::interface::CopyInProgressChanged &) override;
    void on_event(const pv::interface::TrigNextCollect &) override;
    void on_event(const pv::interface::ClearDecodeData &) override;
    void on_event(const pv::interface::AppOptionsChanged &) override;
    void on_event(const pv::interface::FontOptionsChanged &) override;
    void on_event(const pv::interface::ShortcutChanged &) override;
    void on_event(const pv::interface::StyleChanged &) override;

private: 
	pv::ui::DraggableTabWidget *_tab_widget;
    QList<pv::TabContext*> _tab_contexts;
    int _current_tab_index;
    dialogs::DSMessageBox   *_msg;

	QWidget                 *_central_widget;
	QVBoxLayout             *_vertical_layout;

	toolbars::SamplingBar   *_sampling_bar;
    toolbars::TrigBar       *_trig_bar;
    toolbars::FileBar       *_file_bar;
    toolbars::LogoBar       *_logo_bar; //help button, on top right
    toolbars::TitleBar      *_title_bar;


    QDockWidget             *_protocol_dock;
    dock::ProtocolDock      *_protocol_widget;
    QDockWidget             *_trigger_dock;
    QDockWidget             *_dso_trigger_dock;
    dock::TriggerDock       *_trigger_widget;
    dock::DsoTriggerDock    *_dso_trigger_widget;
    QDockWidget             *_measure_dock;
    dock::MeasureDock       *_measure_widget;
    QDockWidget             *_search_dock;
    dock::SearchDock        *_search_widget;
    QDockWidget             *_device_options_dock;
    dock::DeviceOptionsDock *_device_options_widget;
    QDockWidget             *_log_dock;
    dock::LogDock           *_log_widget;
    QDockWidget             *_signal_processing_dock;
    dock::SignalProcessingDock *_signal_processing_widget;
    dock::McpControlDock       *_mcp_control_widget;

    // Sliding drawer panel
    widgets::SlidingDrawer  *_sliding_drawer;
    int _drawer_page_protocol;
    int _drawer_page_trigger;
    int _drawer_page_dso_trigger;
    int _drawer_page_measure;
    int _drawer_page_search;
    int _drawer_page_device_options;
    int _drawer_page_signal_processing;
    int _drawer_page_log;
    int _drawer_page_mcp;
    int _drawer_current_page; // -1 = no page open

    QTranslator     _qtTrans;
    QTranslator     _myTrans;
    EventObject     _event; 
    SigSession      *_session;
    DeviceAgent     *_device_agent;
    bool            _is_auto_switch_device;
    high_resolution_clock::time_point _last_key_press_time;
    bool            _is_save_confirm_msg;
    QString         _pattern_mode;
    QWidget         *_frame;
    DsTimer         _delay_prop_msg_timer;
    QString         _strMsg;
    QString         _lst_title_string;
    QString         _title_ext_string;

    QLabel          *_disk_cache_status_label;
    QLabel          *_trig_time_label;
    QLabel          *_fps_label;
    QLabel          *_sample_period_label;
    void update_sample_period();
    QTimer          _disk_cache_status_timer;
    QTimer          _fps_timer;
    int             _acq_count;

    int         _key_value;
    bool        _key_vaild;
    // removed _debug_helper


    void MainWindowRibbonHelper();
    void Ribbon_setupUi();
    void Ribbon_retranslateUi();
    void setupQuickAccessBar();
    void setupSideBar();
    void setupFileCategory();
    void setupDisplayCategory();
    void setupHelpCategory();

    widgets::SideBar* _side_bar;

    enum {
        SIDEBAR_TRIGGER = 0,
        SIDEBAR_DECODE = 1,
        SIDEBAR_MEASURE = 2,
        SIDEBAR_SEARCH = 3,
        SIDEBAR_FUNCTION = 4,
        SIDEBAR_OPTIONS = 5,
        SIDEBAR_SIGNAL_PROCESSING = 6,
        SIDEBAR_MCP = 7,
        SIDEBAR_LOG = 8,
        SIDEBAR_RUNSTOP = 9,
        SIDEBAR_INSTANT = 10
    };

    ::DockOptions* getDockOptions();

    int _category_file_index;
    int _category_display_index;
    int _category_help_index;

    QMenuBar      *_menu_bar;
    QMenu         *_category_file;
    QMenu         *_category_display;
    QMenu         *_category_help;

};

} // namespace pv

#endif // PXVIEW_PV_MAINWINDOW_H
