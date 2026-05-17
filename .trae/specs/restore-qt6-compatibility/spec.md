# 恢复 Qt6 兼容性（策略 B：彻底迁移）Spec

## Why
PXView 项目原本支持 Qt5/Qt6 双版本编译，但当前代码库中 Qt6 构建逻辑被注释掉，且部分新增代码缺少 Qt6 兼容保护，导致无法使用 Qt6 编译。需要进行彻底的 Qt6 适配迁移，同时保持 Qt5 向后兼容。

## What Changes
- **BREAKING**: C++ 标准从 c++11 升级到 c++17（Qt6 最低要求）
- 恢复 CMakeLists.txt 中 Qt6 构建逻辑，支持 Qt5/Qt6 双版本条件编译
- 将 `QTextCodec` 全部替换为 `QStringConverter`/`QStringEncoder`（不依赖 Qt5Compat）
- 将 `QSignalMapper` 替换为 lambda connect 模式（不依赖 Qt5Compat）
- 将 `Qt5WinExtras`（QWinTaskbarButton/QWinTaskbarProgress）替换为 Win32 ITaskbarList3 原生 API
- 修复 `nativeEvent` 签名在 Qt6 下的 `long*` → `qintptr*` 变更
- 修复 `QMouseEvent::globalPos()` → `globalPosition().toPoint()` 迁移
- 修复 `QMouseEvent::pos()` 在 Qt6 下返回 `QPointF` 的兼容性
- 修复 `QWheelEvent` 相关 API（已有部分适配，补全遗漏）
- 修复 `QFontDatabase` 静态方法在 Qt6 下的变更
- 修复 High DPI 属性在 Qt6 下已移除的问题
- 修复 `QPixmap::grabWidget()` 在 Qt6 下已移除的问题
- 修复 `QDesktopWidget` 残留引用
- 将 `SIGNAL()/SLOT()` 旧式字符串语法迁移到新式仿函数语法（约 602 处）

## Impact
- Affected specs: 构建系统、所有使用 Qt API 的源文件
- Affected code: CMakeLists.txt、PXView/ 下约 50+ 个源文件

## ADDED Requirements

### Requirement: CMakeLists.txt Qt5/Qt6 双版本构建支持
构建系统 SHALL 支持通过 `QT_VERSION_FORCE` 选项选择 Qt5 或 Qt6，默认自动检测。

#### Scenario: 自动检测 Qt 版本
- **WHEN** 未设置 `QT_VERSION_FORCE` 且系统安装了 Qt5
- **THEN** 使用 Qt5 构建并链接 Qt5::Widgets, Qt5::Svg, Qt5::Concurrent 等

#### Scenario: 强制使用 Qt6
- **WHEN** 设置 `QT_VERSION_FORCE=6`
- **THEN** 使用 Qt6 构建并链接 Qt6::Widgets, Qt6::Svg, Qt6::Concurrent 等（不含 Qt5WinExtras）

#### Scenario: Qt6 下 Windows 任务栏进度
- **WHEN** 在 Windows 上使用 Qt6 构建
- **THEN** 通过 Win32 ITaskbarList3 COM 接口实现任务栏进度显示，不依赖 Qt5WinExtras

### Requirement: nativeEvent 签名兼容
所有 `nativeEvent` 重写 SHALL 在 Qt5 下使用 `long*` 参数，Qt6 下使用 `qintptr*` 参数。

#### Scenario: Qt6 下 nativeEvent 编译
- **WHEN** 使用 Qt6 编译
- **THEN** nativeEvent 的第三个参数类型为 `qintptr*`，虚函数签名与基类匹配

### Requirement: QMouseEvent API 迁移
所有 `QMouseEvent::globalPos()` 调用 SHALL 替换为条件编译：Qt6 使用 `globalPosition().toPoint()`，Qt5 保持 `globalPos()`。所有 `QMouseEvent::pos()` 调用 SHALL 替换为条件编译：Qt6 使用 `position().toPoint()`，Qt5 保持 `pos()`。

#### Scenario: Qt6 下鼠标事件坐标获取
- **WHEN** 使用 Qt6 编译并触发鼠标事件
- **THEN** 通过 `globalPosition().toPoint()` 和 `position().toPoint()` 获取正确的整数坐标

### Requirement: QTextCodec 替换为 QStringConverter
所有 `QTextCodec` 使用 SHALL 替换为 `QStringConverter`/`QStringEncoder`，不依赖 Qt5Compat 模块。所有无用的 `#include <QTextCodec>` SHALL 移除。

#### Scenario: Qt6 下编码转换
- **WHEN** 在 Windows 上使用 Qt6 进行编码转换
- **THEN** 使用 `QStringEncoder`/`QStringDecoder` 替代 `QTextCodec`，功能等价

#### Scenario: Qt6 下无 QTextCodec 头文件引用
- **WHEN** 使用 Qt6 编译
- **THEN** 不引用任何 QTextCodec 头文件，包括无用的 include

### Requirement: QSignalMapper 替换为 lambda
所有 `QSignalMapper` 使用 SHALL 替换为 lambda connect 模式。所有无用的 `#include <QSignalMapper>` SHALL 移除。

#### Scenario: DecoderMenu 信号映射
- **WHEN** 用户在 DecoderMenu 中选择一个解码器
- **THEN** 通过 lambda 捕获 action 对象，正确传递到 `on_action` 槽函数

#### Scenario: Qt6 下无 QSignalMapper 头文件引用
- **WHEN** 使用 Qt6 编译
- **THEN** 不引用任何 QSignalMapper 头文件，包括无用的 include

### Requirement: QFontDatabase API 迁移
`QFontDatabase::addApplicationFont()` 和 `QFontDatabase::applicationFontFamilies()` SHALL 在 Qt6 下使用实例方法调用。

#### Scenario: Qt6 下字体加载
- **WHEN** 使用 Qt6 编译并加载应用字体
- **THEN** 通过 `QFontDatabase db; db.addApplicationFont(...)` 实例方法加载字体

### Requirement: High DPI 属性兼容
`Qt::AA_EnableHighDpiScaling`、`Qt::AA_DisableHighDpiScaling`、`Qt::AA_UseHighDpiPixmaps` SHALL 仅在 Qt5 下设置，Qt6 下不设置（Qt6 默认启用高 DPI）。

#### Scenario: Qt6 下高 DPI 行为
- **WHEN** 使用 Qt6 在高 DPI 显示器上运行
- **THEN** 自动启用高 DPI 缩放，不调用已移除的 AA_ 属性

### Requirement: SIGNAL/SLOT 旧式语法迁移
所有 `SIGNAL()/SLOT()` 字符串连接语法 SHALL 替换为 Qt5/6 兼容的新式仿函数语法。

#### Scenario: 编译期信号槽检查
- **WHEN** 信号或槽签名发生变化
- **THEN** 编译器在编译期报错而非运行时

### Requirement: QDesktopWidget 残留清理
所有 `#include <QDesktopWidget>` 残留 SHALL 添加 Qt 版本守卫或移除。

#### Scenario: Qt6 下编译无 QDesktopWidget 错误
- **WHEN** 使用 Qt6 编译
- **THEN** 不引用任何 QDesktopWidget 头文件或 API

### Requirement: QPixmap::grabWidget 替换
`QPixmap::grabWidget()` SHALL 替换为 `QWidget::grab()`。

#### Scenario: Qt6 下截图功能
- **WHEN** 在 Qt6 下调用截图功能
- **THEN** 使用 `widget->grab()` 获取控件截图

## MODIFIED Requirements

### Requirement: C++ 编译标准
C++ 编译标准从 `c++11` 修改为 `c++17`。Qt6 最低要求 C++17，Qt5 也支持 C++17。

## REMOVED Requirements

### Requirement: Qt5WinExtras 依赖
**Reason**: Qt6 已移除 Qt5WinExtras 模块
**Migration**: Windows 任务栏进度功能改用 Win32 ITaskbarList3 COM 接口实现，通过条件编译在 Qt5/Qt6 下均可用

## 当前实施状态

### 已完成（前一轮会话）
- CMakeLists.txt Qt5/Qt6 双版本构建逻辑 ✅
- qtcompat.h 兼容性辅助头文件 ✅
- nativeEvent 签名修复（mainframe, submainframe, winshadow）✅
- WinTaskbarProgress 替代 Qt5WinExtras ✅
- QMouseEvent::globalPos() 大部分迁移 ✅
- QMouseEvent::pos() 大部分迁移 ✅
- QTextCodec → QStringConverter（encoding.cpp, path.cpp）✅
- QSignalMapper → lambda（decodermenu.cpp 实现）✅
- QFontDatabase 迁移（main.cpp）✅
- High DPI 属性守卫（main.cpp）✅
- QDesktopWidget 清理 ✅
- QPixmap::grabWidget 替换 ✅
- SIGNAL/SLOT 全量迁移（514+ 处）✅
- Qt5 编译验证通过 ✅

### 仍需修复（本轮发现 10 处遗漏）
1. **logdock.cpp:37** — 无用 `#include <QTextCodec>` 无版本守卫
2. **decodermenu.h:27** — 无用 `#include <QSignalMapper>` 无版本守卫
3. **titlebar.cpp:649** — `event->pos()` 未使用 QT_COMPAT_POS 宏
4. **titlebar.cpp:657-658** — `event->pos().x()/y()` 未使用 QT_COMPAT_X/Y 宏
5. **titlebar.cpp:666** — `event->globalPos()` 未使用 QT_COMPAT_GLOBAL_POS 宏
6. **mainframe.cpp:475** — `mouse_event->globalPos()` 未使用 QT_COMPAT_GLOBAL_POS 宏
7. **submainframe.cpp:557** — `mouse_event->globalPos()` 未使用 QT_COMPAT_GLOBAL_POS 宏
8. **slidingdrawer.cpp:513** — `event->globalPos()` 未使用 QT_COMPAT_GLOBAL_POS 宏
9. **Qt6 编译验证** — 尚未在 Qt6 环境下验证编译
10. **viewport.cpp 滚轮事件** — 使用内联版本检查而非 qtcompat.h 宏（风格不统一，功能正确）
