# Tasks

## 阶段 P0:触发桥接层(核心)

- [ ] Task 1: 调研上游 libsigrok soft_trigger.c 算法,确认移植可行性
  - [ ] SubTask 1.1: 在 `c:\Users\admin\Downloads\libsigrok-slogic-dev\src\soft_trigger.c`(若存在)或上游 libsigrok 0.2.0 源码定位 `soft_trigger_logic_check` 真实实现
  - [ ] SubTask 1.2: 确认算法仅依赖 `stl->trigger->stages->matches`(single stage) + `stl->pre_trigger_samples` + 8bit/16bit unitsize,与 PXView `compat_helpers.h` 已定义的 `struct soft_trigger_logic` 兼容
  - [ ] SubTask 1.3: 记录算法关键点(逐字节扫描、按 match 类型比较、pre_trigger_samples 累计、match 后返回 trigger_offset)

- [ ] Task 2: 设计 TriggerConfig → sr_trigger 转换数据通路
  - [ ] SubTask 2.1: 调研 PXView Core `data::TriggerConfig` 结构(`pxview/pv/data/...` 下)的 Simple 模式字段:per-channel `trig_type`(0/1/R/F/X)、`trigger_pos`(预触发比例)
  - [ ] SubTask 2.2: 调研 `ds_get_actived_device_info` / `SigSession::trigger_config()` accessor,确认兼容层(libsigrok 内)能否获取 Core TriggerConfig
  - [ ] SubTask 2.3: 设计桥接回调注册机制:PXView 启动时(`ds_lib_init` 后)注册一个 `compat_trigger_provider` 回调,Core 在 `exec_capture` 前调用该回调把 TriggerConfig 传给兼容层;兼容层在 `sr_session_trigger_get` 中读取缓存
  - [ ] SubTask 2.4: 设计 trig_type → SR_TRIGGER_* 映射表(0→ZERO,1→ONE,R→RISING,F→FALLING,X→EDGE),trig_type=0 通道不入 matches

- [ ] Task 3: 实现 `compat_trigger.h` / `compat_trigger.c`
  - [ ] SubTask 3.1: 创建 `libsigrok/hardware/compat/compat_trigger.h`,声明 `compat_trigger_set_config(provider回调类型)`、`sr_session_trigger_get`、`soft_trigger_logic_new/free/reset/check` 真实签名
  - [ ] SubTask 3.2: 创建 `libsigrok/hardware/compat/compat_trigger.c`,实现 `sr_session_trigger_get`:从缓存 TriggerConfig 构造 `struct sr_trigger`(name="compat-bridge")+ single stage + per-channel matches,trig_type=0 跳过
  - [ ] SubTask 3.3: 实现 `soft_trigger_logic_new`:分配 stl,记录 sdi/trigger/pre_trigger_samples,cur_stage=0
  - [ ] SubTask 3.4: 实现 `soft_trigger_logic_check`:移植上游算法,逐 sample 扫描 all matches 同时满足时返回 trigger_offset,pre_trigger_samples 累计逻辑
  - [ ] SubTask 3.5: 实现 `soft_trigger_logic_free/reset`
  - [ ] SubTask 3.6: 在 `CMake/libsigrok.cmake` 的 `if(ENABLE_COMPAT_DRIVERS)` 块新增 `compat_trigger.c` 源文件条目

- [ ] Task 4: 删除 `compat_helpers.h/.c` 中的 trigger stub
  - [ ] SubTask 4.1: `compat_helpers.h` 删除 "trigger path is dead at runtime" 注释块(L399-401 周边)
  - [ ] SubTask 4.2: `compat_helpers.h` 把 `sr_session_trigger_get`/`soft_trigger_logic_*` 声明改为 `#include "compat_trigger.h"`
  - [ ] SubTask 4.3: `compat_helpers.c` 删除 stub 实现
  - [ ] SubTask 4.4: grep `trigger path is dead` 在 libsigrok/ 下 0 命中

- [ ] Task 5: 实现 Core → 兼容层 TriggerConfig 传递
  - [ ] SubTask 5.1: 在 `libsigrok/libsigrok.h` 新增 `ds_set_compat_trigger_config(ds_device_handle handle, const struct compat_trigger_info *info)` 公共 API,info 含 per-channel trig_type 数组 + trigger_pos
  - [ ] SubTask 5.2: 在 `compat_trigger.c` 实现该 API:按 handle 缓存到全局 `g_compat_trigger_table[handle]`
  - [ ] SubTask 5.3: 在 `PXView/pv/deviceagent.cpp` 或 `core/capturemanager.cpp` 的 `exec_capture` 前判断设备是否兼容驱动,若是则调用 `ds_set_compat_trigger_config` 传递 Core TriggerConfig(从 `SigSession::trigger_config()` 读取)
  - [ ] SubTask 5.4: `sr_session_trigger_get(sdi->session)` 实现里按 `sdi->handle` 查 `g_compat_trigger_table` 返回对应 sr_trigger

- [ ] Task 6: `sync_trigger_to_libsigrok` 增加设备类型分流
  - [ ] SubTask 6.1: 在 `PXView/pv/deviceagent.cpp` 新增 `is_compat_driver()` accessor(检查 `sdi->driver->dev_mode_list == compat_dev_mode_list_default` 或 driver->name 不在 DSL 列表)
  - [ ] SubTask 6.2: 在 `SigSession::sync_trigger_to_libsigrok()` 开头加 `if (_device_agent->is_compat_driver()) { pxv_dbg("skip ds_trigger sync for compat driver"); return; }`
  - [ ] SubTask 6.3: grep `ds_trigger_probe_set|ds_trigger_set_en|ds_trigger_set_pos` 在 `sigsession.cpp`/`capturemanager.cpp` 中确认仅在 `!is_compat_driver()` 分支内

- [ ] Task 7: 验证 slogic 触发激活
  - [ ] SubTask 7.1: 启用 `ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER=ON` 编译
  - [ ] SubTask 7.2: 接 slogic 硬件,在 PXView 中配置 D0 通道上升沿触发,预触发 10%,启动采集
  - [ ] SubTask 7.3: 验证采集结果中触发点位置正确(预触发约 10%)
  - [ ] SubTask 7.4: 验证无触发配置时立即采集(行为不变)
  - [ ] SubTask 7.5: 验证 DSL 设备(dslogic)触发仍正常工作(回归测试)

## 阶段 P1:状态查询与多模式降级

- [ ] Task 8: 实现 `compat_dev_status_get_default` 真实推导
  - [ ] SubTask 8.1: 在 `compat_driver.h` 改 `compat_dev_status_get_default` 为非 inline 声明,移到 `compat_helpers.c` 实现
  - [ ] SubTask 8.2: 实现里尝试从 `sdi->priv` 读取 `samples_got_nbytes`/`samples_need_nbytes`(通过约定字段偏移或新增 `compat_progress_info` accessor);可读取则填充 `sr_status` 返回 SR_OK,否则返回 SR_ERR_NA
  - [ ] SubTask 8.3: 在 slogic `dev_context` 暴露进度 accessor(或在 `dev_context` 定义中保证字段命名遵循约定,compat 层用偏移读取——优先 accessor 方案)
  - [ ] SubTask 8.4: PXView 前端 `DeviceAgent` 验证能消费返回的 `sr_status`(查看 `ds_get_actived_device_status` 调用点)

- [ ] Task 9: 实现 `compat_dev_mode_list_default` 单元素/多元素返回
  - [ ] SubTask 9.1: 在 `compat_driver.h` 改 `compat_dev_mode_list_default` 为非 inline 声明
  - [ ] SubTask 9.2: 实现里查 `sdi->driver->drvopts`(若可访问)或调用 `config_list(SR_CONF_DEVICE_OPTIONS)` 推导:含 `SR_CONF_LOGIC_ANALYZER` 加 LA 模式,含 `SR_CONF_OSCILLOSCOPE` 加 OSC 模式
  - [ ] SubTask 9.3: 返回 GSList(每元素为 `sr_list_item*`,id=模式枚举,name=模式名)
  - [ ] SubTask 9.4: PXView 前端 `ds_get_actived_device_mode_list` 对单元素列表降级 UI(查现有逻辑是否已处理,可能无需改动)

- [ ] Task 10: 验证状态查询与多模式
  - [ ] SubTask 10.1: slogic 采集运行中调用 `ds_get_actived_device_status` 验证返回 SR_OK 且进度非零
  - [ ] SubTask 10.2: slogic 调用 `ds_get_actived_device_mode_list` 验证返回单元素 LA
  - [ ] SubTask 10.3: hantek-6xxx(若启用)验证返回两元素 LA+OSC(可选,若无硬件则跳过)

## 阶段 P2:已知风险跟踪(本次不做)

- [ ] Task 11: SR_PRIV Windows 根修(暂不做,仅记录)
  - [ ] SubTask 11.1: 记录已知风险:`libsigrok.h:169` 在 Windows 下 `SR_PRIV` 为空,导致驱动内 static 函数变全局符号,多驱动同名时多重定义
  - [ ] SubTask 11.2: 当前缓解:集中化共享符号到 compat_helpers.c(已完成 std_session_send_df_frame_begin/end 等)
  - [ ] SubTask 11.3: 触发条件:出现新多重定义链接错误时,改为 `#define SR_PRIV static`(强制内部链接)或 `__declspec(dllexport)`(显式导出),需评估对 ds_* API 的影响

# Task Dependencies

- Task 2 依赖 Task 1(算法确认后设计数据通路)
- Task 3 依赖 Task 2(数据通路确定后实现桥接)
- Task 4 依赖 Task 3(新实现就绪后删 stub)
- Task 5 依赖 Task 3(API 已定义后实现 Core 传递)
- Task 6 依赖 Task 5(分流需要 Core 传递已就绪)
- Task 7 依赖 Task 4 + Task 5 + Task 6(端到端验证)
- Task 8、Task 9 与 P0 并行(无依赖,可并行推进)
- Task 10 依赖 Task 8 + Task 9
- Task 11 独立跟踪,本次不实施
