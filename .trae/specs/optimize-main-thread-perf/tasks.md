# Tasks

- [x] Task 1: AppConfig 异步延迟保存
  - [x] SubTask 1.1: 在 `appconfig.h` 中添加 `QTimer` 成员和 `doSaveFrame()` / `doSaveApp()` / `doSaveHistory()` 私有槽声明
  - [x] SubTask 1.2: 修改 `appconfig.cpp`，将 `SaveFrame()` / `SaveApp()` / `SaveHistory()` 改为调用 `QTimer::singleShot(2000)` 延迟执行
  - [x] SubTask 1.3: 在 `AppConfig` 构造函数中连接 `QCoreApplication::aboutToQuit` 信号，退出时立即同步保存
  - [x] SubTask 1.4: 添加 `flushPendingSaves()` 公有方法，供需要立即保存的场景调用

- [x] Task 2: SlidingDrawer 绘制优化
  - [x] SubTask 2.1: 在 `SlidingDrawer::open()` 和 `SlidingDrawer::close()` 开始动画时，对自身和 parentWidget 设置 `Qt::WA_OpaquePaintEvent` + `Qt::WA_NoSystemBackground`
  - [x] SubTask 2.2: 在动画结束回调中恢复默认属性（移除 `WA_OpaquePaintEvent`，保留 `WA_NoSystemBackground`）
  - [x] SubTask 2.3: 在 `setSlideOffset()` 中计算脏区域矩形，对 Viewport 调用 `update(dirtyRect)` 替代全量 update

- [x] Task 3: MeasureDock 组件复用
  - [x] SubTask 3.1: 修改 `build_cursor_pannel()` 逻辑：比较当前 `_opt_row_list` 数量与 `cursor_list` 数量
  - [x] SubTask 3.2: 数量相同时仅遍历更新 `info_label->setText()` 和 `goto_bt` 文本
  - [x] SubTask 3.3: 数量增加时追加新组件；数量减少时 `deleteLater()` 多余组件
  - [x] SubTask 3.4: 同样优化 `build_dist_pannel()` 和 `build_edge_pannel()` 的组件复用逻辑

- [x] Task 4: SVG 图标缓存（IconCache）
  - [x] SubTask 4.1: 创建 `iconcache.h/cpp`，实现 `IconCache` 单例类，包含 `QHash<QString, QPixmap>` 缓存和 `pixmap()` / `icon()` 方法
  - [x] SubTask 4.2: 实现 `clearCache()` 方法，在主题切换时调用
  - [x] SubTask 4.3: 替换 `measuredock.cpp` 中所有 `QIcon(iconPath+"/del.svg")` 等为 `IconCache::icon()` 调用
  - [x] SubTask 4.4: 替换 `protocolitemlayer.cpp`、`protocoldock.cpp` 中的 SVG 图标加载为缓存版本
  - [x] SubTask 4.5: 替换 `titlebar.cpp`、`filebar.cpp`、`logobar.cpp`、`trigbar.cpp` 中的 SVG 图标加载为缓存版本
  - [x] SubTask 4.6: 替换 `decodergroupbox.cpp`、`devmode.cpp` 中的 SVG 图标加载为缓存版本
  - [x] SubTask 4.7: 在 `MainWindow::reStyle()` 或主题切换逻辑中调用 `IconCache::clearCache()`

# Task Dependencies
- [Task 2] depends on [Task 1]（动画优化需要先确保 SaveFrame 不阻塞）
- [Task 3] depends on [Task 4]（MeasureDock 组件复用应同时使用 IconCache）
- [Task 4] 无前置依赖，可独立开始
