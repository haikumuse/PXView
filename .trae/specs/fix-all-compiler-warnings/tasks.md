# Tasks

- [x] Task 1: 修复 CMake 配置警告
  - [x] 1.1: 将 cmake_minimum_required VERSION 从 2.8 提升到 3.10+（CMakeLists.txt:22）
  - [x] 1.2: 设置 CMP0148 策略（CMakeLists.txt:121，替换 find_package(PythonInterp) 为 FindPython3）
  - [x] 1.3: 设置 CMP0167 策略（CMakeLists.txt:196，替换 find_package(Boost) 或设置 cmake_policy）
  - [x] 1.4: 添加 QT_NO_PRIVATE_MODULE_WARNING=ON 抑制 GuiPrivate 警告
  - [x] 1.5: QT_VERSION_FORCE 不在 CMakeLists.txt 中使用，无需修改

- [x] Task 2: 修复 MOC "No relevant classes found" 警告
  - [x] 2.1: 检查23个头文件，将22个不需要MOC的头文件从MOC处理列表移除，为decodermodel.h添加Q_OBJECT

- [x] Task 3: 修复 c_decoder_api.c 中 c_decoder_put_python() 未使用变量
  - [x] 3.1: 移除 c_decoder_put_python() 中未使用的 cb, pdata, pda 变量（行396-398）

- [x] Task 4: 修复 C Decoder 特定警告
  - [x] 4.1: i2c_c.c - 增大 pkt_str/pkt_short 缓冲区到512字节
  - [x] 4.2: swd_c.c - 移除未使用的 parity_bit 变量；增大 ptext 缓冲区到16字节
  - [x] 4.3: mdio_c.c - 添加 (void)di; (void)samplenum;
  - [x] 4.4: dmx512_c.c - 移除未使用的 bit_pos 和 bit_start 变量
  - [x] 4.5: ir_nec_c.c - 添加 hex_width 上限检查 (if > 8 then = 8)
  - [x] 4.6: spdif_c.c - 添加 __attribute__((unused))
  - [x] 4.7: usb_signalling_c.c - 添加 .recv_proto = NULL 初始化器
  - [x] 4.8: can_fd_c.c - 添加 (void)num_txts;
  - [x] 4.9: iso7816_c.c - 移除未使用的 byte_val 变量
  - [x] 4.10: lpc_c.c - 添加 __attribute__((unused))
  - [x] 4.11: lm75_c.c - 添加 (void)rw; (void)di;
  - [x] 4.12: ds1307_c.c - 添加 (void)di;
  - [x] 4.13: ds3231_c.c - 增大 t2 缓冲区到256字节；添加 (void)di;
  - [x] 4.14: numbers_and_state_c.c - 添加 (void)pattern;

- [x] Task 5: 修复 PXView C++ 警告 - Qt 弃用 API
  - [x] 5.1: bool.cpp - 将 QCheckBox::stateChanged 替换为 checkStateChanged
  - [x] 5.2: deviceoptions.cpp:76 - 将 QCheckBox::stateChanged 替换为 checkStateChanged
  - [x] 5.3: measuredock.cpp - 将3处 QCheckBox::stateChanged 替换为 checkStateChanged
  - [x] 5.4: mainwindow.cpp:1767 - 将 QKeyCombination::operator int() 替换为 QKeyCombination + toCombined()

- [x] Task 6: 修复 PXView C++ 警告 - 成员初始化顺序
  - [x] 6.1: sessiondocument.cpp - 调整构造函数初始化列表顺序匹配声明顺序
  - [x] 6.2: viewport.h - 调整 _curVOffset 和 _xcurs_moved；调整 _paint_in_this_second 和 g_drag_active 声明顺序

- [x] Task 7: 修复 PXView C++ 警告 - 未使用变量/参数
  - [x] 7.1: main.cpp:91-92 - (void*)(argc) → (void)argc, (void*)(argv) → (void)argv
  - [x] 7.2: ruler.cpp:741 - 移除未使用的变量 i
  - [x] 7.3: cursor.cpp:50 - 添加 (void)order;
  - [x] 7.4: viewport.cpp - 移除3处未使用的 cursor_list，移除未使用的 mode，添加 (void)event;
  - [x] 7.5: view.cpp - 移除 max_height，添加 (void)event; (void)color;
  - [x] 7.6: measuredock.cpp - 移除未使用的 bkColor
  - [x] 7.7: logobar.cpp - 添加 (void)enable;
  - [x] 7.8: searchdock.cpp - 为6个未使用参数添加 (void) 转换
  - [x] 7.9: titlebar.cpp - 移除未使用的 rect 变量
  - [x] 7.10: storesession.cpp - 移除 status, read_buf；处理 QFile::open 返回值
  - [x] 7.11: protocolexp.cpp - 处理 QFile::open 返回值
  - [x] 7.12: mainframe.cpp - 移除未使用的 newWidth, newHeight, newLeft, newTop
  - [x] 7.13: langresource.cpp - 处理 QFile::open 返回值
  - [x] 7.14: keywordlineedit.cpp - 移除未使用的 pt 变量
  - [x] 7.15: dscombobox.cpp - 移除未使用的 height 变量
  - [x] 7.16: winshadow.cpp - 移除 isActiveWindow，添加 (void)event;
  - [x] 7.17: winnativewidget.cpp - 添加 (void)wParam;
  - [x] 7.18: storeprogress.cpp - 移除未使用的 cursor_list

- [x] Task 8: 修复 PXView C++ 警告 - 符号比较和 switch
  - [x] 8.1: deviceoptions.cpp:763 - 修复 sign-compare（添加 (int) 强转）
  - [x] 8.2: deviceoptionsdock.cpp:717 - 修复 sign-compare（添加 (int) 强转）
  - [x] 8.3: storeprogress.cpp - 修复4处 sign-compare
  - [x] 8.4: view.cpp:1930 - 修复 sign-compare（(size_t)device_ch_count）
  - [x] 8.5: dsldial.cpp:103 - 修复 sign-compare（(qsizetype)displayIndex）
  - [x] 8.6: viewport.cpp - 为2个 switch 添加 default: break;

- [x] Task 9: 修复 PXView C++ 警告 - 其他
  - [x] 9.1: winnativewidget.h - 为 WinNativeWidget 添加 virtual 析构函数
  - [x] 9.2: annotationrestable.cpp - 修复2处 strncpy 后添加 null 终止
  - [x] 9.3: deviceoptionsdock.cpp - 处理2处 QtConcurrent::run 未使用返回值 ((void)前缀)
  - [x] 9.4: mainwindow.cpp - 处理2处 QTranslator::load 未使用返回值 ((void)前缀)
  - [x] 9.5: boost/bind.hpp 弃用 - 在5个文件中添加 #define BOOST_BIND_GLOBAL_PLACEHOLDERS

- [x] Task 10: 修复 libsigrokdecode C 警告
  - [x] 10.1: config.h 中 _POSIX_C_SOURCE 从 200112L 更新为 200809L
  - [x] 10.2: srd.c - 用 #if PY_VERSION_HEX < 0x03090000 包裹 PyEval_InitThreads()
  - [x] 10.3: decoder.c - 修复2处 cast-function-type（添加中间 void* 转换）

# Task Dependencies
- [Task 2] 依赖 [Task 1]（CMake 修复可能影响 MOC 行为）
- [Task 4] 依赖 [Task 3]（c_decoder_api.c 修复影响所有 decoder）
- [Task 5-9] 可并行执行（PXView C++ 修复互不依赖）
- [Task 10] 独立，可与 Task 5-9 并行执行
