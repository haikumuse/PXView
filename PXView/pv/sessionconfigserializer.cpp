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

// Task 20 (purify-architecture-concepts): the .pxc session config serialization
// orchestration (gen_config_json / load_config_from_json / save_config_to_file)
// was extracted verbatim from MainWindow into this class. MainWindow now holds a
// unique_ptr<SessionConfigSerializer> and forwards the three calls. Method
// bodies are unchanged except that implicit this->member access has been
// rewritten to _main_window->accessor() / _session->. The build_channel_colours
// file-scope helper moved here alongside its only caller (gen_config_json).

#include "sessionconfigserializer.h"

#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTextStream>
#include <cassert>
#include <map>

#include "config/appconfig.h"
#include "data/sessiondocument.h"
#include "data/triggerconfig.h"
#include "deviceagent.h"
#include "dock/protocoldock.h"
#include "dock/triggerdock.h"
#include "dsvdef.h"
#include "log.h"
#include "sigsession.h"
#include "storesession.h"
#include "tabcontext.h"
#include "toolbars/samplingbar.h"
#include "ui/langresource.h"
#include "ui/msgbox.h"
#include "utility/encoding.h"
#include "utility/path.h"
#include "view/signal.h"
#include "view/view.h"
#include "view/viewstatus.h"

namespace pv {

namespace {
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
      colours[sig->get_index()] =
          c.isValid() ? c.name().toStdString() : "default";
    }
  }
  return colours;
}
} // namespace

SessionConfigSerializer::SessionConfigSerializer(MainWindow *main_window,
                                                 SigSession *session)
    : _main_window(main_window), _session(session) {}

SessionConfigSerializer::~SessionConfigSerializer() {}

bool SessionConfigSerializer::gen_config_json(QJsonObject &sessionVar) {
  AppConfig &app = AppConfig::Instance();

  GVariant *gvar_opts;
  GVariant *gvar;
  gsize num_opts;

  QString title = QApplication::applicationName() + " v" +
                  QApplication::applicationVersion();

  sessionVar["Version"] = QJsonValue::fromVariant(SESSION_FORMAT_VERSION);
  sessionVar["Device"] =
      QJsonValue::fromVariant(_main_window->device_agent()->driver_name());
  sessionVar["DeviceMode"] =
      QJsonValue::fromVariant(_main_window->device_agent()->get_work_mode());
  sessionVar["Language"] = QJsonValue::fromVariant(app.frameOptions.language);
  sessionVar["Title"] = QJsonValue::fromVariant(title);

  if (_main_window->device_agent()->is_hardware() &&
      _main_window->device_agent()->get_work_mode() == LOGIC) {
    sessionVar["CollectMode"] = _session->get_collect_mode();
  }

  gvar_opts = _main_window->device_agent()->get_config_list(
      NULL, SR_CONF_DEVICE_SESSIONS);
  if (gvar_opts == NULL) {
    pxv_warn("Device config list is empty. id:SR_CONF_DEVICE_SESSIONS");
    /* Driver supports no device instance sessions. */
    return false;
  }

  const int *const options = (const int32_t *)g_variant_get_fixed_array(
      gvar_opts, &num_opts, sizeof(int32_t));

  for (unsigned int i = 0; i < num_opts; i++) {
    const struct sr_config_info *const info =
        _main_window->device_agent()->get_config_info(options[i]);
    gvar = _main_window->device_agent()->get_config(info->key);
    if (gvar != NULL) {
      if (info->datatype == SR_T_BOOL)
        sessionVar[info->name] =
            QJsonValue::fromVariant(g_variant_get_boolean(gvar));
      else if (info->datatype == SR_T_UINT64)
        sessionVar[info->name] = QJsonValue::fromVariant(
            QString::number(g_variant_get_uint64(gvar)));
      else if (info->datatype == SR_T_UINT8)
        sessionVar[info->name] =
            QJsonValue::fromVariant(g_variant_get_byte(gvar));
      else if (info->datatype == SR_T_INT16)
        sessionVar[info->name] =
            QJsonValue::fromVariant(g_variant_get_int16(gvar));
      else if (info->datatype == SR_T_FLOAT) // save as string format
        sessionVar[info->name] = QJsonValue::fromVariant(
            QString::number(g_variant_get_double(gvar)));
      else if (info->datatype == SR_T_CHAR)
        sessionVar[info->name] =
            QJsonValue::fromVariant(g_variant_get_string(gvar, NULL));
      else if (info->datatype == SR_T_LIST)
        sessionVar[info->name] =
            QJsonValue::fromVariant(g_variant_get_int16(gvar));
      else {
        pxv_err("Unkown config info type:%d", info->datatype);
        assert(false);
        g_variant_unref(gvar);
        continue;
      }
      g_variant_unref(gvar);
    }
  }

  // Task 3 (purify-architecture-concepts): channel 段改为通过 SignalConfigStore
  // 序列化（单一序列化路径），不再直访 view::Signal。先调用 save_signal_config
  // 从当前 device + View 状态填充 _signal_config，再 signal_config_to_json 产出
  // channels[] 数组。顶层 key 仍为 "channel"（单数）以保持 .pxc 外层结构不变；
  // 数组内字段统一使用 ChannelConfig 字段名（不保留 strigger/trigValue/zeroPos/
  // mapUnit/mapMin/mapMax/mapDefault/colour/type/name/vfactor 等 MainWindow 旧 key）。
  pv::TabContext *ctx = _main_window->current_context();
  pv::data::SessionDocument *doc = ctx ? ctx->document() : nullptr;
  if (doc) {
    doc->save_signal_config(_session->get_signal_models(),
                            build_channel_colours(_main_window->current_view()));
    QJsonObject sig_cfg = doc->signal_config_to_json();
    sessionVar["channel"] = sig_cfg["channels"].toArray();
  } else {
    pxv_warn("MainWindow::gen_config_json: no active document, writing empty "
             "channel array");
    sessionVar["channel"] = QJsonArray();
  }

  if (_main_window->device_agent()->get_work_mode() == LOGIC) {
    // Task 6 (purify-architecture-concepts): trigger 序列化改走 Core
    // TriggerConfig（唯一真相源），不再调用 _trigger_widget->get_session()
    // 经 View 层产出旧 JSON key。to_json() 写入 mode/trigger_pos/stage_count/
    // stages[]/adv_enabled/adv_tab_index/serial_* 新结构。
    sessionVar["trigger"] = _session->trigger_config().to_json();
  }

  StoreSession ss(_session);
  QJsonArray decodeJson;
  ss.gen_decoders_json(decodeJson);
  sessionVar["decoder"] = decodeJson;

  if (_main_window->device_agent()->get_work_mode() == DSO) {
    sessionVar["measure"] =
        _main_window->current_view()->get_viewstatus()->get_session();
  }

  // Task 16 (purify-architecture-concepts): persist per-channel UI layout
  // (view_index/v_offset/own_height/visible) as a separate top-level
  // "uiLayout" segment, parallel to "channel"/"trigger"/"decoder". This is a
  // View-layer concern serialized independently by view::View — Core's
  // ChannelConfig no longer holds these fields (Task 15 migrated them to
  // pv::view::DockUiState::channel_layouts). Placing the call here in
  // gen_config_json covers both save paths (save_config_to_file +
  // genSessionData). Explicit null guard: assert is a no-op in Release.
  if (auto *view = _main_window->current_view()) {
    sessionVar["uiLayout"] = view->save_ui_layout_to_json();
  } else {
    sessionVar["uiLayout"] = QJsonArray();
  }

  if (gvar_opts != NULL)
    g_variant_unref(gvar_opts);

  return true;
}

bool SessionConfigSerializer::load_config_from_json(QJsonDocument &doc,
                                                    bool &haveDecoder) {
  haveDecoder = false;

  // DeviceConfigChanged broadcasts are now ASYNC (queued on qApp via
  // Qt::QueuedConnection by EventBus), so the previous
  // SuppressConfigBroadcastGuard (which prevented nested reload ->
  // signals_changed -> View AllReplaced deleting the DsoSignal mid-method) is
  // no longer needed: the caller's stack frame completes before any listener
  // processes the message. Device config is still written; reload() at the
  // end rebuilds from it.

  QJsonObject sessionObj = doc.object();

  int mode = _main_window->device_agent()->get_work_mode();

  // check config file version
  if (!sessionObj.contains("Version")) {
    pxv_dbg("Profile version is not exists!");
    return false;
  }

  int format_ver = sessionObj["Version"].toInt();

  if (format_ver < 2) {
    pxv_err("Profile version is error!");
    return false;
  }

  if (sessionObj.contains("CollectMode") &&
      _main_window->device_agent()->is_hardware()) {
    int collect_mode = sessionObj["CollectMode"].toInt();
    _session->set_collect_mode((DEVICE_COLLECT_MODE)collect_mode);
  }

  int conf_dev_mode = sessionObj["DeviceMode"].toInt();

  if (_main_window->device_agent()->is_hardware()) {
    QString driverName = _main_window->device_agent()->driver_name();
    QString sessionDevice = sessionObj["Device"].toString();
    // check device and mode
    if (driverName != sessionDevice || mode != conf_dev_mode) {
      MsgBox::Show(
          NULL,
          L_S(STR_PAGE_MSG, S_ID(IDS_MSG_PROFILE_NOT_COMPATIBLE),
              "Profile is not compatible with current device or mode!"),
          _main_window);
      return false;
    }
  }

  // load device settings
  GVariant *gvar_opts =
      _main_window->device_agent()->get_config_list(NULL, SR_CONF_DEVICE_SESSIONS);
  gsize num_opts;

  if (gvar_opts != NULL) {
    const int *const options = (const int32_t *)g_variant_get_fixed_array(
        gvar_opts, &num_opts, sizeof(int32_t));

    for (unsigned int i = 0; i < num_opts; i++) {
      const struct sr_config_info *info =
          _main_window->device_agent()->get_config_info(options[i]);

      if (!sessionObj.contains(info->name))
        continue;

      GVariant *gvar = NULL;
      int id = 0;

      if (info->datatype == SR_T_BOOL) {
        gvar = g_variant_new_boolean(sessionObj[info->name].toInt());
      } else if (info->datatype == SR_T_UINT64) {
        // from string text.
        gvar = g_variant_new_uint64(
            sessionObj[info->name].toString().toULongLong());
      } else if (info->datatype == SR_T_UINT8) {
        if (sessionObj[info->name].toString() != "")
          gvar = g_variant_new_byte(sessionObj[info->name].toString().toUInt());
        else
          gvar = g_variant_new_byte(sessionObj[info->name].toInt());
      } else if (info->datatype == SR_T_INT16) {
        gvar = g_variant_new_int16(sessionObj[info->name].toInt());
      } else if (info->datatype == SR_T_FLOAT) {
        if (sessionObj[info->name].toString() != "")
          gvar = g_variant_new_double(
              sessionObj[info->name].toString().toDouble());
        else
          gvar = g_variant_new_double(sessionObj[info->name].toDouble());
      } else if (info->datatype == SR_T_CHAR) {
        gvar = g_variant_new_string(
            sessionObj[info->name].toString().toLocal8Bit().data());
      } else if (info->datatype == SR_T_LIST) {
        id = 0;

        if (format_ver > 2) {
          // Is new version format.
          id = sessionObj[info->name].toInt();
        } else {
          const char *fd_key =
              sessionObj[info->name].toString().toLocal8Bit().data();
          id = _main_window->device_agent()->option_value_to_code(
              conf_dev_mode, info->key, fd_key);
          if (id == -1) {
            pxv_err("Convert failed, key:\"%s\", value:\"%s\"", info->name,
                    fd_key);
            id = 0; // set default value.
          } else {
            pxv_info("Convert success, key:\"%s\", value:\"%s\", get code:%d",
                     info->name, fd_key, id);
          }
        }
        gvar = g_variant_new_int16(id);
      }

      if (gvar == NULL) {
        pxv_warn("Warning: Profile failed to parse key:'%s'", info->name);
        continue;
      }

      bool bFlag = _main_window->device_agent()->set_config(info->key, gvar);
      if (!bFlag) {
        pxv_err("Set device config option failed, id:%d, code:%d", info->key,
                id);
      }
    }
  }

  // load channel settings
  // Task 3 (purify-architecture-concepts): channel 段改走 SignalConfigStore 单一
  // 路径。原代码按 DSO/非 DSO 两分支直改 sr_channel->vdiv/coupling/vfactor/
  // trig_value/map_*/enabled/name，现统一为：signal_config_from_json 解析
  // channels[] 数组到 _signal_config，apply_signal_config 应用到 sr_channel。
  // 顶层 key 仍是 "channel"（单数），此处包成 {"channels": [...]} 喂给 store。
  // work_mode/operation_mode/channel_mode/is_demo 取当前 device 已应用的值，
  // 避免 apply_signal_config 误改 device mode（device settings 循环已设置）。
  if (sessionObj.contains("channel")) {
    pv::TabContext *ctx = _main_window->current_context();
    pv::data::SessionDocument *doc = ctx ? ctx->document() : nullptr;
    if (doc) {
      QJsonObject sig_obj;
      sig_obj["channels"] = sessionObj["channel"].toArray();
      doc->signal_config_from_json(sig_obj);
      // 用当前 device 已应用的 mode/op_mode/ch_mode/is_demo 覆盖，保证
      // apply_signal_config 不会改变 device mode（仅应用 per-channel 字段）。
      auto &cfg = doc->signal_config_store()->get_signal_config();
      cfg.work_mode = _main_window->device_agent()->get_work_mode();
      int tmp_mode;
      if (_main_window->device_agent()->get_config_int16(SR_CONF_OPERATION_MODE,
                                                         tmp_mode))
        cfg.operation_mode = tmp_mode;
      if (_main_window->device_agent()->get_config_int16(SR_CONF_CHANNEL_MODE,
                                                         tmp_mode))
        cfg.channel_mode = tmp_mode;
      cfg.is_demo = _main_window->device_agent()->is_demo();
      doc->apply_signal_config();
    } else {
      pxv_warn("MainWindow::load_config_from_json: no active document, "
               "skipping channel apply");
    }
  }

  // Task 16 (purify-architecture-concepts): load per-channel UI layout from
  // the top-level "uiLayout" segment into the View's DockUiState. MUST be
  // loaded BEFORE _session->reload() below: reload() broadcasts
  // signals_changed (async, Qt::QueuedConnection) which triggers
  // SignalFactory::update_signals(AllReplaced) — that branch reads
  // DockUiState::channel_layouts to restore view_index/v_offset/own_height/
  // visible. Loading here ensures the map is populated before the async
  // handler runs. Old .pxc files without "uiLayout" segment use default
  // layout (ChannelLayoutState ctor defaults) — no error, no migration, per
  // "不考虑兼容性". Explicit null guard: assert is a no-op in Release.
  if (sessionObj.contains("uiLayout")) {
    if (auto *view = _main_window->current_view()) {
      view->load_ui_layout_from_json(sessionObj["uiLayout"].toArray());
    } else {
      pxv_warn("MainWindow::load_config_from_json: no active view, skipping "
               "uiLayout apply");
    }
  }

  // reload() rebuilds SignalModels from the (just-updated) sr_channel state
  // (probe->enabled/name/vdiv/coupling/vfactor/zero_offset/trig_value set
  // above via apply_signal_config). It also reads probe->trig_value (DSO) and
  // probe->offset (ANALOG vertical_offset) into the new SignalModels, and
  // preserves trig_type/color from the old models (which apply_signal_config
  // just updated via set_color/set_trig_type). SignalFactory::create_signal()
  // then builds the new view::Signal objects from these fully-populated models:
  //   - colour   <- model->color()              (apply_model_properties)
  //   - name     <- model->name()               (apply_model_properties)
  //   - trig_type<- model->trig_type()          (apply_model_properties -> LogicSignal::set_trig)
  //   - zero_offset/trig_value (DSO) <- sr_channel (DsoSignal constructor -> load_settings)
  //   - vertical_offset (ANALOG)    <- model->vertical_offset() (AnalogSignal constructor)
  // Task 13: the View-side per-channel loop that previously called the
  // per-signal colour/trig/zero/trig-ratio/commit setters here is now DELETED —
  // the Core path (apply_signal_config + reload + SignalFactory) fully restores
  // all channel config. No backward-compat for old ratio-format (0,1) .pxc
  // files (user said "不考虑兼容性").
  _session->reload();

  // update UI settings
  _main_window->sampling_bar()->update_sample_rate_list();
  _main_window->trigger_widget()->device_updated();
  _main_window->current_view()->header_updated();

  // load trigger settings
  // Task 6: trigger 反序列化改走 Core TriggerConfig（唯一真相源）。
  // from_json() 读 to_json() 写入的新结构；set_trigger_config() 广播
  // DSV_MSG_TRIGGER_CONFIG_CHANGED；随后 refresh_ui_from_core() 把 Core
  // 状态映射到 TriggerDock 控件（View 层不再解析 trigger JSON）。
  if (sessionObj.contains("trigger")) {
    _session->set_trigger_config(
        data::TriggerConfig::from_json(sessionObj["trigger"].toObject()));
    _main_window->trigger_widget()->refresh_ui_from_core();
  }

  // load decoders
  if (sessionObj.contains("decoder")) {
    QJsonArray deArray = sessionObj["decoder"].toArray();
    if (deArray.empty() == false) {
      haveDecoder = true;
      StoreSession ss(_session);
      ss.load_decoders(_main_window->protocol_widget(), deArray);
      _main_window->current_view()->update_all_trace_postion();
    }
  }

  // load measure
  if (sessionObj.contains("measure")) {
    auto *bottom_bar = _main_window->current_view()->get_viewstatus();
    bottom_bar->load_session(sessionObj["measure"].toArray(), format_ver);
  }

  if (gvar_opts != NULL)
    g_variant_unref(gvar_opts);

  return true;
}

bool SessionConfigSerializer::save_config_to_file(QString name) {
  if (name == "") {
    pxv_err("Session file name is empty.");
    assert(false);
    return false;
  }

  std::string file_name = pv::path::ToUnicodePath(name);
  pxv_info("Store session to file: \"%s\"", file_name.c_str());

  QFile sf(name);
  if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    pxv_warn("Warning: Couldn't open profile to write!");
    return false;
  }

  QTextStream outStream(&sf);
  encoding::set_utf8(outStream);

  QJsonObject sessionVar;
  if (!gen_config_json(sessionVar)) {
    return false;
  }

  QJsonDocument sessionDoc(sessionVar);
  outStream << QString::fromUtf8(sessionDoc.toJson());
  sf.close();
  return true;
}

} // namespace pv
