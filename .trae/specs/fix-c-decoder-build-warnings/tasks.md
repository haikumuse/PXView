# Tasks

- [x] Task 1: 修复 format-truncation 警告（avclan_c, arp_c, i2c_packet_c, ccd_c）
  - [x] avclan_c.c: 增大目标缓冲区 t2[64]→256, t[128]→256
  - [x] arp_c.c: 增大目标缓冲区 t[64]→256, t2[32]→128
  - [x] i2c_packet_c.c: 增大 final_short 为 MAX_STR_LEN*2
  - [x] ccd_c.c: 增大 t3[32]→64

- [x] Task 2: 修复 maybe-uninitialized 警告（as5047_c, cc1101_c, cyrf6936_c, adns5020_c）
  - [x] as5047_c.c: 初始化 mosi_b=0, miso_b=0, have_mosi=0, have_miso=0
  - [x] cc1101_c.c: 初始化 have_mosi=0, have_miso=0, min_b=0, max_b=0
  - [x] cyrf6936_c.c: 初始化 have_mosi=0, have_miso=0
  - [x] adns5020_c.c: 初始化 have_mosi=0, have_miso=0

- [x] Task 3: 修复 eeprom93xx_c.c array-bounds 警告（8 处）
  - [x] 修复 entries 从 NULL 初始化改为正确从 fields[0].bytes 提取数据

- [x] Task 4: 修复 ir_irmp_c.c cast-function-type 警告（3 处）
  - [x] 使用中间 void* 转换：(specific_func_ptr)(void*)GetProcAddress(...)

- [x] Task 5: 修复 unused-variable/parameter 警告
  - [x] arm_etmv3_c.c: 添加 (void)state_strs
  - [x] avr_pdi_c.c: 添加 (void)avr_pdi_binary_labels
  - [x] cjtag_oscan0_c.c: 添加 (void)tck

- [x] Task 6: 修复 sign-compare 警告（i2cdemux_c, i2cfilter_c）
  - [x] i2cdemux_c.c: 将 n_fields 转为 uint64_t 匹配 sizeof
  - [x] i2cfilter_c.c: 同上

- [x] Task 7: 修复其他零散警告
  - [x] ieee488_c.c: 移除冗余的 b >= 0x00 比较
  - [x] ade77xx_c.c: 添加 (void)vali
  - [x] dcc_c.c: 添加括号 ((x >> 3) & 1) == 0
  - [x] amulet_ascii_c.c: 添加 #undef L

- [x] Task 8: 修复 libsigrok/lib_main.c 类型转换警告
  - [x] 修复 cast from pointer: 使用 uintptr_t 替代 unsigned long
  - [x] 修复 cast to pointer: 使用 (gpointer)(uintptr_t) 替代

# Task Dependencies
- Task 1-8 相互独立，可并行执行
