# 修复所有编译器警告 Spec

## Why
项目当前构建产生大量编译器警告（约150+条），涵盖未使用变量/参数、类型不匹配、已弃用API、格式截断、初始化顺序等多种类别。这些警告降低代码质量、掩盖真正的错误，且部分警告（如未初始化变量、整数溢出）可能导致运行时问题。

## What Changes
- 修复 CMake 配置警告（cmake_minimum_required 版本、CMP0148/CMP0167 策略、未使用变量）
- 修复 MOC "No relevant classes found" 警告（移除不必要的 Q_OBJECT 宏）
- 修复 c_decoder_api.c 中 c_decoder_put_python() 的未使用变量（cb, pdata, pda）
- 修复各 C decoder 的警告（未使用变量/参数、格式截断、缺失字段初始化器等）
- 修复 PXView C++ 代码警告（已弃用 Qt API、初始化顺序、未使用变量/参数、符号比较等）
- 修复 libsigrokdecode C 代码警告（_POSIX_C_SOURCE 重定义、已弃用 Python API、函数类型转换等）

## Impact
- Affected code: CMakeLists.txt, libsigrokdecode/c_decoder_api.c, 37个 C decoder 文件, PXView/pv/* (约30个文件), libsigrokdecode/* (约10个文件)
- 所有修改均为警告消除，不改变任何运行时行为

## 排除范围
- **common/minizip** — 第三方库代码，不修改
- **libsigrok C 代码** — 硬件驱动层代码，不修改

## ADDED Requirements

### Requirement: 消除 CMake 配置警告
系统构建配置 SHALL 不产生 CMake 警告。

#### Scenario: CMake 配置无警告
- **WHEN** 执行 cmake 配置
- **THEN** 无 CMake Deprecation Warning、Policy Warning 或未使用变量警告

### Requirement: 消除 MOC "No relevant classes found" 警告
MOC 处理头文件 SHALL 不产生 "No relevant classes found" 警告。

#### Scenario: MOC 无冗余警告
- **WHEN** 构建系统运行 MOC 处理头文件
- **THEN** 不出现 "No relevant classes found" 输出

### Requirement: 消除 C Decoder 和 libsigrokdecode C 代码编译警告
所有 C decoder 和 libsigrokdecode C 源文件编译 SHALL 不产生 -W 开头的编译器警告。

#### Scenario: C 代码无警告编译
- **WHEN** 使用 GCC 编译 C decoder 和 libsigrokdecode C 源文件
- **THEN** 不产生 unused-variable, unused-parameter, unused-but-set-variable, sign-compare, format-truncation, stringop-truncation, missing-field-initializers, deprecated-declarations, cast-function-type 等警告

### Requirement: 消除 C++ 代码编译警告
所有 PXView C++ 源文件编译 SHALL 不产生编译器警告。

#### Scenario: C++ 代码无警告编译
- **WHEN** 使用 GCC 编译所有 C++ 源文件
- **THEN** 不产生 unused-variable, unused-parameter, unused-but-set-variable, sign-compare, reorder, deprecated-declarations, switch, nodiscard/unused-result, delete-non-virtual-dtor 等警告

### Requirement: 消除第三方库集成警告
第三方库（Python、Boost）集成 SHALL 不产生警告。

#### Scenario: Python 头文件无重定义警告
- **WHEN** 编译包含 Python.h 的 libsigrokdecode 源文件
- **THEN** 不产生 _POSIX_C_SOURCE 重定义警告

#### Scenario: Boost 无弃用警告
- **WHEN** 编译包含 boost/bind.hpp 的源文件
- **THEN** 不产生 BOOST_BIND_GLOBAL_PLACEHOLDERS 弃用 pragma 消息

## MODIFIED Requirements
（无修改的已有需求）

## REMOVED Requirements
（无移除的已有需求）

---

## 警告分类详细清单

### A. CMake 配置警告 (5项)
1. CMakeLists.txt:22 - cmake_minimum_required 版本 < 3.10 弃用警告
2. CMakeLists.txt:121 - CMP0148: FindPythonInterp/FindPythonLibs 已移除
3. CMakeLists.txt:196 - CMP0167: FindBoost 模块已移除
4. CMakeLists.txt:180 - Qt6 GuiPrivate 模块警告
5. 未使用变量 QT_VERSION_FORCE

### B. MOC "No relevant classes found" (23项)
sessionmanager.h, log.h, icontextaware.h, sigsession.h, datasource.h, decodermodel.h, appcontrol.h, appconfig.h, annotationrestable.h, decoderstatus.h, msgbox.h, ZipMaker.h, dsvdef.h, fn.h, iconcache.h, sessiondocument.h, array.h, tabcontext.h, encoding.h, sessionsnapshot.h, path.h, deviceagent.h, winnativewidget.h

### C. c_decoder_api.c - c_decoder_put_python() 未使用变量 (3项，重复37次)
1. 行396: unused variable 'cb'
2. 行397: variable 'pdata' set but not used
3. 行398: unused variable 'pda'

### D. C Decoder 特定警告 (16项)
1. i2c_c.c:215-218 - format-truncation (2), stringop-truncation (4)
2. swd_c.c:268 - unused variable parity_bit
3. swd_c.c:366 - format-truncation
4. mdio_c.c:335 - unused parameters di, samplenum
5. dmx512_c.c:217,222 - unused-but-set-variable bit_pos, bit_start
6. ir_nec_c.c:211-218 - format-truncation (6)
7. spdif_c.c:102 - unused function get_pulse_type_for_width
8. usb_signalling_c.c:497 - missing field initializer recv_proto
9. can_fd_c.c:127 - unused parameter num_txts
10. iso7816_c.c:590 - unused variable byte_val
11. lpc_c.c:101 - unused variable lpc_size_names
12. lm75_c.c:125,241 - unused parameters rw, di
13. ds1307_c.c:366 - unused parameter di
14. ds3231_c.c:147 - format-truncation
15. ds3231_c.c:595 - unused parameter di
16. numbers_and_state_c.c:330 - unused parameter pattern

### ~~E. common/minizip 警告~~ — 排除，不修复

### F. PXView C++ 警告 (约40项)
1. bool.cpp:62 - deprecated QCheckBox::stateChanged
2. sessiondocument.h:148 - member init order (_decoder_model vs _dock_sample_rate)
3. main.cpp:91-92 - int-to-pointer-cast, unused value
4. deviceoptions.cpp:76 - deprecated QCheckBox::stateChanged
5. deviceoptions.cpp:763 - sign-compare
6. ruler.cpp:741 - unused-but-set-variable i
7. cursor.cpp:50 - unused parameter order
8. mainwindow.cpp:1767 - deprecated QKeyCombination::operator int()
9. mainwindow.cpp:2068,2070 - unused result QTranslator::load (2)
10. viewport.h/cpp - member init order (2组), unused variable cursor_list (3), unused variable mode, -Wswitch (2), unused parameter event
11. view.cpp - unused variable max_height, unused parameter event, unused parameter color, sign-compare
12. dsldial.cpp:103 - sign-compare
13. boost/bind.hpp - deprecated placeholders pragma (4个文件)
14. measuredock.cpp - deprecated QCheckBox::stateChanged (3), unused-but-set-variable bkColor
15. logobar.cpp:206 - unused parameter enable
16. searchdock.cpp - unused parameters (6)
17. deviceoptionsdock.cpp - unused result QtConcurrent::run (2), sign-compare
18. storeprogress.cpp - sign-compare (4), unused variable cursor_list
19. titlebar.cpp:712 - unused-but-set-variable rect
20. storesession.cpp - unused variable status, unused result QFile::open, unused variable read_buf
21. protocolexp.cpp:202 - unused result QFile::open
22. mainframe.cpp:415-418 - unused variables (4)
23. submainframe.cpp:144,183 - delete non-virtual destructor
24. annotationrestable.cpp - stringop-truncation (2)
25. langresource.cpp:165 - unused result QFile::open
26. keywordlineedit.cpp:238 - unused-but-set-variable pt
27. dscombobox.cpp:152 - unused-but-set-variable height
28. winshadow.cpp:175,182 - unused variable isActiveWindow, unused parameter event
29. winnativewidget.cpp:392 - unused parameter wParam

### ~~G. libsigrok C 警告~~ — 排除，不修复

### H. libsigrokdecode C 警告 (约12项)
1. _POSIX_C_SOURCE redefined (7个文件)
2. srd.c:301 - deprecated PyEval_InitThreads
3. decoder.c:1383,1384 - cast-function-type (2)
