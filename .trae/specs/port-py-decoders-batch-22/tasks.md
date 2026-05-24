# 移植任务分解 — Batch 22

> 解码器：xfp, hdcp, hdmi_scdc, tca6408a, tmp102
> 建议实现顺序：tca6408a → hdcp → hdmi_scdc → tmp102 → xfp

---

## Task 1: tca6408a_c — TI TCA6408A 8-bit I²C I/O Expander

**优先级**: P0（最简单，作为模板验证流程）
**预估代码量**: ~200 行
**源文件**: `libsigrokdecode/decoders/tca6408a/pd.py`

### 1.1 创建文件 `libsigrokdecode/c_decoders/tca6408a_c.c`

### 1.2 实现元数据

- [ ] 定义 `ANN_REGISTER=0, ANN_VALUE=1, ANN_WARNING=2, NUM_ANN=3` 枚举
- [ ] 定义 `tca6408a_ann_labels[3][3]`（第一列 `""`）
- [ ] 定义 `tca6408a_ann_rows[2]`：regs 行(0,1)，warnings 行(2)
- [ ] 定义 `tca6408a_inputs[] = {"i2c", NULL}`
- [ ] 定义 `tca6408a_tags[] = {"Embedded/industrial", "IC", NULL}`

### 1.3 实现状态机

- [ ] 定义 `tca6408a_state` 枚举：`IDLE, GET_SLAVE_ADDR, GET_REG_ADDR, WRITE_IO_REGS, READ_IO_REGS, READ_IO_REGS2`
- [ ] 定义 `tca6408a_state` 私有结构体（state, reg, chip, ss, es, logic_output_es, logic_value, out_ann）

### 1.4 实现 recv_proto()

- [ ] `IDLE` → 收到 `START` → `GET_SLAVE_ADDR`
- [ ] `GET_SLAVE_ADDR` → 收到任意 → 保存 chip → `GET_REG_ADDR`
- [ ] `GET_REG_ADDR` → 收到 `ADDRESS READ/WRITE` → 调用 `check_correct_chip()`；收到 `DATA WRITE` → 保存 reg → 输出寄存器名 → `WRITE_IO_REGS`
- [ ] `WRITE_IO_REGS` → 收到 `START REPEAT` → `READ_IO_REGS`；收到 `DATA WRITE` → 调用 `handle_reg()`；收到 `STOP` → `IDLE`
- [ ] `READ_IO_REGS` → 收到 `ADDRESS READ` → `READ_IO_REGS2`
- [ ] `READ_IO_REGS2` → 收到 `DATA READ` → 调用 `handle_reg()`；收到 `STOP` → `IDLE`

### 1.5 实现辅助函数

- [ ] `tca6408a_check_correct_chip()`：地址不在 (0x20, 0x21) 时输出警告
- [ ] `tca6408a_handle_write_reg()`：输出寄存器名称（Input port/Output port/Polarity inversion/Configuration）
- [ ] `tca6408a_handle_reg()`：根据 reg 值输出寄存器值

### 1.6 实现 start/reset/destroy

- [ ] `start()`：`c_decoder_register_output(di, SRD_OUTPUT_ANN, "tca6408a")`
- [ ] `reset()`：初始化私有结构体，state=IDLE
- [ ] `destroy()`：释放私有数据

### 1.7 定义导出结构体

- [ ] `struct srd_c_decoder tca6408a_c_decoder`（`.id = "tca6408a_c"`, `.name = "TCA6408A(C)"`）
- [ ] `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()`

### 1.8 修改 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `tca6408a_c`

### 1.9 编译验证

- [ ] 运行 `build_incremental.cmd` 确认编译通过
- [ ] 检查 `build.dir/decoders/c_decoders/` 下生成 `tca6408a_c.dll`

---

## Task 2: hdcp_c — HDCP over HDMI

**优先级**: P1
**预估代码量**: ~350 行
**源文件**: `libsigrokdecode/decoders/hdcp/pd.py`

### 2.1 创建文件 `libsigrokdecode/c_decoders/hdcp_c.c`

### 2.2 实现元数据

- [ ] 定义 20 个注解枚举（ANN_MSG_0x00 到 ANN_MSG_0x11 + ANN_SUMMARY + ANN_WARNING）
- [ ] 定义 `hdcp_ann_labels[20][3]`
- [ ] 定义 `hdcp_ann_rows[3]`：messages 行(0-17)，summaries 行(18)，warnings 行(19)
- [ ] 定义 `hdcp_inputs[] = {"i2c", NULL}`
- [ ] 定义 `hdcp_tags[] = {"PC", "Security/crypto", NULL}`

### 2.3 实现查找表

- [ ] `hdcp_msg_ids[]`：HDCP 2.2 消息 ID → 名称映射（12 条）
- [ ] `hdcp_write_items[]`：写偏移 → 寄存器名称映射（17 条）

### 2.4 实现状态机

- [ ] 定义 `hdcp_state` 枚举：`IDLE, GET_SLAVE_ADDR, WRITE_OFFSET, BUFFER_DATA`
- [ ] 定义 `hdcp_state` 私有结构体（state, stack[256], stack_len, type[64], ss, es, ss_block, es_block, out_ann）

### 2.5 实现 recv_proto()

- [ ] `IDLE` → 收到 `START` → 重置状态 → `GET_SLAVE_ADDR`
- [ ] `GET_SLAVE_ADDR` → 收到 `ADDRESS READ`（0x3A）→ `BUFFER_DATA`；收到 `ADDRESS WRITE`（0x3A）→ `WRITE_OFFSET`；非 0x3A → `IDLE`
- [ ] `WRITE_OFFSET` → 收到 `DATA WRITE` → 查找 write_items → 判断是否缓冲 → `BUFFER_DATA` 或 `IDLE`
- [ ] `BUFFER_DATA` → 收到 `STOP/NACK` → 处理缓冲区 → `IDLE`；收到 `DATA READ/WRITE` → 压入 stack

### 2.6 实现缓冲区处理

- [ ] `hdcp_process_buffer()`：
  - type 为空 → 仅输出 type
  - `RxStatus` → 解析 2 字节（reauth/ready/length）
  - `1.4 Bstatus` → 解析 2 字节（device_count/depth/hdmi_mode）
  - `Read_Message`/`Write_Message` → 首字节为消息 ID，查找 msg_ids
  - `HDCP2Version` → 检查 bit2
  - 其他 → 直接输出 type

### 2.7 实现 start/reset/destroy

- [ ] `start()`：`c_decoder_register_output(di, SRD_OUTPUT_ANN, "hdcp")`
- [ ] `reset()`：初始化私有结构体
- [ ] `destroy()`：释放私有数据

### 2.8 定义导出结构体

- [ ] `struct srd_c_decoder hdcp_c_decoder`（`.id = "hdcp_c"`, `.name = "HDCP(C)"`）

### 2.9 修改 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `hdcp_c`

### 2.10 编译验证

- [ ] 运行 `build_incremental.cmd` 确认编译通过

---

## Task 3: hdmi_scdc_c — HDMI SCDC

**优先级**: P2
**预估代码量**: ~500 行
**源文件**: `libsigrokdecode/decoders/hdmi_scdc/pd.py`

### 3.1 创建文件 `libsigrokdecode/c_decoders/hdmi_scdc_c.c`

### 3.2 实现元数据

- [ ] 定义 4 个注解枚举（ANN_ADDRESS, ANN_REGISTER, ANN_FIELDS, ANN_DEBUG）
- [ ] 定义 `hdmi_scdc_ann_labels[4][3]`
- [ ] 定义 `hdmi_scdc_ann_rows[2]`：scdc 行(0,1,2)，debug 行(3)
- [ ] 定义选项 `verbosity`（short/long/debug）

### 3.3 实现 SCDC 寄存器查找表

- [ ] `scdc_reg_defs[]`：16 个寄存器定义（offset, name, type）
- [ ] 字段解释表（每个寄存器的 mask + value → 文本映射）

### 3.4 实现状态机

- [ ] 定义 8 个状态枚举
- [ ] 定义私有结构体（state, reg, offset, protocol, databytes[], err_det_lower, block_s, block_e, verbosity）

### 3.5 实现 recv_proto()

- [ ] `IDLE` → `START` → `GET_SLAVE_ADDR`
- [ ] `GET_SLAVE_ADDR` → `ADDRESS WRITE`(0xA8) → `GET_OFFSET`；`ADDRESS READ`(0xA9) → `READ_REGISTER`
- [ ] `GET_OFFSET` → `DATA WRITE` → 保存 offset → 输出寄存器名 → `OFFSET_RECEIVED`
- [ ] `OFFSET_RECEIVED` → `START REPEAT` → `GET_SLAVE_ADDR`；`DATA WRITE` → 缓冲 → `WRITE_REGISTER` → handle_scdc
- [ ] `READ_REGISTER`/`WRITE_REGISTER` → `DATA READ/WRITE` → 缓冲 → handle_scdc；`STOP/START REPEAT` → `IDLE`

### 3.6 实现 handle_scdc()

- [ ] 根据 offset 查找寄存器定义
- [ ] 遍历字段列表，根据 mask 提取值，查找 interpretation
- [ ] CED 寄存器特殊处理（0x50-0x55：2 字节组合，通道计算，自动递增 offset）
- [ ] 根据 verbosity 选项调整输出格式

### 3.7 实现 start/reset/destroy

- [ ] `start()`：注册输出，读取 verbosity 选项
- [ ] `reset()`：初始化私有结构体
- [ ] `destroy()`：释放私有数据

### 3.8 定义导出结构体

- [ ] `struct srd_c_decoder hdmi_scdc_c_decoder`（`.id = "hdmi_scdc_c"`, `.name = "HDMI_SCDC(C)"`）

### 3.9 修改 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `hdmi_scdc_c`

### 3.10 编译验证

- [ ] 运行 `build_incremental.cmd` 确认编译通过

---

## Task 4: tmp102_c — Digital Temperature Sensor TMP102

**优先级**: P3
**预估代码量**: ~600 行
**源文件**: `libsigrokdecode/decoders/tmp102/pd.py`

### 4.1 创建文件 `libsigrokdecode/c_decoders/tmp102_c.c`

### 4.2 实现元数据

- [ ] 定义 35 个注解枚举（5 地址 + 5 寄存器 + 11 位 + 14 信息）
- [ ] 定义 `tmp102_ann_labels[35][3]`
- [ ] 定义 `tmp102_ann_rows[4]`：bits 行, regs 行, info 行, warnings 行
- [ ] 定义选项 `radix`（Hex/Dec/Oct/Bin）和 `units`（Celsius/Fahrenheit/Kelvin）

### 4.3 实现地址和寄存器查找表

- [ ] `tmp102_addresses[]`：4 个从地址映射（GND=0x48, VCC=0x49, SDA=0x4A, SCL=0x4B）
- [ ] `tmp102_rates[]`：转换率映射
- [ ] `tmp102_faults[]`：故障队列映射

### 4.4 实现状态机

- [ ] 定义 4 个状态枚举
- [ ] 定义私有结构体（state, addr, reg, em, write, bytes[], ssd, ssb, ss, es, radix, units）

### 4.5 实现 recv_proto()

- [ ] `IDLE` → `START` → `ADDRESS_SLAVE`
- [ ] `ADDRESS_SLAVE` → `ADDRESS WRITE/READ` → 检查地址 → `REGISTER_ADDRESS` 或 `REGISTER_DATA`
- [ ] `REGISTER_ADDRESS` → `DATA WRITE/READ` → 保存 reg → `REGISTER_DATA`；`STOP/START REPEAT` → `IDLE`
- [ ] `REGISTER_DATA` → `DATA WRITE/READ` → 缓冲字节；`START REPEAT` → `ADDRESS_SLAVE`；`STOP` → handle_data → `IDLE`

### 4.6 实现数据处理函数

- [ ] `tmp102_calculate_temperature()`：12/13-bit 二进制补码 + 单位转换
- [ ] `tmp102_handle_datareg_0x00()`：温度寄存器
- [ ] `tmp102_handle_datareg_0x01()`：配置寄存器（OS/R/F/POL/TM/SD/CR/AL/EM 位解析）
- [ ] `tmp102_handle_datareg_0x02()`：TLOW 寄存器
- [ ] `tmp102_handle_datareg_0x03()`：THIGH 寄存器
- [ ] `tmp102_handle_datareg_0x06()`：General Call Reset

### 4.7 实现辅助函数

- [ ] `tmp102_check_addr()`：地址合法性检查
- [ ] `tmp102_format_data()`：根据 radix 选项格式化数据

### 4.8 实现 start/reset/destroy

- [ ] `start()`：注册输出，读取 radix 和 units 选项
- [ ] `reset()`：初始化私有结构体
- [ ] `destroy()`：释放私有数据

### 4.9 定义导出结构体

- [ ] `struct srd_c_decoder tmp102_c_decoder`（`.id = "tmp102_c"`, `.name = "TMP102(C)"`）

### 4.10 修改 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `tmp102_c`

### 4.11 编译验证

- [ ] 运行 `build_incremental.cmd` 确认编译通过

---

## Task 5: xfp_c — XFP 10 Gigabit Small Form Factor Pluggable Module

**优先级**: P4（最复杂，建议最后实现）
**预估代码量**: ~800+ 行
**源文件**: `libsigrokdecode/decoders/xfp/pd.py`

### 5.1 创建文件 `libsigrokdecode/c_decoders/xfp_c.c`

### 5.2 实现元数据

- [ ] 定义 2 个注解枚举（ANN_FIELD_NAME_VAL, ANN_FIELD_VAL）
- [ ] 定义 `xfp_ann_labels[2][3]`
- [ ] 定义 `xfp_ann_rows[2]`：field-names-and-vals 行(0)，field-vals 行(1)

### 5.3 实现 plugtrx 查找表（从 Python common/plugtrx.py 移植）

- [ ] `MODULE_ID[]`：模块标识符映射
- [ ] `ALARM_THRESHOLDS[]`：告警阈值名称映射
- [ ] `AD_READOUTS[]`：A/D 读数名称映射
- [ ] `GCS_BITS[]`：通用控制/状态位映射
- [ ] `CONNECTOR[]`：连接器类型映射
- [ ] `TRANSCEIVER[][]`：收发器合规性映射（8×8 二维数组）
- [ ] `SERIAL_ENCODING[]`：串行编码支持映射
- [ ] `XMIT_TECH[]`：发射技术映射
- [ ] `CDR[]`：CDR 支持映射
- [ ] `DEVICE_TECH[][]`：设备技术映射
- [ ] `ENHANCED_OPTS[]`：增强选项映射
- [ ] `AUX_TYPES[]`：辅助监控类型映射

### 5.4 实现内存映射处理函数

- [ ] Lower Memory 处理函数（MAP_LOWER_MEMORY 对应）：
  - [ ] `xfp_module_id()`
  - [ ] `xfp_signal_cc()`
  - [ ] `xfp_alarm_warnings()`
  - [ ] `xfp_vps()`
  - [ ] `xfp_ber()`
  - [ ] `xfp_wavelength_cr()`
  - [ ] `xfp_fec_cr()`
  - [ ] `xfp_int_ctrl()`
  - [ ] `xfp_ad_readout()`
  - [ ] `xfp_gcs()`
  - [ ] `xfp_page_select()`
  - [ ] `xfp_ignore()`
- [ ] High Table 1 处理函数（MAP_HIGH_TABLE_1 对应）：
  - [ ] `xfp_ext_module_id()`
  - [ ] `xfp_connector()`
  - [ ] `xfp_transceiver()`
  - [ ] `xfp_serial_encoding()`
  - [ ] `xfp_br_min()` / `xfp_br_max()`
  - [ ] `xfp_link_length_*()`（5 个函数）
  - [ ] `xfp_device_tech()`
  - [ ] `xfp_vendor()` / `xfp_vendor_pn()` / `xfp_vendor_rev()` / `xfp_vendor_sn()`
  - [ ] `xfp_cdr()`
  - [ ] `xfp_vendor_oui()`
  - [ ] `xfp_wavelength()` / `xfp_wavelength_tolerance()`
  - [ ] `xfp_max_case_temp()`
  - [ ] `xfp_power_supply()`
  - [ ] `xfp_manuf_date()`
  - [ ] `xfp_diag_mon()`
  - [ ] `xfp_enhanced_opts()`
  - [ ] `xfp_aux_mon()`
  - [ ] `xfp_maybe_ascii()`

### 5.5 实现换算函数

- [ ] `xfp_to_temp()`：16-bit 二进制补码 → 摄氏度（1/256°C 精度）
- [ ] `xfp_to_current()`：16-bit → mA（0.2μA 精度）
- [ ] `xfp_to_power()`：16-bit → mW（0.1μW 精度）
- [ ] `xfp_to_wavelength()`：16-bit → nm（0.05nm 精度）
- [ ] `xfp_to_wavelength_tolerance()`：16-bit → nm（0.005nm 精度）

### 5.6 实现状态机

- [ ] 定义 3 个状态枚举（IDLE, GET_SLAVE_ADDR, READ_REGS）
- [ ] 定义私有结构体（state, cnt, buf[256], buf_len, sn[256][2], cur_highmem_page, have_clei, ss, es, out_ann）

### 5.7 实现 recv_proto()

- [ ] `IDLE` → `START` → `GET_SLAVE_ADDR`
- [ ] `GET_SLAVE_ADDR` → `ADDRESS READ/WRITE` → `READ_REGS`
- [ ] `READ_REGS` → `DATA READ` → 递增 cnt，缓冲字节，检查映射边界 → 调用处理函数；`STOP` → `IDLE`

### 5.8 实现内存映射分发

- [ ] Lower Memory 映射表：使用偏移量作为键，函数指针作为值
- [ ] High Table 1 映射表：同上
- [ ] 在 cnt 到达映射键时，调用对应处理函数并清空缓冲区

### 5.9 实现 start/reset/destroy

- [ ] `start()`：`c_decoder_register_output(di, SRD_OUTPUT_ANN, "xfp")`
- [ ] `reset()`：初始化私有结构体，cnt=-1，cur_highmem_page=0
- [ ] `destroy()`：释放私有数据

### 5.10 定义导出结构体

- [ ] `struct srd_c_decoder xfp_c_decoder`（`.id = "xfp_c"`, `.name = "XFP(C)"`）

### 5.11 修改 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `xfp_c`

### 5.12 编译验证

- [ ] 运行 `build_incremental.cmd` 确认编译通过

---

## Task 6: 集成测试与最终验证

### 6.1 编译验证

- [ ] 全部 5 个解码器编译通过
- [ ] 检查 DLL 生成：`build.dir/decoders/c_decoders/xfp_c.dll`, `hdcp_c.dll`, `hdmi_scdc_c.dll`, `tca6408a_c.dll`, `tmp102_c.dll`

### 6.2 运行时验证

- [ ] 启动 PXView，确认 C 解码器出现在解码器列表中
- [ ] 加载包含 I2C 数据的会话文件
- [ ] 依次选择 5 个 C 解码器，确认无崩溃
- [ ] 对比 Python 版本和 C 版本的注解输出

### 6.3 CMakeLists.txt 最终确认

- [ ] 确认 `C_DECODERS` 列表包含所有 5 个新解码器
- [ ] 确认无编译警告
