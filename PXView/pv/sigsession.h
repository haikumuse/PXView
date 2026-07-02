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

#ifndef PXVIEW_PV_SIGSESSION_H
#define PXVIEW_PV_SIGSESSION_H

#include <QDateTime>
#include <QString>
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>
#include "data/analogsnapshot.h"
#include "data/datasource.h"
#include "data/dsosnapshot.h"
#include "data/logicsnapshot.h"
#include "data/mathstack.h"
#include "data/signalmodel.h"
#include "data/triggerconfig.h"
#include "deviceagent.h"
#include "dsvdef.h"
#include "eventobject.h"
#include "interface/icallbacks.h"
#include "core/eventbus.h"
#include "core/capturemanager.h"
#include <libsigrok.h>

struct srd_decoder;
struct srd_channel;
class DecoderStatus;
typedef std::lock_guard<std::mutex> ds_lock_guard;

namespace pv {
namespace data {
class SignalData; class Snapshot; class LissajousModel; class SessionDocument;
class DecoderStack; class SpectrumStack;
namespace decode { class Decoder; }
} // namespace data
namespace core {
class FilterProcessor; class DecodeTaskManager; class DataFeedParser;
class DocumentRegistry; class CaptureManager;
} // namespace core

class SessionData {
public:
  SessionData();
  data::LogicSnapshot *get_logic() { return &logic; }
  data::AnalogSnapshot *get_analog() { return &analog; }
  data::DsoSnapshot *get_dso() { return &dso; }
  void clear();
  uint64_t _cur_snap_samplerate, _cur_samplelimits, _trig_pos;
  data::LogicSnapshot *_logic_backup;
  bool _glitch_filter_active, _signal_invert_active;
  bool _glitch_filter_auto_apply = false;  // 采集后自动重新应用滤波
  std::vector<uint32_t> _glitch_filter_thresholds;
  std::vector<GlitchFilterMode> _glitch_filter_modes;
  std::vector<bool> _signal_invert_channels;
private:
  data::LogicSnapshot logic; data::AnalogSnapshot analog; data::DsoSnapshot dso;
};

using namespace pv::data;

class SigSession : public IMessageListener, public IDeviceAgentCallback, public pv::data::DataSource {
  friend class core::FilterProcessor; friend class core::DecodeTaskManager;
  friend class core::DataFeedParser; friend class core::DocumentRegistry; friend class core::CaptureManager;
private:
  static constexpr float Oversampling = 2.0f;
  SigSession(SigSession &o);
public:
  static const int RefreshTime = 500, RepeatHoldDiv = 20, FeedInterval = 50, WaitShowTime = 500;
  enum SESSION_ERROR_STATUS { No_err, Hw_err, Malloc_err, Test_timeout_err, Pkt_data_err, Data_overflow };
  explicit SigSession();
  ~SigSession();
  DeviceAgent *get_device() { return &_device_agent; }
  void add_callback(ISessionCallbackBase *callback) { _event_bus->add_callback(callback); }
  void remove_callback(ISessionCallbackBase *callback) { _event_bus->remove_callback(callback); }
  void set_callback(ISessionCallbackBase *callback) { add_callback(callback); }
  bool init(); void uninit(); void Open(); void Close();
  bool set_default_device(); bool set_device(ds_device_handle dev_handle);
  bool set_file(QString name); void close_file(ds_device_handle dev_handle);
  bool start_capture(bool instant = false, data::SessionDocument *owner = nullptr) { return _capture_manager->start_capture(instant, owner); }
  bool stop_capture() { return _capture_manager->stop_capture(); }
  bool switch_work_mode(int mode);
  uint64_t cur_samplerate();
  uint64_t cur_snap_samplerate() override;
  uint64_t cur_samplelimits() override;
  double cur_sampletime() override;
  double cur_snap_sampletime() override;
  double cur_view_time();
  bool re_start() { if (_is_working) stop_capture(); return start_capture(_capture_manager->is_instant()); }
  QDateTime get_session_time() { return _session_time; }
  QDateTime get_trig_time() { return _trig_time; }
  bool is_triged() { return _is_triged; }
  uint64_t get_trigger_pos() override { return _view_data->_trig_pos; }
  bool is_first_store_confirm() { return _capture_manager->is_first_store_confirm(); }
  bool get_capture_status(bool &triggered, int &progress) { return _capture_manager->get_capture_status(triggered, progress); }
  void clear_store_confirm_flag() { _capture_manager->clear_store_confirm_flag(); }
  std::vector<std::shared_ptr<data::SignalModel>> &get_signal_models() override;
  bool add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus, std::list<pv::data::decode::Decoder *> &sub_decoders, std::shared_ptr<data::DecoderStack> &out_stack, data::SessionDocument *doc = nullptr);
  int get_trace_index_by_key_handel(void *handel, data::SessionDocument *doc = nullptr);
  void remove_decoder(int index, data::SessionDocument *doc = nullptr);
  void remove_decoder_by_key_handel(void *handel, data::SessionDocument *doc = nullptr);
  std::vector<std::shared_ptr<data::DecoderStack>> &get_decoder_stacks(data::SessionDocument *doc = nullptr) override;
  void rst_decoder(int index, data::SessionDocument *doc = nullptr);
  void rst_decoder_by_key_handel(void *handel, data::SessionDocument *doc = nullptr);
  std::vector<std::shared_ptr<data::SpectrumStack>> &get_spectrum_stacks() override { return _spectrum_stacks; }
  data::LissajousModel *get_lissajous_model() override { return _lissajous_model; }
  std::shared_ptr<data::MathStack> get_math_stack() override { return _math_stack; }
  uint16_t get_ch_num(int type);
  bool is_data_lock() { return _capture_manager->is_data_lock(); }
  void data_lock() { _capture_manager->data_lock(); }
  void data_unlock() { _capture_manager->data_unlock(); }
  void data_auto_lock(int lock) { _capture_manager->data_auto_lock(lock); }
  void data_auto_unlock() { _capture_manager->data_auto_unlock(); }
  bool get_data_auto_lock() { return _capture_manager->get_data_auto_lock(); }
  void spectrum_rebuild();
  void lissajous_rebuild(bool enable, int xindex, int yindex, double percent);
  void lissajous_disable();
  void math_rebuild(bool enable, int ch1_index, int ch2_index, data::MathStack::MathType type);
  bool trigd() { return _trigger_flag; }
  uint8_t trigd_ch() { return _trigger_ch; }
  data::Snapshot *get_snapshot(int type) override;
  data::LogicSnapshot *get_logic_snapshot() override;
  data::AnalogSnapshot *get_analog_snapshot() override;
  data::DsoSnapshot *get_dso_snapshot() override;
  SESSION_ERROR_STATUS get_error() { return _error; }
  void set_error(SESSION_ERROR_STATUS state) { _error = state; }
  void clear_error();
  uint64_t get_error_pattern() { return _error_pattern; }
  double get_repeat_intvl() { return _capture_manager->get_repeat_intvl(); }
  void set_repeat_intvl(double interval) { _capture_manager->set_repeat_intvl(interval); }
  int get_repeat_hold() { return _capture_manager->get_repeat_hold(); }
  void set_save_start(uint64_t start) { _save_start = start; }
  uint64_t get_save_start() { return _save_start; }
  void set_save_end(uint64_t end) { _save_end = end; }
  uint64_t get_save_end() { return _save_end; }
  void clear_all_decoder(bool bUpdateView = true);
  bool is_closed() { return _bClose; }
  bool is_instant() { return _capture_manager->is_instant(); }
  bool is_working() { return _is_working || _device_status == ST_RUNNING; }
  bool is_init_status() { return _device_status == ST_INIT; }
  bool is_running_status() { return _device_status == ST_RUNNING; }
  bool is_stopped_status() { return _device_status == ST_STOPPED; }
  void set_collect_mode(DEVICE_COLLECT_MODE m) { _capture_manager->set_collect_mode(m); }
  int get_collect_mode() { return _capture_manager->get_collect_mode(); }
  bool is_repeat_mode() { return _capture_manager->is_repeat_mode(); }
  bool is_single_mode() { return _capture_manager->is_single_mode(); }
  bool is_loop_mode() { return _capture_manager->is_loop_mode(); }
  bool is_realtime_refresh() { return _capture_manager->is_realtime_refresh(); }
  bool is_repeating() { return _capture_manager->is_repeating(); }
  void session_save() { dispatch_to<ISessionStateCallback>([](ISessionStateCallback *cb) { cb->session_save(); }); }
  void show_region(uint64_t start, uint64_t end, bool keep) { dispatch_to<ICaptureCallback>([start, end, keep](ICaptureCallback *cb) { cb->show_region(start, end, keep); }); }
  void decode_done() { dispatch_to<ISessionStateCallback>([](ISessionStateCallback *cb) { cb->decode_done(); }); }
  bool is_saving() { return _is_saving; }
  void set_saving(bool flag) { _is_saving = flag; }
  DeviceEventObject *device_event_object() { return &_device_event; }
  void reload();
  void refresh(int holdtime) { _capture_manager->refresh(holdtime); }
  void check_update() { _capture_manager->check_update(); }
  void set_map_zoom(int index) { _map_zoom = index; }
  int get_map_zoom() { return _map_zoom; }
  bool is_single_buffer() { return _view_data == _capture_data; }
  void update_view() { dispatch_to<IDataCallback>([](IDataCallback *cb) { cb->data_updated(); }); }
  void auto_end() { _capture_manager->auto_end(); }
  bool have_hardware_data();
  struct ds_device_base_info *get_device_list(int &out_count, int &actived_index);
  void add_msg_listener(IMessageListener *ln) { _event_bus->add_msg_listener(ln); }
  void broadcast_msg(int msg, int param = 0) { _event_bus->broadcast_msg(msg, param); }
  void add_event_listener(interface::IEventListener *l) { _event_bus->add_event_listener(l); }
  void remove_event_listener(interface::IEventListener *l) { _event_bus->remove_event_listener(l); }
  template <typename EventType> void broadcast(const EventType &ev) { _event_bus->broadcast(ev); }
  bool have_new_realtime_refresh(bool keep) { return _capture_manager->have_new_realtime_refresh(keep); }
  std::shared_ptr<data::DecoderStack> get_decoder_trace(int index, data::SessionDocument *doc = nullptr);
  std::shared_ptr<data::SignalModel> get_signal_by_index(int index);
  bool have_view_data() { return get_signal_snapshot()->have_data(); }
  bool is_copy_in_progress() const;
  data::SessionDocument *get_capture_owner_document() const;
  void clear_capture_owner_document(data::SessionDocument *doc);
  void join_copy_thread();
  void on_load_config_end();
  void init_signals();
  bool is_doing_action() { return _capture_manager->is_action(); }
  void clear_view_data();
  void set_trace_name(std::shared_ptr<data::SignalModel> model, QString name);
  void set_decoder_row_label(int index, QString label);
  void set_decoder_pannel(IDecoderPannel *pannel) { _decoder_pannel = pannel; }
  void rebuild_decoder_pannel() { if (_decoder_pannel) _decoder_pannel->rebuild_layers(); }
  void update_dso_data_scale();
  void remove_decode_task(std::shared_ptr<data::DecoderStack> stack);
  sr_status get_dso_status() { return _dso_status; }
  bool dso_status_is_valid() { return _dso_status_valid; }
  double get_logic_data_view_time();
  int64_t get_ring_sample_count();
  bool dso_data_is_out_off_range() { return _view_data->get_dso()->data_is_out_off_range(); }
  void set_active_document(data::SessionDocument *doc);
  data::SessionDocument *get_active_document();
  void copy_data_to_document(data::SessionDocument *doc);
  void attach_data_to_signal(SessionData *data);
  const data::TriggerConfig& trigger_config() const { return _trigger_config; }
  void set_trigger_config(const data::TriggerConfig& cfg);
  void register_document(data::SessionDocument *doc);
  void unregister_document(data::SessionDocument *doc);
  void clear_all_documents_decoders();
  std::vector<std::shared_ptr<data::DecoderStack>> &decode_traces(data::SessionDocument *doc = nullptr) { return get_decoder_stacks(doc); }
  void update_lang_text();
  bool have_decoded_result();
  void apply_samplerate();
  void set_glitch_filter(const std::vector<uint32_t> &thresholds, const std::vector<GlitchFilterMode> &filter_modes = {});
  void clear_glitch_filter();
  bool is_glitch_filter_active();
  // Per-channel glitch filter state (Task 9 / I4): public read accessors for
  // the current thresholds/modes so the View layer can snapshot prior state
  // before applying a new filter, then restore it via set_glitch_filter() on
  // undo_filter(). Returns references to the view-data vectors; callers must
  // copy if they need a stable snapshot. Safe to call from the GUI thread.
  const std::vector<uint32_t>& glitch_filter_thresholds() const { return _view_data->_glitch_filter_thresholds; }
  const std::vector<GlitchFilterMode>& glitch_filter_modes() const { return _view_data->_glitch_filter_modes; }
  // 采集后自动重新应用滤波(保留上次阈值/模式)
  void set_glitch_filter_auto_apply(bool en) { _view_data->_glitch_filter_auto_apply = en; }
  bool glitch_filter_auto_apply() const { return _view_data->_glitch_filter_auto_apply; }
  // 新采集开始时清除滤波状态(不恢复数据,因为数据已被 clear)
  void clear_glitch_filter_state_for_capture();
  void set_signal_invert(const std::vector<bool> &channels);
  void clear_signal_invert();
  bool is_signal_invert_active();
  void restart_decoders();
  void start_all_decode_tasks();
  size_t get_disk_write_queue_depth();
  double get_disk_write_speed_mbps();
  bool is_disk_write_disk_full();
private:
  void set_cur_samplelimits(uint64_t samplelimits); void set_cur_snap_samplerate(uint64_t samplerate);
  void math_disable(); void sync_trigger_to_libsigrok();
  template <typename Iface, typename F> void dispatch_to(F fn) { _event_bus->dispatch_to<Iface>(fn); }
  void data_updated(); void set_receive_data_len(quint64 len); void receive_header();
  void cur_snap_samplerate_changed(); void frame_began(); void frame_ended();
  void update_capture(); void repeat_hold(int percent);
  void receive_trigger(quint64 trigger_pos); void show_wait_trigger();
  void trigger_message(int msg); void signals_changed(); void session_error();
  void delay_prop_msg(QString strMsg);
  void clear_all_decode_task(int &runningDex); void clear_all_decode_task2();
  void add_decode_task(std::shared_ptr<data::DecoderStack> stack);
  void set_session_time(QDateTime time) { _session_time = time; }
  void OnMessage(int msg, int param = 0) override; void DeviceConfigChanged() override;
  static sr_input_format *determine_input_file_format(const std::string &filename);
  static void device_lib_event_callback_ex(int event, void *user_data); void on_device_lib_event(int event);
  data::Snapshot *get_signal_snapshot(); void clear_signals();
  std::shared_ptr<data::SignalModel> get_channel_by_index(int orgIndex);
  void make_channels_view_index(int start_dex = -1);
  mutable std::mutex _sampling_mutex, _data_mutex;
  std::vector<std::shared_ptr<data::SignalModel>> _signal_models;
  static std::vector<std::shared_ptr<data::DecoderStack>> _empty_decoder_stacks;
  std::vector<std::shared_ptr<data::SpectrumStack>> _spectrum_stacks;
  data::LissajousModel *_lissajous_model = nullptr;
  std::shared_ptr<data::MathStack> _math_stack = nullptr;
  QDateTime _session_time, _trig_time;
  bool _is_triged, _trigger_flag, _hw_replied, _bClose, _is_saving, _dso_status_valid;
  uint8_t _trigger_ch; SESSION_ERROR_STATUS _error;
  uint64_t _error_pattern, _save_start, _save_end;
  int _map_zoom;
  std::atomic<bool> _is_working; std::atomic<int> _device_status;
  std::atomic<uint64_t> _next_decoder_handle_id{1};
  DeviceAgent _device_agent;
  std::unique_ptr<core::EventBus> _event_bus;
  DeviceEventObject _device_event;
  SessionData *_view_data, *_capture_data;
  std::vector<SessionData *> _data_list;
  IDecoderPannel *_decoder_pannel;
  sr_status _dso_status;
  std::unique_ptr<core::FilterProcessor> _filter_processor;
  std::unique_ptr<core::DecodeTaskManager> _decode_task_manager;
  std::unique_ptr<core::DataFeedParser> _data_feed_parser;
  std::unique_ptr<core::DocumentRegistry> _document_registry;
  std::unique_ptr<core::CaptureManager> _capture_manager;
  data::TriggerConfig _trigger_config;
};

} // namespace pv
#endif // PXVIEW_PV_SIGSESSION_H
