# 技术债务优先级评估 Spec

## Why

`complete-remaining-modernization-debt` spec 完成后，需要评估剩余技术债务哪些值得继续做、哪些应该停止。避免无收益的纯重构浪费精力，把后续工作集中在 ROI 高的项上。

## 评估方法

对每个候选债务项，按以下维度打分（1-5）：
- **收益**：解决后带来的实际价值（可维护性/稳定性/编译时间/开发者体验）
- **风险**：引入回归的概率和影响范围
- **成本**：工作量大小（行数/接口改动/测试覆盖）

最终判定：收益高 + 风险低 = 值得做；收益低或风险高 = 不值得做。

## 调查事实摘要（来自 search subagent）

| 债务项 | 当前状态 | 关键数据 |
|--------|----------|----------|
| 1. SigSession God class | **已解决** | 366+1981行，7 unique_ptr，~130 public 方法，已是 facade；manager 无 SigSession* 反向引用 |
| 2. Manager 循环依赖 | **已解决** | core/*.h 中 0 处 SigSession* 成员，仅 1 处 forward decl（documentregistry.h 参数） |
| 3. DocumentRegistry 非拥有裸指针 | **已解决** | `vector<unique_ptr<SessionDocument>> _owned_documents`，访问器返回弱指针 |
| 4. FilterProcessor 越界访问 | **已解决** | 0 处 `_session->` 调用 |
| 5. LogicSnapshot 拆分剩余 | **剩余** | h=340行 / cpp=2303行；cluster C (glitch filter) ~505行/10方法；cluster E (diagnostics) ~66行/9方法；cluster F (loop mode) ~80行+散布在15+方法 |
| 6. CMake 模块化剩余 | **剩余** | core_sources.cmake 仍含 53 个 .cpp；data/ 目录 ~24 .cpp 是最大可提取簇 |
| 7. dso_measure.cpp libsigrok.h 依赖 | **剩余** | 仅依赖 `sr_status` 值类型（1 处局部变量 + ~30 处字段访问），0 处函数调用；需改 DataSource::get_dso_status() 签名 |
| 8. view.h God header | **剩余** | 804行 / 12 includes / 28 inline forwarders / 5 access 段 / 6 friend decl |

## 评估结果

### 债务 1-4：已解决 — 不需要再做

**结论**：先前 `modernize-core-layer-radical` + `complete-remaining-modernization-debt` 两个 spec 已经彻底解决。project_memory.md 中相关条目应标记为已完成。

---

### 债务 5：LogicSnapshot 拆分剩余 — **部分值得做**

**子项 5C：Cluster C (glitch filter) 拆分** — **值得做**
- 收益：4/5 — 提取 ~505 行（含 274 行的 `apply_glitch_filter` 巨型方法），让 logicsnapshot.cpp 从 2303 降到 ~1800，职责清晰
- 风险：2/5 — glitch filter 逻辑自包含，与 storage/query 耦合弱，提取为 `LogicSnapshotGlitchFilter` 类（friend back-pointer 模式，与 DiskCacheWriter 一致）
- 成本：3/5 — 约 600 行移动 + 调用点转发，已有 DiskCacheWriter 模式可复用
- **ROI：高**

**子项 5E：Cluster E (diagnostics) 拆分** — **不值得做**
- 收益：2/5 — 仅 66 行，9 个 1-3 行 forwarder，提取后无明显可读性提升
- 风险：2/5 — 低
- 成本：2/5 — 小
- **ROI：低** — 收益太小，不值得单独拆

**子项 5F：Cluster F (loop mode) 拆分** — **不值得做**
- 收益：3/5 — loop 逻辑散布在 15+ 方法的 `_loop_offset` 调整中，提取后能让 storage 方法更干净
- 风险：5/5 — loop offset 逻辑与 `first_payload`/`append_cross_payload`/`get_samples`/`get_nxt_edge_unlock` 等核心存储路径深度交织，提取几乎必然引入回归
- 成本：5/5 — 需要重新设计 loop 模式的抽象边界，不是简单移动
- **ROI：负** — 风险高于收益，保持现状

---

### 债务 6：CMake 模块化剩余 — **值得做（部分）**

**子项 6A：提取 `pv/data/` 为 STATIC lib** — **值得做**
- 收益：4/5 — 24 个 .cpp 提取后 core_sources.cmake 从 53 降到 ~29，编译依赖图更清晰，data/ 修改不触发 core 重编
- 风险：2/5 — data/ 内部依赖（logicsnapshot → snapshot 等）都在同目录，外部依赖通过 `pxview-core` PUBLIC 链接透传
- 成本：3/5 — 创建 CMakeLists.txt + 移动 entries + 验证编译
- **ROI：高**

**子项 6B：提取 `pv/core/` 为 STATIC lib** — **不值得做**
- 收益：2/5 — 仅 7 个 .cpp，且 SigSession（在 pv/ 顶层）反向依赖 core/ managers，提取后 SigSession 仍需 link core lib，无实质解耦
- 风险：3/5 — manager 之间通过 SessionStateContext 协作，提取后仍需暴露 SessionStateContext 为 PUBLIC
- 成本：2/5
- **ROI：低** — 收益不足以抵消 CMake 复杂度增加

**子项 6C：提取 `pv/api/` 为 STATIC lib** — **不值得做**
- 收益：2/5 — 仅 5 个 .cpp
- 风险：3/5 — api/ 依赖 SigSession（通过 SessionService），而 SigSession 在 pxview-core，提取后循环依赖需打破
- 成本：3/5
- **ROI：低** — 循环依赖问题大于收益

---

### 债务 7：dso_measure.cpp libsigrok.h 依赖 — **不值得做**

- 收益：2/5 — 仅 1 个 View 文件去掉 libsigrok.h include，sr_status 字段访问逻辑不变
- 风险：4/5 — 需要在 Core 层定义 `DsoMeasureStatus` 镜像结构（~30 字段），维护 sr_status ↔ DsoMeasureStatus 双写一致性；DataSource::get_dso_status() 签名变更影响所有实现
- 成本：4/5 — Core 镜像结构 + 转换函数 + DataSource 接口变更 + 所有调用点更新
- **ROI：负** — 引入双写风险换取 1 个 include 移除，不划算。libsigrok.h 是 C 硬件抽象层头，View 层合法使用其值类型不是分层违规（与先前 spec 结论一致）

---

### 债务 8：view.h God header — **部分值得做**

**子项 8A：access 段整理（5 → 3）** — **值得做**
- 收益：3/5 — 当前 5 段（public/public slots/signals/private slots/private）符合 Qt 惯例，但可合并为 public / signals / private 三段
- 风险：1/5 — 纯 cosmetic，无行为变更
- 成本：1/5 — 移动成员声明
- **ROI：高**（但收益也小，属于顺手做）

**子项 8B：inline forwarder 下沉到 .cpp** — **不值得做**
- 收益：2/5 — 28 个 forwarder 下沉减少 header 编译时间，但 Q_OBJECT/Q_PROPERTY 已强制 QScrollArea 完整定义
- 风险：2/5 — 低
- 成本：3/5 — 28 个方法下沉，.cpp 增加 ~150 行
- **ROI：低** — Qt MOC 约束下，include 数量无法降到 ≤8，下沉 forwarder 收益有限

**子项 8C：include 数量 ≤8 目标** — **不可行，放弃**
- Qt MOC 要求 Q_OBJECT/Q_PROPERTY 所在文件能完整解析基类 QScrollArea，无法前向声明
- 当前 12 includes 中 5 个是 std 头（不可去）、2 个是 Qt 头（QScrollArea 必需）、5 个是项目头（部分可去）
- 现实目标：12 → 10，收益太小不值得做

---

## 最终建议清单

| 优先级 | 债务项 | 子项 | 建议动作 |
|--------|--------|------|----------|
| P1 | 5C | LogicSnapshot glitch filter 拆分 | **做** — 提取 LogicSnapshotGlitchFilter，~505 行 |
| P2 | 6A | pv/data/ 提取为 STATIC lib | **做** — 24 .cpp 提取 |
| P3 | 8A | view.h access 段整理 | **做**（顺手，低成本） |
| — | 5E | Cluster E diagnostics 拆分 | **不做** — 太小 |
| — | 5F | Cluster F loop mode 拆分 | **不做** — 风险太高 |
| — | 6B | pv/core/ 提取为 STATIC lib | **不做** — 收益不足 |
| — | 6C | pv/api/ 提取为 STATIC lib | **不做** — 循环依赖 |
| — | 7 | dso_measure.cpp libsigrok.h | **不做** — 双写风险 > 收益 |
| — | 8B | view.h forwarder 下沉 | **不做** — Qt MOC 约束下收益有限 |
| — | 8C | view.h include ≤8 | **放弃** — 不可行 |

## What Changes

本 spec 是**评估型**，不直接修改代码。评估结论如下：

- **批准做的项**（P1/P2/P3）：后续可由用户发起 implementation spec 执行
- **不做的项**：在 project_memory.md 中标记为"已评估，不做"，避免后续重复评估
- **已解决的项**（1-4）：在 project_memory.md 中标记为"已解决"

## Impact

- Affected specs: `complete-remaining-modernization-debt`（前序）、可能的后续 `split-logicsnapshot-glitch-filter`、`extract-pv-data-static-lib`
- Affected code: 无直接代码改动
- Affected docs: `project_memory.md`（追加评估结论）、`AGENTS.md`（无需改动）

## ADDED Requirements

### Requirement: 技术债务评估结论归档
系统 SHALL 在 project_memory.md 中记录本次评估结论，对每个债务项标注"已解决/值得做/不做"状态，避免后续重复评估。

#### Scenario: 评估结论归档
- **WHEN** 本 spec 完成后
- **THEN** project_memory.md "Completion Records" 章节追加"技术债务优先级评估"小节，列出 9 项结论
