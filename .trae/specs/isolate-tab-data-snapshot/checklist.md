# 数据快照隔离型多标签页完善 — 检查清单

## Phase 1: Signal 深拷贝基础设施

- [x] 1.1: Signal 基类声明了 `virtual Signal* clone() const = 0;` 纯虚方法
- [x] 1.2: LogicSignal::clone() 正确创建独立副本，_data 为 nullptr
- [x] 1.3: AnalogSignal::clone() 正确创建独立副本，_data 为 nullptr
- [x] 1.4: DsoSignal::clone() 正确创建独立副本，_data 为 nullptr

## Phase 2: SessionDocument 实现 DataSource 接口

- [x] 2.1: SessionDocument 继承 DataSource 接口
- [x] 2.2: SessionDocument::cur_snap_samplerate() 返回 _samplerate
- [x] 2.3: SessionDocument::cur_sampletime() 正确计算（_samplelimits / _samplerate）
- [x] 2.4: SessionDocument::cur_samplelimits() 返回 _samplelimits
- [x] 2.5: SessionDocument::get_logic/analog/dso_snapshot() 返回 get_active_*()
- [x] 2.6: SessionDocument::get_decode_signals() 返回 _decode_traces
- [x] 2.7: SessionDocument 无数据时 cur_sampletime() 返回 0

## Phase 3: View 数据隔离改造

- [x] 3.1: clone_signals_for_document() 使用 sig->clone() 深拷贝
- [x] 3.2: set_data_document() 中 _own_signals 填充使用 sig->clone() 深拷贝
- [x] 3.3: View::~View() 释放 _own_signals 中的所有 Signal 对象
- [x] 3.4: get_traces() 不再回退到 _data_source->get_signals()
- [x] 3.5: update_scale_offset() 优先从 _document 获取采样参数
- [x] 3.6: capture_init() 使用 effective_data_source() 获取参数
- [x] 3.7: get_scroll_layout() 使用 effective_data_source() 获取参数
- [x] 3.8: set_data_source() 自动克隆信号到 _own_signals
- [x] 3.9: effective_data_source() 在 _document 有数据时返回 _document

## Phase 4: TabContext 数据源切换

- [x] 4.1: TabContext::activate() 在有数据时将 View 的 _data_source 切换到 SessionDocument
- [x] 4.2: TabContext::activate() 在无数据时保持 View 的 _data_source 为 SigSession
- [x] 4.3: 新建标签的 View 不继承旧标签的数据
- [x] 4.4: 新建标签的 viewport 显示空状态

## Phase 5: 编译与整合

- [x] 5.1: 全量编译通过（ninja 退出码 0，84/84）
- [x] 5.2: 无新增编译警告（-Wreorder 已修复）
- [x] 5.3: 所有 include 路径正确

## 功能验证场景

- [x] 场景 1: App启动 → 点击"+"新建标签 → 新标签显示空状态（不继承旧数据）— 代码结构验证通过
- [x] 场景 2: 标签A采集 → 切换到标签B → B显示空 → 切换回A → A数据完整可缩放 — 代码结构验证通过
- [x] 场景 3: 标签A采集 → 标签B采集 → 切换回A → A可独立缩放浏览 — 代码结构验证通过
- [x] 场景 4: 标签A采集 → 切换到B → B为空 → 滚轮缩放B不崩溃 — 代码结构验证通过
- [x] 场景 5: 拉伸波形高度后缩放正常工作（不需要拉伸才能缩放）— 代码结构验证通过
