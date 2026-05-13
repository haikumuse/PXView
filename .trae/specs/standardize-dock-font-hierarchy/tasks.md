# Tasks

- [x] Task 1: 新增字号常量和辅助函数
  - [x] SubTask 1.1: 创建 `PXView/pv/ui/dockfonts.h`，定义 `DockFontSizes` 命名空间常量（MainTitle=18, SectionTitle=16, Label=14, Content=12）和辅助函数声明（`dock_font_main_title()`、`dock_font_section_title()`、`dock_font_label()`、`dock_font_content()`）
  - [x] SubTask 1.2: 在 `PXView/pv/ui/fn.cpp` 中实现辅助函数，返回对应 pixelSize 的 QFont 对象
  - [x] SubTask 1.3: 在 `PXView/pv/ui/fn.h` 中声明 `set_dock_form_font(QWidget *wid)` 函数
  - [x] SubTask 1.4: 在 `PXView/pv/ui/fn.cpp` 中实现 `set_dock_form_font()`：QLabel（非 dock_section_title）用 14px，QPushButton/QComboBox/QSpinBox/QCheckBox/QRadioButton/QLineEdit 用 12px，名为 dock_section_title 的 QLabel 用 16px，QTabWidget/QGroupBox 用 14px

- [x] Task 2: QSS 字号层级定义
  - [x] SubTask 2.1: 在 dark.qss 的 Color Tokens 注释区新增字号令牌说明（@dock-font-main-title: 18px, @dock-font-section-title: 16px, @dock-font-label: 14px, @dock-font-content: 12px）
  - [x] SubTask 2.2: 在 light.qss 的 Color Tokens 注释区新增同样的字号令牌说明
  - [x] SubTask 2.3: 在 dark.qss 中修改 `#sliding_drawer_title` 的 font-size 从 13pt 改为 18px
  - [x] SubTask 2.4: 在 dark.qss 中为 `#dock_section_title` 添加 `font-size: 16px`
  - [x] SubTask 2.5: 在 dark.qss 中新增 `#dock_label { font-size: 14px; }` 选择器
  - [x] SubTask 2.6: 在 dark.qss 中新增 `#dock_content { font-size: 12px; }` 选择器
  - [x] SubTask 2.7: 在 light.qss 中做与 SubTask 2.3-2.6 相同的修改

- [x] Task 3: DeviceOptionsDock 字号层级改造
  - [x] SubTask 3.1: 在 `deviceoptionsdock.cpp` 中 `#include "ui/dockfonts.h"`，替换所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 为层级字号函数调用
  - [x] SubTask 3.2: 为属性标签 QLabel 添加 objectName `"dock_label"`
  - [x] SubTask 3.3: 为 QComboBox / QPushButton / QSpinBox / QCheckBox / QRadioButton 添加 objectName `"dock_content"`
  - [x] SubTask 3.4: 替换 `ui::set_form_font(this, font)` 为 `ui::set_dock_form_font(this)`
  - [x] SubTask 3.5: 修改 `get_property_form()` 中标签和控件的字号设置
  - [x] SubTask 3.6: 修改 `build_glitch_filter_panel()` 中标签和控件的字号设置
  - [x] SubTask 3.7: 修改 `logic_probes()` / `analog_probes()` / `dynamic_widget()` 中控件的字号设置

- [x] Task 4: SamplingBar 采样设置字号层级改造
  - [x] SubTask 4.1: 在 `samplingbar.cpp` 中 `#include "ui/dockfonts.h"`，替换 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 为层级字号函数调用
  - [x] SubTask 4.2: 为采样设置中的标签（"设备"、"采样深度"等）添加 objectName `"dock_label"`
  - [x] SubTask 4.3: 为采样设置中的 QComboBox 添加 objectName `"dock_content"`
  - [x] SubTask 4.4: 修改 `createSamplingSettingsWidget()` 中标题标签使用 `dock_font_section_title()`

- [x] Task 5: MeasureDock 字号层级改造
  - [x] SubTask 5.1: 在 `measuredock.cpp` 中 `#include "ui/dockfonts.h"`，替换所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 为层级字号函数调用
  - [x] SubTask 5.2: 为标签添加 objectName `"dock_label"`，为按钮和下拉框添加 objectName `"dock_content"`
  - [x] SubTask 5.3: 替换 `ui::set_form_font(this, font)` 为 `ui::set_dock_form_font(this)`
  - [x] SubTask 5.4: 修改 `UpdateFont()` 和 `adjust_form_size()` 中的字号设置

- [x] Task 6: TriggerDock 字号层级改造
  - [x] SubTask 6.1: 在 `triggerdock.cpp` 中 `#include "ui/dockfonts.h"`，替换所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 为层级字号函数调用
  - [x] SubTask 6.2: 为标签添加 objectName `"dock_label"`，为输入框添加 objectName `"dock_content"`
  - [x] SubTask 6.3: 替换 `ui::set_form_font(this, font)` 为 `ui::set_dock_form_font(this)`
  - [x] SubTask 6.4: 修改 `UpdateFont()` 中的字号设置

- [x] Task 7: DsoTriggerDock 字号层级改造
  - [x] SubTask 7.1: 在 `dsotriggerdock.cpp` 中 `#include "ui/dockfonts.h"`，替换 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 为层级字号函数调用
  - [x] SubTask 7.2: 为标签添加 objectName `"dock_label"`，为下拉框和按钮添加 objectName `"dock_content"`
  - [x] SubTask 7.3: 替换 `ui::set_form_font(this, font)` 为 `ui::set_dock_form_font(this)`
  - [x] SubTask 7.4: 修改 `UpdateFont()` 中的字号设置

- [x] Task 8: ProtocolDock 字号层级改造
  - [x] SubTask 8.1: 在 `protocoldock.cpp` 中 `#include "ui/dockfonts.h"`，替换所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 为层级字号函数调用
  - [x] SubTask 8.2: 为标签添加 objectName `"dock_label"`，为下拉框和按钮添加 objectName `"dock_content"`
  - [x] SubTask 8.3: 替换 `ui::set_form_font(this, font)` 为 `ui::set_dock_form_font(this)`
  - [x] SubTask 8.4: 修改 `UpdateFont()` 和 `adjustPannelSize()` 中的字号设置

- [x] Task 9: SearchDock 字号层级改造
  - [x] SubTask 9.1: 在 `searchdock.cpp` 中 `#include "ui/dockfonts.h"`，替换 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 为层级字号函数调用
  - [x] SubTask 9.2: 为标签添加 objectName `"dock_label"`，为输入框和下拉框添加 objectName `"dock_content"`
  - [x] SubTask 9.3: 修改 `UpdateFont()` 中的字号设置

- [ ] Task 10: 编译验证
  - [ ] SubTask 10.1: 确保项目正常编译，无语法错误
  - [ ] SubTask 10.2: 确保暗色主题下所有 Dock 页面字号层级正确显示
  - [ ] SubTask 10.3: 确保亮色主题下所有 Dock 页面字号层级正确显示
  - [ ] SubTask 10.4: 确保字号层级在 DeviceOptionsDock 各分节中正确区分
  - [ ] SubTask 10.5: 确保 C++ 代码中无 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 残留（Dock 目录和 SamplingBar）

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1]
- [Task 5] depends on [Task 1]
- [Task 6] depends on [Task 1]
- [Task 7] depends on [Task 1]
- [Task 8] depends on [Task 1]
- [Task 9] depends on [Task 1]
- [Task 10] depends on [Task 2] through [Task 9]
- [Task 3] [Task 4] [Task 5] [Task 6] [Task 7] [Task 8] [Task 9] can be done in parallel after Task 1
