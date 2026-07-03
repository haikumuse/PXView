# Checklist

## 阶段 P0:触发桥接层

- [ ] 上游 libsigrok `soft_trigger.c` 算法已调研,关键点(逐字节扫描/match 比较/pre_trigger 累计/trigger_offset 返回)已记录
- [ ] PXView Core `data::TriggerConfig` Simple 模式字段(per-channel trig_type / trigger_pos)已确认
- [ ] Core → 兼容层 TriggerConfig 传递数据通路已设计(桥接回调 + handle 索引缓存)
- [ ] trig_type → SR_TRIGGER_* 映射表已定义(0→ZERO,1→ONE,R→RISING,F→FALLING,X→EDGE;trig_type=0 不入 matches)
- [ ] `libsigrok/hardware/compat/compat_trigger.h` 已创建,声明 sr_session_trigger_get / soft_trigger_logic_* 真实签名 + compat_trigger_set_config 回调类型
- [ ] `libsigrok/hardware/compat/compat_trigger.c` 已创建,实现 sr_session_trigger_get(从缓存 TriggerConfig 构造 sr_trigger single stage + matches)
- [ ] `compat_trigger.c` 实现 soft_trigger_logic_new(分配 stl,记录 sdi/trigger/pre_trigger_samples/cur_stage=0)
- [ ] `compat_trigger.c` 实现 soft_trigger_logic_check(移植上游算法,逐 sample 扫描,all matches 满足时返回 trigger_offset,pre_trigger_samples 累计)
- [ ] `compat_trigger.c` 实现 soft_trigger_logic_free/reset
- [ ] `CMake/libsigrok.cmake` 的 `if(ENABLE_COMPAT_DRIVERS)` 块已新增 compat_trigger.c 源条目
- [ ] `compat_helpers.h` 删除 "trigger path is dead at runtime" 注释块(L399-401 周边)
- [ ] `compat_helpers.h` 改 `#include "compat_trigger.h"` 替代本地声明
- [ ] `compat_helpers.c` 删除 sr_session_trigger_get / soft_trigger_logic_* stub 实现
- [ ] grep `trigger path is dead` 在 libsigrok/ 下 0 命中
- [ ] `libsigrok.h` 新增 `ds_set_compat_trigger_config(handle, info)` 公共 API
- [ ] `compat_trigger.c` 实现按 handle 缓存 TriggerConfig 到全局表
- [ ] `PXView/pv/deviceagent.cpp` 或 `core/capturemanager.cpp` 在 exec_capture 前对兼容驱动调用 ds_set_compat_trigger_config
- [ ] `PXView/pv/deviceagent.cpp` 新增 `is_compat_driver()` accessor
- [ ] `SigSession::sync_trigger_to_libsigrok()` 开头加 `if (is_compat_driver()) return;` 分流
- [ ] grep `ds_trigger_probe_set|ds_trigger_set_en|ds_trigger_set_pos` 在 sigsession.cpp/capturemanager.cpp 中确认仅在 `!is_compat_driver()` 分支内
- [ ] slogic 触发硬件实测:配置 D0 上升沿 + 预触发 10%,采集结果触发点位置正确
- [ ] slogic 无触发配置时立即采集(行为与现状等价)
- [ ] DSL 设备(dslogic)触发回归测试通过(硬件触发不受影响)
- [ ] `cd build && ninja -j 16 && ninja install` 编译通过

## 阶段 P1:状态查询与多模式降级

- [ ] `compat_dev_status_get_default` 改为非 inline 声明,移到 compat_helpers.c 实现
- [ ] 实现尝试从 sdi->priv 读取采集进度(slogic devc->samples_got_nbytes/samples_need_nbytes),可读则填充 sr_status 返回 SR_OK
- [ ] slogic dev_context 暴露进度 accessor(或字段命名遵循约定)
- [ ] 无进度字段的兼容驱动(如 scpi-dmm)仍返回 SR_ERR_NA
- [ ] slogic 采集运行中 `ds_get_actived_device_status` 返回 SR_OK 且进度非零
- [ ] `compat_dev_mode_list_default` 改为非 inline 声明
- [ ] 实现查 drvopts 推导模式:含 SR_CONF_LOGIC_ANALYZER 加 LA,含 SR_CONF_OSCILLOSCOPE 加 OSC
- [ ] 返回 GSList(sr_list_item*),id=模式枚举,name=模式名
- [ ] slogic `ds_get_actived_device_mode_list` 返回单元素 LA
- [ ] PXView 前端对单元素列表降级 UI(禁用模式切换)——查现有逻辑是否已处理
- [ ] hantek-6xxx(若启用且无硬件可跳过)返回两元素 LA+OSC

## 阶段 P2:已知风险跟踪

- [ ] SR_PRIV Windows 空问题已记录(本次不修)
- [ ] 当前缓解措施(集中化共享符号)已确认有效
- [ ] 触发条件(出现新多重定义链接错误时再做根修)已记录在 spec.md

## 整体验证

- [ ] slogic 整体功能还原度从 ~70% 提升至 ~95%(触发/状态/多模式三项补齐)
- [ ] DSL 原生驱动触发/状态/多模式回归测试通过(无回归)
- [ ] 其他兼容驱动(fx2lafw/kingst-la2016 等)无回归(端到端编译通过)
- [ ] `cd build && ninja -j 16 && ninja install` 整体编译通过
