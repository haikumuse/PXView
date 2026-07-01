# 验证清单

## 缺陷重构：std_session_send_df_frame_begin 收编

- [x] 已读取并对比 5 个驱动（fx2lafw/gwinstek-gds-800/hameg-hmo/hantek-dso/hung-chang-dso-2100）的本地 `std_session_send_df_frame_begin` 实现，确认语义等价（两变体：ds_data_forward+SR_PKT_OK / sr_session_send+SR_PKT_OK，等价）
- [x] `compat_helpers.h` 已添加 `std_session_send_df_frame_begin(const struct sr_dev_inst *sdi)` 声明（line 214）
- [x] `compat_helpers.c` 已添加 `std_session_send_df_frame_begin` 单一规范实现（line 236，发送 SR_DF_FRAME_BEGIN packet，使用 ds_data_forward 模式）
- [x] `fx2lafw/protocol.h` 声明已删除，`fx2lafw/protocol.c` 定义已删除
- [x] `gwinstek-gds-800/protocol.h` 声明已删除，`gwinstek-gds-800/protocol.c` 定义已删除
- [x] `hameg-hmo/protocol.h` 声明已删除，`hameg-hmo/protocol.c` 定义已删除
- [x] `hantek-dso/protocol.h` 声明已删除，`hantek-dso/protocol.c` 定义已删除
- [x] `hung-chang-dso-2100/protocol.c` 定义已删除（protocol.h 本来无声明）
- [x] 全局 grep 验证：`std_session_send_df_frame_begin` 本地定义仅剩 compat_helpers.c 一处（5 个目标驱动仅剩调用点；另有 9 个非目标驱动有本地定义但当前均 OFF，不在本 spec 范围）

## sipeed-slogic-analyzer 迁移

- [x] `libsigrok/hardware/sipeed-slogic-analyzer/protocol.h` 已创建（include 为 compat.h，保留 dev_context/slogic_model/函数声明）
- [x] `libsigrok/hardware/sipeed-slogic-analyzer/protocol.c` 已创建（include 为 compat.h；df_header/end 用 2-arg；session_source_add 5-arg；无本地 std_session_send_df_frame_begin 定义；libusb 调用保留；handle_events 签名改为 const struct sr_dev_inst *；sipeed_slogic_acquisition_stop 参数加 const；local_std_u64_idx helper 替代 3-arg std_u64_idx）
- [x] `libsigrok/hardware/sipeed-slogic-analyzer/api.c` 已创建（含 8 个 compat 包装函数；结构体无 .config_channel_set/.dev_clear；.priv = NULL；config_channel_set 逻辑合并进 config_set；local_std_str_idx/local_std_u64_idx/local_std_i32_idx helpers；std_gvar_min_max_step_thresholds 5-arg；sr_session_send 宏绕过；soft_trigger_logic 无 unitsize）
- [x] driver_info 结构体命名为 `sipeed_slogic_analyzer_driver_info`（api.c:662 定义，与目录名对齐）

## 构建集成

- [x] `CMakeLists.txt` 已添加 `option(ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER ...)`（line 622）
- [x] `CMakeLists.txt` 已添加源文件条目（line 1047-1052，api.c + protocol.c）
- [x] `CMakeLists.txt` 已添加 `add_definitions(-DHAVE_DRIVER_SIPEED_SLOGIC_ANALYZER)`（line 753-755）
- [x] `libsigrok/hwdriver.c` 已添加 extern 声明（line 175-177，由 HAVE_DRIVER_SIPEED_SLOGIC_ANALYZER 守卫）
- [x] `libsigrok/hwdriver.c` 已在 drivers_list 添加 `&sipeed_slogic_analyzer_driver_info`（line 366-368）

## 编译验证

- [x] `cmake` 重新配置成功（ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER=ON，cache line 472）
- [x] `ninja -j 16` 编译通过，无 error（仅剩良性 warning：LOG_PREFIX 重定义、sign-compare、discarded-qualifiers）
- [x] 5 个被删本地定义的驱动：当前均 OFF 不参与编译；启用 fx2lafw 验证（见下条）通过
- [x] sipeed-slogic-analyzer 的 api.c / protocol.c 编译通过（.obj 产物生成）
- [x] 最终链接成功（PXView.exe 生成，时间戳 2026-07-01 15:01:47，无 multiple definition / undefined reference 错误）
- [x] **缺陷验证**：同时启用 fx2lafw + sipeed-slogic（两个调用 `std_session_send_df_frame_begin` 的驱动）编译链接成功，968/968 通过，**multiple definition 缺陷已消除**
