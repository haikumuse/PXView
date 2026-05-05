# Tasks

- [x] Task 1: 修改 SearchDock 头文件，更新成员变量声明
  - [x] SubTask 1.1: 移除 FakeLineEdit 和搜索按钮相关成员（`_search_value`, `_search_button`）
  - [x] SubTask 1.2: 新增编辑器容器成员 `QVector<QLineEdit*> _search_lineEdit_vec`、图例标签 `QLabel*`、网格布局 `QGridLayout*`
  - [x] SubTask 1.3: 新增 `on_pattern_changed()` 槽函数声明
  - [x] SubTask 1.4: 新增 `build_editors()` 方法声明（用于动态重建编辑器）
  - [x] SubTask 1.5: 新增设备更新槽连接声明，移除 `on_set()` 槽声明

- [x] Task 2: 重写 SearchDock 构造函数，实现新的两段式垂直布局
  - [x] SubTask 2.1: 创建上方 QGridLayout，用于放置逐通道编辑器（初始为空，等待 build_editors 填充）
  - [x] SubTask 2.2: 创建图例标签，添加到编辑器网格右侧
  - [x] SubTask 2.3: 创建下方水平布局，放置 Previous/Next 按钮，居中对齐
  - [x] SubTask 2.4: 用 QVBoxLayout 将上方编辑区和下方按钮区组合
  - [x] SubTask 2.5: 连接设备更新信号到重建编辑器逻辑

- [x] Task 3: 实现 build_editors() 方法，动态创建逐通道搜索模式编辑器
  - [x] SubTask 3.1: 清除旧的编辑器控件和布局项
  - [x] SubTask 3.2: 遍历 `_session->get_signals()`，为每个 SR_CHANNEL_LOGIC 信号创建 SearchEdgeFlagEdit
  - [x] SubTask 3.3: 设置验证器（[10XRFCxrfc]）、最大长度 1、等宽字体
  - [x] SubTask 3.4: 从 `_pattern` 映射表预填充已有值，无则默认 "X"
  - [x] SubTask 3.5: 将通道名称标签、索引标签、编辑器添加到网格布局
  - [x] SubTask 3.6: 连接每个编辑器的 editingFinished 信号到 on_pattern_changed()

- [x] Task 4: 实现 on_pattern_changed() 槽函数
  - [x] SubTask 4.1: 将编辑器文本转为大写
  - [x] SubTask 4.2: 从编辑器读取所有通道的模式值，更新 `_pattern` 映射表
  - [x] SubTask 4.3: 调用 `_view.set_search_pos(_view.get_search_pos(), false)` 重置搜索光标

- [x] Task 5: 修改 on_previous() 和 on_next() 方法
  - [x] SubTask 5.1: 确保在搜索前从编辑器同步最新 `_pattern` 值（防止遗漏）
  - [x] SubTask 5.2: 其余逻辑保持不变

- [x] Task 6: 更新 retranslateUi()、reStyle()、UpdateFont() 方法
  - [x] SubTask 6.1: retranslateUi() 更新图例标签文本（支持国际化）
  - [x] SubTask 6.2: reStyle() 保持 Previous/Next 按钮图标更新
  - [x] SubTask 6.3: UpdateFont() 更新编辑器字体大小

- [x] Task 7: 处理设备更新时的编辑器重建
  - [x] SubTask 7.1: 连接 `_session->device_event_object()` 的 `device_updated` 信号到重建槽
  - [x] SubTask 7.2: 在重建时保留已有通道的模式值，新增通道默认 "X"
  - [x] SubTask 7.3: 移除 `_pattern` 中已不存在的通道条目

- [x] Task 8: 编译验证
  - [x] SubTask 8.1: 确保项目可以正常编译，无语法错误
  - [x] SubTask 8.2: 确保无未使用变量警告

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 3]
- [Task 5] depends on [Task 4]
- [Task 6] depends on [Task 2]
- [Task 7] depends on [Task 3]
- [Task 8] depends on [Task 1] through [Task 7]
