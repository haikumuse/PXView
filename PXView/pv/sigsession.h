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
#include <algorithm>
#include <list>
#include <set>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include "data/analogsnapshot.h"
#include "data/datasource.h"
#include "data/dsosnapshot.h"
#include "data/lissajousmodel.h"
#include "data/logicsnapshot.h"
#include "data/mathstack.h"
#include "data/sessiondocument.h"
#include "data/sessionsnapshot.h"
#include "data/signalmodel.h"
#include "deviceagent.h"
#include "dstimer.h"
#include "eventobject.h"
#include "interface/icallbacks.h"
#include <libsigrok.h>

struct srd_decoder;
struct srd_channel;
class DecoderStatus;

typedef std::lock_guard<std::mutex> ds_lock_guard;

namespace pv {

namespace data {
class SignalData;
class Snapshot;
class AnalogSnapshot;
class DsoSnapshot;
class LogicSnapshot;
class DecoderModel;
class MathStack;

namespace decode {
class Decoder;
}
} // namespace data

enum DEVICE_STATUS_TYPE {
  ST_INIT = 0,
  ST_RUNNING = 1,
  ST_STOPPED = 2,
};

enum DEVICE_COLLECT_MODE {
  COLLECT_SINGLE = 0,
  COLLECT_REPEAT = 1,
  COLLECT_LOOP = 2,
};

class SessionData {
public:
  SessionData();

  inline data::LogicSnapshot *get_logic() { return &logic; }

  inline data::AnalogSnapshot *get_analog() { return &analog; }

  inline data::DsoSnapshot *get_dso() { return &dso; }

  void clear();

public:
  uint64_t _cur_snap_samplerate;
  uint64_t _cur_samplelimits;
  uint64_t _trig_pos;
  data::LogicSnapshot *_logic_backup;
  bool _glitch_filter_active;
  std::vector<uint32_t> _glitch_filter_thresholds;
  std::vector<GlitchFilterMode> _glitch_filter_modes;
  bool _signal_invert_active;
  std::vector<bool> _signal_invert_channels;

private:
  data::LogicSnapshot logic;
  data::AnalogSnapshot analog;
  data::DsoSnapshot dso;
};

using namespace pv::data;

// created by MainWindow
class SigSession : public IMessageListener,
                   public IDeviceAgentCallback,
                   public pv::data::DataSource {
private:
  static constexpr float Oversampling = 2.0f;

public:
  static const int RefreshTime = 500;
  static const int RepeatHoldDiv = 20;
  static const int FeedInterval = 50;
  static const int WaitShowTime = 500;

  enum SESSION_ERROR_STATUS {
    No_err,
    Hw_err,
    Malloc_err,
    Test_timeout_err,
    Pkt_data_err,
    Data_overflow
  };

private:
  SigSession(SigSession &o);

public:
  explicit SigSession();

  ~SigSession();

  inline DeviceAgent *get_device() { return &_device_agent; }

  void add_callback(ISessionCallbackBase *callback) { _callbacks.push_back(callback); }
  void remove_callback(ISessionCallbackBase *callback);
  // Deprecated: use add_callback instead
  void set_callback(ISessionCallbackBase *callback) { add_callback(callback); }

  bool init();
  void uninit();
  void Open();
  void Close();

  bool set_default_device();
  bool set_device(ds_device_handle dev_handle);
  bool set_file(QString name);
  void close_file(ds_device_handle dev_handle);
  bool start_capture(bool instant = false,
                     data::SessionDocument *owner = nullptr);
  bool stop_capture();
  bool switch_work_mode(int mode);

  uint64_t cur_samplerate();
  uint64_t cur_snap_samplerate() override;
  uint64_t cur_samplelimits() override;
  double cur_sampletime() override;
  double cur_snap_sampletime() override;
  double cur_view_time();

  inline bool re_start() {
    if (_is_working)
      stop_capture();
    return start_capture(_is_instant);
  }

  inline QDateTime get_session_time() { return _session_time; }

  inline QDateTime get_trig_time() { return _trig_time; }

  inline bool is_triged() { return _is_triged; }

  inline uint64_t get_trigger_pos() override { return _view_data->_trig_pos; }

  bool is_first_store_confirm();
  bool get_capture_status(bool &triggered, int &progress);

  inline void clear_store_confirm_flag() {
    _confirm_store_time_id = _work_time_id;
  }

  std::vector<data::SignalModel *> &get_signal_models() override;

  bool add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus,
                   std::list<pv::data::decode::Decoder *> &sub_decoders,
                   data::DecoderStack *&out_stack,
                   data::SessionDocument *doc = nullptr);
  int get_trace_index_by_key_handel(void *handel,
                                    data::SessionDocument *doc = nullptr);
  void remove_decoder(int index, data::SessionDocument *doc = nullptr);
  void remove_decoder_by_key_handel(void *handel,
                                    data::SessionDocument *doc = nullptr);

  std::vector<data::DecoderStack *> &get_decoder_stacks(
      data::SessionDocument *doc = nullptr) override;

  void rst_decoder(int index, data::SessionDocument *doc = nullptr);
  void rst_decoder_by_key_handel(void *handel,
                                 data::SessionDocument *doc = nullptr);

  inline pv::data::DecoderModel *get_decoder_model() override {
    return _decoder_model;
  }

  inline std::vector<data::SpectrumStack *> &get_spectrum_stacks() override {
    return _spectrum_stacks;
  }

  inline data::LissajousModel *get_lissajous_model() override {
    return _lissajous_model;
  }

  inline data::MathStack *get_math_stack() override { return _math_stack; }

  uint16_t get_ch_num(int type);

  inline bool is_data_lock() { return _data_lock; }

  void data_auto_lock(int lock);
  void data_auto_unlock();
  bool get_data_auto_lock();
  void spectrum_rebuild();
  void lissajous_rebuild(bool enable, int xindex, int yindex, double percent);
  void lissajous_disable();

  void math_rebuild(bool enable, int ch1_index, int ch2_index,
                    data::MathStack::MathType type);

  inline bool trigd() { return _trigger_flag; }
  inline uint8_t trigd_ch() { return _trigger_ch; }
  inline void set_trigger_preconfigured(bool v) { _trigger_preconfigured = v; }
  inline bool is_trigger_preconfigured() { return _trigger_preconfigured; }

  data::Snapshot *get_snapshot(int type) override;

  data::LogicSnapshot *get_logic_snapshot() override;
  data::AnalogSnapshot *get_analog_snapshot() override;
  data::DsoSnapshot *get_dso_snapshot() override;

  inline SESSION_ERROR_STATUS get_error() { return _error; }

  inline void set_error(SESSION_ERROR_STATUS state) { _error = state; }

  void clear_error();

  inline uint64_t get_error_pattern() { return _error_pattern; }

  inline double get_repeat_intvl() { return _repeat_intvl; }

  inline void set_repeat_intvl(double interval) { _repeat_intvl = interval; }

  int get_repeat_hold();

  inline void set_save_start(uint64_t start) { _save_start = start; }

  inline uint64_t get_save_start() { return _save_start; }

  inline void set_save_end(uint64_t end) { _save_end = end; }

  inline uint64_t get_save_end() { return _save_end; }

  void clear_all_decoder(bool bUpdateView = true);

  inline bool is_closed() { return _bClose; }

  inline bool is_instant() { return _is_instant; }

  inline bool is_working() {
    return _is_working || _device_status == ST_RUNNING;
  }

  inline bool is_init_status() { return _device_status == ST_INIT; }

  // The collect thread is running.
  inline bool is_running_status() { return _device_status == ST_RUNNING; }

  inline bool is_stopped_status() { return _device_status == ST_STOPPED; }

  void set_collect_mode(DEVICE_COLLECT_MODE m);

  inline int get_collect_mode() { return (int)_clt_mode; }

  inline bool is_repeat_mode() { return _clt_mode == COLLECT_REPEAT; }

  inline bool is_single_mode() { return _clt_mode == COLLECT_SINGLE; }

  inline bool is_loop_mode() { return _clt_mode == COLLECT_LOOP; }

  bool is_realtime_refresh();

  inline bool is_repeating() {
    return _clt_mode == COLLECT_REPEAT && !_is_instant;
  }

  inline void session_save() {
    dispatch_to<ISessionStateCallback>(
        [](ISessionStateCallback *cb) { cb->session_save(); });
  }

  inline void show_region(uint64_t start, uint64_t end, bool keep) {
    dispatch_to<ICaptureCallback>(
        [start, end, keep](ICaptureCallback *cb) {
          cb->show_region(start, end, keep);
        });
  }

  inline void decode_done() {
    dispatch_to<ISessionStateCallback>(
        [](ISessionStateCallback *cb) { cb->decode_done(); });
  }

  inline bool is_saving() { return _is_saving; }

  inline void set_saving(bool flag) { _is_saving = flag; }

  inline DeviceEventObject *device_event_object() { return &_device_event; }

  void reload();
  void refresh(int holdtime);
  void check_update();

  inline void set_map_zoom(int index) { _map_zoom = index; }

  inline int get_map_zoom() { return _map_zoom; }

  inline bool is_single_buffer() { return _view_data == _capture_data; }

  inline void update_view() {
    dispatch_to<IDataCallback>([](IDataCallback *cb) { cb->data_updated(); });
  }

  void auto_end();
  bool have_hardware_data();
  struct ds_device_base_info *get_device_list(int &out_count,
                                              int &actived_index);
  void add_msg_listener(IMessageListener *ln);
  void broadcast_msg(int msg);
  bool have_new_realtime_refresh(bool keep);
  data::DecoderStack *get_decoder_trace(int index,
                                        data::SessionDocument *doc = nullptr);
  data::SignalModel *get_signal_by_index(int index);

  inline bool have_view_data() { return get_signal_snapshot()->have_data(); }
  inline bool is_copy_in_progress() const { return _copy_in_progress; }
  inline data::SessionDocument *get_capture_owner_document() const { return _capture_owner_document; }

  void on_load_config_end();
  void init_signals();

  inline bool is_doing_action() { return _is_action; }

  void clear_view_data();
  void set_trace_name(data::SignalModel *model, QString name);
  void set_decoder_row_label(int index, QString label);

  inline void set_decoder_pannel(IDecoderPannel *pannel) {
    _decoder_pannel = pannel;
  }

  void rebuild_decoder_pannel() {
    if (_decoder_pannel)
      _decoder_pannel->rebuild_layers();
  }

  void update_dso_data_scale();

  void add_decode_task(data::DecoderStack *stack);
  void remove_decode_task(data::DecoderStack *stack);

  inline sr_status get_dso_status() { return _dso_status; }

  inline bool dso_status_is_valid() { return _dso_status_valid; }

  double get_logic_data_view_time();

  int64_t get_ring_sample_count();

  inline bool dso_data_is_out_off_range() {
    return _view_data->get_dso()->data_is_out_off_range();
  }

  void set_active_document(data::SessionDocument *doc);
  data::SessionDocument *get_active_document() { return _active_document; }
  void copy_data_to_document(data::SessionDocument *doc);
  void attach_data_to_signal(SessionData *data);

  void register_document(data::SessionDocument *doc) {
    _all_documents.push_back(doc);
  }
  void unregister_document(data::SessionDocument *doc) {
    auto it = std::find(_all_documents.begin(), _all_documents.end(), doc);
    if (it != _all_documents.end())
      _all_documents.erase(it);
  }
  void clear_all_documents_decoders();

  inline std::vector<data::DecoderStack *> &decode_traces(
      data::SessionDocument *doc = nullptr) {
    return get_decoder_stacks(doc);
  }

  void update_lang_text();

  bool have_decoded_result();

  void apply_samplerate();

  void set_glitch_filter(const std::vector<uint32_t> &thresholds,
                         const std::vector<GlitchFilterMode> &filter_modes = {});
  void clear_glitch_filter();
  bool is_glitch_filter_active();

  void set_signal_invert(const std::vector<bool> &channels);
  void clear_signal_invert();
  bool is_signal_invert_active();

  void restart_decoders();
  // Re-attaches _view_data snapshots to SignalModels (which may have been
  // recreated by reload() with NULL pointers) and starts decode tasks on all
  // decoder stacks. Centralizes the "start decoders after data is ready" logic
  // so every code path (capture end, copy-to-doc done, signal invert, etc.)
  // consistently ensures SignalModels have valid snapshot pointers.
  void start_all_decode_tasks();

  size_t get_disk_write_queue_depth();
  double get_disk_write_speed_mbps();
  bool is_disk_write_disk_full();

private:
  void set_cur_samplelimits(uint64_t samplelimits);
  void set_cur_snap_samplerate(uint64_t samplerate);
  void math_disable();

  bool exec_capture();
  void exit_capture();

  // Dispatch helper: invokes fn on every registered callback that
  // implements the given sub-interface (using dynamic_cast).
  template <typename Iface, typename F> void dispatch_to(F fn) {
    for (auto *cb : _callbacks) {
      if (auto *iface = dynamic_cast<Iface *>(cb))
        fn(iface);
    }
  }

  // IDataCallback dispatch helpers
  inline void data_updated() {
    dispatch_to<IDataCallback>([](IDataCallback *cb) { cb->data_updated(); });
  }

  inline void set_receive_data_len(quint64 len) {
    dispatch_to<IDataCallback>(
        [len](IDataCallback *cb) { cb->receive_data_len(len); });
  }

  inline void receive_header() {
    dispatch_to<IDataCallback>([](IDataCallback *cb) { cb->receive_header(); });
  }

  inline void cur_snap_samplerate_changed() {
    dispatch_to<IDataCallback>(
        [](IDataCallback *cb) { cb->cur_snap_samplerate_changed(); });
  }

  // ICaptureCallback dispatch helpers
  inline void frame_began() {
    dispatch_to<ICaptureCallback>([](ICaptureCallback *cb) { cb->frame_began(); });
  }

  inline void frame_ended() {
    dispatch_to<ICaptureCallback>([](ICaptureCallback *cb) { cb->frame_ended(); });
  }

  inline void update_capture() {
    dispatch_to<ICaptureCallback>(
        [](ICaptureCallback *cb) { cb->update_capture(); });
  }

  inline void repeat_hold(int percent) {
    dispatch_to<ICaptureCallback>(
        [percent](ICaptureCallback *cb) { cb->repeat_hold(percent); });
  }

  // ITriggerCallback dispatch helpers
  inline void receive_trigger(quint64 trigger_pos) {
    dispatch_to<ITriggerCallback>(
        [trigger_pos](ITriggerCallback *cb) {
          cb->receive_trigger(trigger_pos);
        });
  }

  inline void show_wait_trigger() {
    dispatch_to<ITriggerCallback>(
        [](ITriggerCallback *cb) { cb->show_wait_trigger(); });
  }

  inline void trigger_message(int msg) {
    // R5: dispatch to GUI first (MainWindow::trigger_message via EventObject
    // Qt signal), then broadcast to all IMessageListeners (including
    // SigSession itself) so Core-side OnMessage handlers run without relying
    // on the GUI to forward the message. This keeps headless mode working.
    dispatch_to<ITriggerCallback>(
        [msg](ITriggerCallback *cb) { cb->trigger_message(msg); });
    broadcast_msg(msg);
  }

  // ISessionStateCallback dispatch helpers
  inline void signals_changed() {
    dispatch_to<ISessionStateCallback>(
        [](ISessionStateCallback *cb) { cb->signals_changed(); });
  }

  inline void session_error() {
    dispatch_to<ISessionStateCallback>(
        [](ISessionStateCallback *cb) { cb->session_error(); });
  }

  inline void delay_prop_msg(QString strMsg) {
    dispatch_to<ISessionStateCallback>(
        [strMsg](ISessionStateCallback *cb) { cb->delay_prop_msg(strMsg); });
  }

  void clear_all_decode_task(int &runningDex);

  inline void clear_all_decode_task2() {
    int run_dex = 0;
    clear_all_decode_task(run_dex);
  }

  void decode_single_task(data::DecoderStack *task);

  void capture_init();
  void nodata_timeout();
  void feed_timeout();
  void clear_decode_result();

  bool action_start_capture(bool instant,
                            data::SessionDocument *owner = nullptr);
  bool action_stop_capture();

  inline void set_session_time(QDateTime time) { _session_time = time; }

  // IMessageListener
  void OnMessage(int msg) override;

  // IDeviceAgentCallback
  void DeviceConfigChanged() override;

private:
  /**
   * Attempts to autodetect the format. Failing that
   * @param filename The filename of the input file.
   * @return A pointer to the 'struct sr_input_format' that should be
   * 	used, or NULL if no input format was selected or
   * 	auto-detected.
   */
  static sr_input_format *
  determine_input_file_format(const std::string &filename);

  // data feed
  void feed_in_header(const sr_dev_inst *sdi);
  void feed_in_meta(const sr_dev_inst *sdi, const sr_datafeed_meta &meta);
  void feed_in_trigger(const ds_trigger_pos &trigger_pos);
  void feed_in_logic(const sr_datafeed_logic &o);

  void feed_in_dso(const sr_datafeed_dso &o);
  void feed_in_analog(const sr_datafeed_analog &o);
  void data_feed_in(const struct sr_dev_inst *sdi,
                    const struct sr_datafeed_packet *packet);

  static void data_feed_callback_ex(const struct sr_dev_inst *sdi,
                                    const struct sr_datafeed_packet *packet,
                                    void *user_data);

  static void device_lib_event_callback_ex(int event, void *user_data);

  void on_device_lib_event(int event);
  Snapshot *get_signal_snapshot();
  void repeat_capture_wait_timeout();
  void repeat_wait_prog_timeout();
  void realtime_refresh_timeout();
  void trig_check_timeout();

  void clear_signals();

  void glitch_filter_task(const std::vector<uint32_t> thresholds,
                          const std::vector<GlitchFilterMode> filter_modes);
  void signal_invert_task(const std::vector<bool> channels);

  inline void data_lock() { _data_lock = true; }

  inline void data_unlock() { _data_lock = false; }

  data::SignalModel *get_channel_by_index(int orgIndex);
  void make_channels_view_index(int start_dex = -1);

private:
  mutable std::mutex _sampling_mutex;
  mutable std::mutex _data_mutex;
  mutable std::mutex _running_tasks_mutex;
  std::vector<std::thread> _decode_threads;
  std::vector<data::DecoderStack *> _running_tasks;

  std::vector<data::SignalModel *> _signal_models;
  static std::vector<data::DecoderStack *> _empty_decoder_stacks;
  pv::data::DecoderModel *_decoder_model;
  std::vector<data::SpectrumStack *> _spectrum_stacks;
  data::LissajousModel *_lissajous_model = nullptr;
  data::MathStack *_math_stack = nullptr;

  DiskCacheConfig _disk_cache_config;

  DsTimer _feed_timer;
  DsTimer _out_timer;
  DsTimer _repeat_timer;
  DsTimer _repeat_wait_prog_timer;
  DsTimer _refresh_rt_timer;
  DsTimer _trig_check_timer;

  int _noData_cnt;
  bool _data_lock;
  bool _data_updated;
  int _data_auto_lock;

  QDateTime _session_time;
  QDateTime _trig_time;
  bool _is_triged;
  bool _trigger_flag;
  uint8_t _trigger_ch;
  bool _trigger_preconfigured;  // set by MCP/API when trigger is configured externally
  bool _hw_replied;

  SESSION_ERROR_STATUS _error;
  uint64_t _error_pattern;
  int _map_zoom;
  bool _bClose;

  uint64_t _save_start;
  uint64_t _save_end;
  volatile bool _is_working;
  double _repeat_intvl; // The progress wait timer interval.
  int _repeat_hold_prg; // The time sleep progress
  int _repeat_wait_prog_step;
  bool _is_saving;
  bool _is_instant;
  volatile int _device_status;
  int _work_time_id;
  int _capture_times;
  int _confirm_store_time_id;
  uint64_t _rt_refresh_time_id;
  uint64_t _rt_ck_refresh_time_id;
  DEVICE_COLLECT_MODE _clt_mode;
  bool _is_stream_mode;

  bool _is_action;
  uint64_t _dso_packet_count;

  std::vector<ISessionCallbackBase*> _callbacks;
  DeviceAgent _device_agent;
  std::vector<IMessageListener *> _msg_listeners;
  DeviceEventObject _device_event;
  SessionData *_view_data;
  SessionData *_capture_data;
  data::SessionDocument *_active_document;
  std::vector<data::SessionDocument *> _all_documents;
  std::vector<SessionData *> _data_list;
  IDecoderPannel *_decoder_pannel;
  sr_status _dso_status;
  bool _dso_status_valid;

  std::thread *_glitch_filter_thread;
  bool _glitch_filter_running;
  std::thread *_signal_invert_thread;
  bool _signal_invert_running;
  volatile bool _copy_in_progress;
  data::SessionDocument *_capture_owner_document;
};

} // namespace pv

#endif // PXVIEW_PV_SIGSESSION_H
