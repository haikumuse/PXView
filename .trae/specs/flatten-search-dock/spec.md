# 搜索面板一级化 Spec

## Why
当前 SearchDock 的搜索模式编辑是二级交互：用户需要先点击 FakeLineEdit 搜索框，再在弹出的 `dialogs::Search` 模态对话框中编辑每个通道的搜索模式。这种二级菜单交互增加了操作步骤，不够直观。将搜索模式编辑器直接嵌入 SearchDock 面板中，使用户可以一步到位地编辑搜索模式，提升操作效率。

## What Changes
- 移除 `FakeLineEdit`（假搜索框）和搜索按钮，替换为直接嵌入的逐通道搜索模式编辑器
- 将 `dialogs::Search` 对话框中的 `SearchEdgeFlagEdit`、验证器、图例标签等 UI 元素内联到 `SearchDock` 中
- 修改 `SearchDock::on_set()` 的逻辑：不再弹出模态对话框，改为实时响应编辑器变化
- 调整 `SearchDock` 的布局：上方为逐通道模式编辑区（含图例），下方为 Previous/Next 导航按钮
- `SearchDock` 需要在信号列表变化时（设备更新、通道增减）动态重建编辑器
- **BREAKING**: `dialogs::Search` 对话框将不再被 `SearchDock` 使用（但保留文件，不删除）

## Impact
- Affected specs: 无已有 spec
- Affected code:
  - `DSView/pv/dock/searchdock.h`（修改：移除 FakeLineEdit 和搜索按钮成员，新增编辑器容器和图例标签成员）
  - `DSView/pv/dock/searchdock.cpp`（修改：重写构造函数布局，重写 on_set 逻辑，新增实时更新逻辑和动态重建逻辑）
  - `DSView/pv/dialogs/search.h`（不修改，保留）
  - `DSView/pv/dialogs/search.cpp`（不修改，保留）
  - `DSView/pv/widgets/fakelineedit.h`（不修改，保留）
  - `DSView/pv/widgets/fakelineedit.cpp`（不修改，保留）

## ADDED Requirements

### Requirement: 内联搜索模式编辑器
SearchDock SHALL 直接在面板中显示每个逻辑通道的搜索模式编辑器，无需弹出对话框即可编辑。

#### Scenario: 面板显示编辑器
- **WHEN** SearchDock 可见
- **AND** 存在逻辑通道信号
- **THEN** 面板上方区域显示一个网格布局，每行包含：通道名称标签、通道索引标签、搜索模式输入框
- **AND** 每个输入框默认值为 "X"（Don't care）
- **AND** 输入框仅接受 [10XRFCxrfc] 字符，最大长度为 1
- **AND** 输入框使用等宽字体

#### Scenario: 编辑模式实时生效
- **WHEN** 用户在某个通道的搜索模式输入框中输入或修改字符
- **THEN** 该字符自动转为大写
- **AND** `_pattern` 映射表实时更新
- **AND** 搜索光标位置重置（等效于原来的 `_view.set_search_pos(_view.get_search_pos(), false)`）

#### Scenario: 图例显示
- **WHEN** SearchDock 可见
- **THEN** 在编辑器区域右侧或下方显示图例标签："X: Don't care, 0: Low level, 1: High level, R: Rising edge, F: Falling edge, C: Rising/Falling edge"

### Requirement: 导航按钮布局
SearchDock SHALL 在编辑器区域下方显示 Previous/Next 导航按钮。

#### Scenario: 导航按钮布局
- **WHEN** SearchDock 可见
- **THEN** Previous 和 Next 按钮位于面板底部，水平居中排列
- **AND** 按钮功能与原有 on_previous()/on_next() 完全一致

### Requirement: 动态通道重建
SearchDock SHALL 在设备更新或通道列表变化时动态重建搜索模式编辑器。

#### Scenario: 设备更新时重建
- **WHEN** 接收到设备更新信号（device_updated）
- **THEN** SearchDock 重建编辑器网格，保留已有通道的搜索模式，新增通道默认为 "X"

#### Scenario: 通道数量减少
- **WHEN** 设备更新后逻辑通道数量减少
- **THEN** 已不存在的通道的搜索模式从 `_pattern` 中移除
- **AND** 编辑器网格仅显示当前存在的逻辑通道

### Requirement: 搜索模式持久化
SearchDock SHALL 在面板生命周期内保持搜索模式状态，不因面板显示/隐藏而丢失。

#### Scenario: 面板隐藏后重新显示
- **WHEN** 用户隐藏 SearchDock 后再次显示
- **THEN** 所有通道的搜索模式保持隐藏前的值

## MODIFIED Requirements

### Requirement: SearchDock 布局
SearchDock 的布局从单行水平布局改为垂直两段式布局：上方为搜索模式编辑区，下方为导航按钮区。

原有布局：
```
[stretch] [Pre] [FakeLineEdit+SearchIcon] [Next] [stretch]
```

新布局：
```
┌─────────────────────────────┐
│  Channel0: 0 [X]            │
│  Channel1: 1 [X]            │
│  Channel2: 2 [R]            │
│  ...                        │
│  ┌─────────────────────────┐│
│  │X:Don't care 0:Low ...   ││
│  └─────────────────────────┘│
├─────────────────────────────┤
│     [Pre]      [Next]       │
└─────────────────────────────┘
```

## REMOVED Requirements

### Requirement: FakeLineEdit 触发搜索对话框
**Reason**: 搜索模式编辑器已内联到面板中，不再需要 FakeLineEdit 作为触发器。
**Migration**: FakeLineEdit 和搜索按钮从 SearchDock 中移除，`on_set()` 方法不再打开对话框。
