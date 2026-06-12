# 修复 C Decoder 和 libsigrok 编译警告 Spec

## Why
全量 clean build 后，排除 minizip 和 libsigrok/hardware 后仍有约 45 条编译警告，分布在 C decoder（约 43 条）和 libsigrok/lib_main.c（2 条）。PXView C++ 代码已零警告。

## What Changes
- 修复 C decoder 中 format-truncation 警告：增大 snprintf 目标缓冲区或截断输入
- 修复 C decoder 中 maybe-uninitialized 警告：初始化变量
- 修复 C decoder 中 array-bounds 警告：修复 eeprom93xx_c 的零长度数组访问
- 修复 C decoder 中 cast-function-type 警告：修复 ir_irmp_c 的 FARPROC 转换
- 修复 C decoder 中 unused-variable/parameter 警告
- 修复 C decoder 中 sign-compare/type-limits/parentheses/macro-redefined 警告
- 修复 libsigrok/lib_main.c 的 int-to-pointer-cast 和 pointer-to-int-cast 警告

## Impact
- Affected code: 约 20 个 C decoder 文件, libsigrok/lib_main.c
- 所有修改不改变运行时行为，仅消除编译警告

## ADDED Requirements

### Requirement: C decoder 编译零警告
所有 215 个 C decoder 编译 SHALL 不产生 GCC 警告。

#### Scenario: Clean build 无 C decoder 警告
- **WHEN** 执行全量 clean build
- **THEN** libsigrokdecode/c_decoders/ 下所有 .c 文件编译无 warning

### Requirement: libsigrok 零编译警告
libsigrok 核心 C 代码（非 hardware/）编译 SHALL 不产生 GCC 警告。

#### Scenario: lib_main.c 无类型转换警告
- **WHEN** 编译 libsigrok/lib_main.c
- **THEN** 不产生 int-to-pointer-cast 和 pointer-to-int-cast 警告

## MODIFIED Requirements
（无）

## REMOVED Requirements
（无）

---

## 警告详细清单

### A. format-truncation (约 15 条)
- avclan_c.c: 7 处 snprintf 截断
- arp_c.c: 3 处 snprintf 截断
- i2c_packet_c.c: 2 处 snprintf 截断
- ccd_c.c: 1 处 snprintf 截断

### B. maybe-uninitialized (约 10 条)
- as5047_c.c: mosi_b, miso_b, have_mosi, have_miso
- cc1101_c.c: have_mosi, have_miso, min_b, max_b
- cyrf6936_c.c: have_mosi, have_miso
- adns5020_c.c: have_mosi

### C. array-bounds (8 条)
- eeprom93xx_c.c: 8 处 mw_py_entry[0] 零长度数组越界

### D. cast-function-type (3 条)
- ir_irmp_c.c: 3 处 FARPROC 到具体函数指针的转换

### E. unused-variable/parameter (3 条)
- arm_etmv3_c.c: state_strs
- avr_pdi_c.c: avr_pdi_binary_labels
- cjtag_oscan0_c.c: tck

### F. sign-compare (2 条)
- i2cdemux_c.c: ?: 操作数符号不匹配
- i2cfilter_c.c: ?: 操作数符号不匹配

### G. type-limits (1 条)
- ieee488_c.c: 比较总是 true

### H. unused-but-set-variable (1 条)
- ade77xx_c.c: vali

### I. parentheses (1 条)
- dcc_c.c: & 操作中比较需要括号

### J. macro redefined (1 条)
- amulet_ascii_c.c: 'L' 宏重定义

### K. libsigrok 核心 (2 条)
- lib_main.c:1614: cast from pointer to integer of different size
- lib_main.c:1630: cast to pointer from integer of different size
