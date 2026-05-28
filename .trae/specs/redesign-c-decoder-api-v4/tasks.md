# Tasks

- [x] Task 1: 实现 v4 核心 API — libsigrokdecode.h 头文件
  - [x] 删除旧 API 声明（c_cond_* 系列、recv_proto、c_decoder_get_pin、c_cond_wait_current、旧式长名称 API）
  - [x] 新增 SRD_C_DECODER_API_VERSION = 4
  - [x] 新增 c_wait() 声明和 H/L/R/F/E/N/SKIP/OR/END 宏
  - [x] 新增 di_samplenum(di) / di_matched(di) 宏
  - [x] 新增 c_pin() 声明
  - [x] 新增 c_put / c_put_v 宏
  - [x] 新增 c_field 结构体、c_field_type 枚举、C_U8~C_BYTES 宏
  - [x] 新增 c_proto() 声明
  - [x] 新增 c_opt_bool() 声明
  - [x] 新增快捷 API 别名（c_opt_int/c_opt_str/c_opt_dbl/c_has_ch/c_samplerate 等）
  - [x] 修改 srd_c_decoder 结构体：移除 recv_proto，新增 decode_upper 和 state_size
  - [x] 新增 C_DECODER_STATE 宏定义
  - [x] 新增 C_DECODER_DEFINE 宏定义
  - [x] 保留 C_ANN_PUT / C_ANN_PUT_VAL / C_ANN_PUT_TYPE 宏不变

- [x] Task 2: 实现 v4 核心 API — c_decoder_api.c 实现
  - [x] 删除旧实现（c_cond_new/rise/fall/high/low/edge/noedge/skip/or/wait/free、c_cond_wait_current、c_decoder_put_proto、c_decoder_get_pin 等）
  - [x] 实现 c_wait()：va_list 遍历参数，栈上构建条件列表，调用底层 wait，缓存引脚值
  - [x] 实现 c_pin()：从 di->c_pin_cache 读取
  - [x] 实现 c_proto()：va_list 收集 c_field 参数，遍历 next_di 调用 decode_upper 或 Python 桥接
  - [x] 实现 c_opt_bool()：支持 "yes"/"true"/"1" → 1, "no"/"false"/"0" → 0
  - [x] 实现快捷 API 别名函数（c_opt_int/c_opt_str/c_has_ch/c_samplerate 等）

- [x] Task 3: 修改引擎层适配 v4
  - [x] 修改 instance.c：C→C 堆叠调用路径从 recv_proto 改为 decode_upper
  - [x] 修改 decoder.c：srd_c_decoder_register() 适配新结构体（state_size 字段）
  - [x] 修改 type_decoder.c：Python→C 桥接路径适配 c_field 输出
  - [x] 修改 srd_c_decoder_load()：版本检查从 3 改为 4

- [x] Task 4: 适配测试框架
  - [x] 修改 decoder_test.c：适配 decode_upper 调用路径
  - [x] 验证 run_all_tests.py 对比逻辑与 v4 兼容
  - [x] 验证 test_factory.py 测试数据生成与 v4 兼容

- [x] Task 5: Phase 1 迁移 — 核心底层解码器 (10 个)
  - [x] i2c_c, spi_c, uart_c, jtag_c, swd_c, can_c, can_fd_c, onewire_link_c, ps2_c, usb_signalling_c
  - [x] 从 Python 重新翻译，使用 c_wait/c_proto/C_DECODER_STATE/C_DECODER_DEFINE

- [x] Task 6: Phase 2 迁移 — 二级底层解码器 (15 个)
  - [x] nrzi_c, 4b5b_c, ethernet_c, ipv4_c, onewire_network_c, usb_packet_c, i2s_c, hdlc_c, mdio_c, microwire_c, iso7816_c, iebus_c, ook_c, afsk_c, tmc_c

- [x] Task 7: Phase 3 迁移 — 剩余底层解码器 (88 个)
  - [x] 批量机械迁移所有 inputs=['logic'] 的解码器

- [x] Task 8: Phase 4 迁移 — 上层解码器 (102 个)
  - [x] 批量机械迁移所有上层解码器

- [x] Task 9: Phase 5 迁移 — 特殊前缀版本 (6 个)
  - [x] 0_i2c_c, 0_spi_c, 0_uart_c, 1_i2c_c, 1_spi_c, 1_uart_c

- [x] Task 10: 编译验证与修复
  - [x] 全部 215 个 C 解码器 + 核心库编译通过，0 错误
  - [x] 修复约 30+ 个文件的迁移脚本 bug（di_samplenum参数名、fields变量名冲突、c_field类型访问、缺失data_len成员等）

- [ ] Task 11: 全量对比测试与修复
  - [ ] 运行 `python run_all_tests.py --all` 全量对比测试
  - [ ] 修复所有 FAIL 和 ERROR 状态的解码器
  - [ ] 确认 WARN 状态解码器的测试数据有效性
  - [ ] 生成最终测试报告

# Task Dependencies
- Task 1-9: 已完成
- Task 10: 已完成（编译通过）
- Task 11: 依赖 Task 10（全量测试需要所有解码器编译通过）
