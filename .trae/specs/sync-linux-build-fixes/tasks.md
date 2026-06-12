# Tasks

- [x] Task 1: 修复 app_service.cpp 的 reinterpret_cast 类型转换错误
  - [x] 将第 83 行 `reinterpret_cast<uintptr_t>(base_info->handle)` 改为 `static_cast<uint64_t>(base_info->handle)`
  - [x] 将第 127 行 `reinterpret_cast<uintptr_t>(item.handle)` 改为 `static_cast<uint64_t>(item.handle)`
  - [x] 将第 159 行 `reinterpret_cast<uintptr_t>(dev->handle())` 改为 `static_cast<uint64_t>(dev->handle())`

- [x] Task 2: 修复 session_service.cpp 的 deprecated API 和未使用参数
  - [x] 将 `QDateTime::fromMSecsSinceEpoch(msecs, Qt::UTC)` 改为 `QDateTime::fromMSecsSinceEpoch(msecs, QTimeZone::UTC)`
  - [x] 在 show_region 函数体中添加 `(void)keep;` 消除未使用参数警告

- [x] Task 3: 修复 rpc_dispatcher.cpp 的 nodiscard 和未使用参数
  - [x] 将第 13 行 `dbg_file.open(...)` 改为 `(void)dbg_file.open(...)`
  - [x] 在 on_list_analyzers 函数体中添加 `(void)params;` 消除未使用参数警告

- [x] Task 4: 修复 ook_oregon_c.c 的未初始化变量警告
  - [x] 添加 `memset(nib_strs, 0, sizeof(nib_strs));` 初始化数组

- [x] Task 5: 修复 instance.c 的 wanted_term 警告
  - [x] 在 di_thread 函数中添加 `(void)wanted_term;` 消除 set-but-not-used 警告
  - [x] 在 c_di_thread 函数中添加 `(void)wanted_term;` 消除 set-but-not-used 警告

- [x] Task 6: 改进 CMakeLists.txt 中 nlohmann_json 构建方式
  - [x] 改为 find_package 优先 + FetchContent fallback 模式

- [x] Task 7: 移除 CMakeLists.txt 中无条件 test_font 目标
  - [x] 注释掉 `add_executable(test_font test_font.cpp)` 和 `target_link_libraries(test_font Qt6::Gui Qt6::Widgets)`

# Task Dependencies
- Task 1-5 相互独立，可并行执行
- Task 6 需要在 Windows 和 Linux 两个平台上验证
- Task 7 独立，可并行执行
