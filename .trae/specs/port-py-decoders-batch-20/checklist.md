# Python → C 解码器移植检查清单 (Batch 20)

## 全局检查项

### 文件结构检查

- [ ] 每个解码器文件位于 `libsigrokdecode/c_decoders/<id>_c.c`
- [ ] 文件名格式正确：`mlx90614_c.c`, `mpu6050_c.c`, `mxc6225xu_c.c`, `nunchuk_c.c`, `pca9571_c.c`
- [ ] 包含正确的头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`

### C 解码器框架合规性检查

- [ ] `srd_c_decoder` 结构体中 `.id` 格式为 `"<python_id>_c"`
- [ ] `srd_c_decoder` 结构体中 `.name` 格式为 `"<Name>(C)"`
- [ ] `srd_c_decoder` 结构体中 `.desc` 末尾包含 `"(C implementation)"`
- [ ] `ann_labels[][3]` 第一列全部为 `""`
- [ ] `ann_labels[][3]` 第二列与 Python annotation id 一致
- [ ] 所有 annotation class 都映射到某个 annotation row
- [ ] `inputs` 为 `{"i2c", NULL}`
- [ ] `num_inputs` 为 `1`
- [ ] `channels` 为 `NULL`，`num_channels` 为 `0`
- [ ] `optional_channels` 为 `NULL`，`num_optional_channels` 为 `0`
- [ ] `outputs` 为 `NULL`，`num_outputs` 为 `0`
- [ ] `binary` 为 `NULL`，`num_binary` 为 `0`
- [ ] `.decode` 回调为空函数（I2C 上层解码器不使用 decode）
- [ ] `.recv_proto` 回调已实现
- [ ] `SRD_C_DECODER_EXPORT` 宏用于导出函数
- [ ] `srd_c_decoder_entry()` 返回解码器结构体指针
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`

### 私有数据管理检查

- [ ] `reset()` 中首次调用时使用 `g_malloc0()` 分配私有数据
- [ ] `reset()` 中使用 `c_decoder_set_private()` 设置私有数据
- [ ] `reset()` 中使用 `memset()` 清零私有数据
- [ ] `start()` 中使用 `c_decoder_get_private()` 获取私有数据
- [ ] `recv_proto()` 中使用 `c_decoder_get_private()` 获取私有数据
- [ ] `destroy()` 中使用 `g_free()` 释放私有数据
- [ ] `destroy()` 中调用 `c_decoder_set_private(di, NULL)` 置空

### 输出注册检查

- [ ] `start()` 中调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "<decoder_name>")`
- [ ] 返回值保存到私有数据的 `out_ann` 字段
- [ ] 使用 `C_ANN_PUT(di, ss, es, out_ann, class, text)` 宏输出 annotation

### 状态机检查

- [ ] 状态机枚举定义完整，覆盖所有 Python 中的状态
- [ ] `recv_proto()` 中对 `cmd` 参数使用 `strcmp()` 比较
- [ ] 处理 `"START"` / `"START REPEAT"` 条件
- [ ] 处理 `"STOP"` 条件（通常重置到 IDLE）
- [ ] 处理 `"ADDRESS READ"` / `"ADDRESS WRITE"` 条件
- [ ] 处理 `"DATA READ"` / `"DATA WRITE"` 条件
- [ ] 从 `data[0]` 提取数据字节（检查 `data_len > 0`）
- [ ] 忽略 `"BITS"` / `"ACK"` / `"NACK"` 包（视解码器而定）

### 内存安全检查

- [ ] `snprintf()` 缓冲区大小正确，无溢出风险
- [ ] 所有字符串缓冲区大小足够（建议至少 128 字节，复杂寄存器 256 字节）
- [ ] 无动态内存泄漏（除 `g_malloc0` / `g_free` 配对外无其他 malloc）
- [ ] 无空指针解引用（`c_decoder_get_private()` 返回值已检查）

---

## MLX90614 专项检查

- [ ] Annotation 枚举：`ANN_CELSIUS=0`, `ANN_KELVIN=1`, `NUM_ANN=2`
- [ ] 状态机：3 个状态（IGNORE_START_REPEAT, IGNORE_ADDRESS_WRITE, GET_TEMPERATURE）
- [ ] 初始状态为 `MLX90614_IGNORE_START_REPEAT`（等待 START REPEAT）
- [ ] 温度计算：`kelvin = (data[0] | (data[1] << 8)) * 0.02`
- [ ] 温度计算：`celsius = kelvin - 273.15`
- [ ] **Bug 修正**：收集 2 字节后立即计算（不等待第 3 字节）
- [ ] 第 1 字节记录 ss，第 2 字节记录 es
- [ ] 计算后重置状态和 data_count
- [ ] Annotation row "temps-celsius" 包含 ANN_CELSIUS
- [ ] Annotation row "temps-kelvin" 包含 ANN_KELVIN
- [ ] Tags: `{"IC", "Sensor", NULL}`，num_tags=2
- [ ] 无 options

---

## MPU6050 专项检查

- [ ] Annotation 枚举：29 个 class（0-28）+ NUM_ANN=29
- [ ] 状态机：4 个状态（IDLE, GET_SLAVE_ADDR, GET_REG_ADDR, WRITE_REGS）
- [ ] I2C 地址过滤：`addr == 0x68`，不匹配输出 ANN_WARNING
- [ ] 寄存器处理函数表：`handle_reg_0x1b`, `handle_reg_0x1c`, `handle_reg_0x38`, `handle_reg_0x6b`, `handle_reg_0x75`
- [ ] 数据寄存器处理：0x3B-0x48 范围，自动 reg++
- [ ] **Bug 修正**：16 位数据拼接使用 `(high << 8) | low`（非 `+= b/1000000`）
- [ ] 温度计算：`temp_c = raw / 340.0 + 36.53`
- [ ] GYRO_CONFIG (0x1B)：自检位 + FS_SEL 量程（±250/500/1000/2000 °/s）
- [ ] ACCEL_CONFIG (0x1C)：自检位 + AFS_SEL 量程（±2/4/8/16g）
- [ ] INT_ENABLE (0x38)：中断使能位
- [ ] PWR_MGMT_1 (0x6B)：电源管理位
- [ ] WHO_AM_I (0x75)：设备 ID
- [ ] 4 个 annotation rows：bits(9-23), regs(0-8), accel-gyro(24-27), warnings(28)
- [ ] Tags: `{"Gyroscope", NULL}`，num_tags=1
- [ ] 无 options
- [ ] 无 print() 调试输出

---

## MXC6225XU 专项检查

- [ ] Annotation 枚举：`ANN_TEXT=0`, `NUM_ANN=1`
- [ ] 状态机：6 个状态（IDLE, GET_SLAVE_ADDR, GET_REG_ADDR, WRITE_REGS, READ_REGS, READ_REGS2）
- [ ] 寄存器处理：0x00(XOUT), 0x01(YOUT), 0x02(STATUS), 0x03(DETECTION)
- [ ] XOUT/YOUT：8-bit 2's complement，使用 `(int8_t)b` 转换
- [ ] STATUS (0x02) 位域解析：INT[7], SH[6:5], TILT[4], ORI[3:2], OR[1:0]
- [ ] DETECTION (0x03) 位域解析：PD[7], SHM[6], SHTH[5:4], SHC[3:2], ORC[1:0]
- [ ] 查找表：`sh_str[4]`, `ori_str[4]`, `shth_str[4]`, `shc_str[4]`, `orc_str[4]`
- [ ] WRITE_REGS → "START REPEAT" → READ_REGS → "ADDRESS READ" → READ_REGS2 流程正确
- [ ] 1 个 annotation row：text(0)
- [ ] Tags: `{"IC", "Sensor", NULL}`，num_tags=2
- [ ] 无 options
- [ ] **建议**：添加 I2C 地址过滤 option（默认 0x2A）

---

## Nunchuk 专项检查

- [ ] Annotation 枚举：15 个 class（0-14）+ NUM_ANN=15
- [ ] 状态机：4 个状态（IDLE, GET_SLAVE_ADDR, READ_REGS, WRITE_REGS）
- [ ] 6 字节读取序列：reg 从 0x00 自动递增
- [ ] 寄存器 0x00-0x04：直接值输出
- [ ] 寄存器 0x05：按键 + 加速度低位解析
  - [ ] BZ = bit[0]（0=按下, 1=未按下）
  - [ ] BC = bit[1]（0=按下, 1=未按下）
  - [ ] AX[1:0] = bits[3:2]，合并到 ax
  - [ ] AY[1:0] = bits[5:4]，合并到 ay
  - [ ] AZ[1:0] = bits[7:6]，合并到 az
- [ ] Summary 输出：STOP 时所有值有效才输出
- [ ] Init 序列检测：收集 2 字节写数据
  - [ ] 长度 != 2 → warning
  - [ ] 内容 != [0x40, 0x00] → warning
  - [ ] 内容 == [0x40, 0x00] → "Initialize Nunchuk"
- [ ] 传感器值初始为 -1，STOP 后重置为 -1
- [ ] 3 个 annotation rows：regs(0-12), summary(13), warnings(14)
- [ ] Tags: `{"Sensor", NULL}`，num_tags=1
- [ ] 无 options
- [ ] 位级 annotation 简化：`putd(bit1, bit2, ...)` 改为 `C_ANN_PUT` 使用当前 ss/es

---

## PCA9571 专项检查

- [ ] Annotation 枚举：`ANN_REGISTER=0`, `ANN_VALUE=1`, `ANN_WARNING=2`, `NUM_ANN=3`
- [ ] 状态机：4 个状态（IDLE, GET_SLAVE_ADDR, READ_DATA, WRITE_DATA）
- [ ] I2C 地址过滤：`addr == 0x25`，不匹配输出 ANN_WARNING
- [ ] "START" / "START REPEAT" 都进入 GET_SLAVE_ADDR
- [ ] "NACK" / "STOP" 都重置到 IDLE
- [ ] "ACK" / "BITS" 被忽略
- [ ] 读操作：输出 "Outputs read: %02X"，检查与 last_write 一致性
- [ ] 写操作：输出 "Outputs set: %02X"，更新 last_write
- [ ] last_write 初始值：0xFF（芯片端口默认高电平）
- [ ] **OUTPUT_LOGIC 暂不实现**（C 框架可能不支持）
- [ ] 2 个 annotation rows：regs(0,1), warnings(2)
- [ ] Tags: `{"Embedded/industrial", "IC", NULL}`，num_tags=2
- [ ] 无 options

---

## CMakeLists.txt 更新检查

- [ ] 在 `C_DECODERS` 列表中添加 `mlx90614_c`
- [ ] 在 `C_DECODERS` 列表中添加 `mpu6050_c`
- [ ] 在 `C_DECODERS` 列表中添加 `mxc6225xu_c`
- [ ] 在 `C_DECODERS` 列表中添加 `nunchuk_c`
- [ ] 在 `C_DECODERS` 列表中添加 `pca9571_c`
- [ ] 新增项按字母顺序插入

---

## 编译验证检查

- [ ] `build_incremental.cmd` 执行成功
- [ ] 5 个 DLL 文件生成到 `build.dir/decoders/c_decoders/`
  - [ ] `mlx90614_c.dll`（或 `.so`）
  - [ ] `mpu6050_c.dll`
  - [ ] `mxc6225xu_c.dll`
  - [ ] `nunchuk_c.dll`
  - [ ] `pca9571_c.dll`
- [ ] 无编译警告（Wall/Wextra）
- [ ] 无链接错误

---

## 功能验证检查

### 通用验证

- [ ] PXView 启动无崩溃
- [ ] 解码器列表中显示 5 个新 C 解码器（名称带 "(C)" 后缀）
- [ ] 解码器可被选择和添加到信号上
- [ ] 与 i2c 解码器正确堆叠

### MLX90614 验证

- [ ] I2C 总线上有 MLX90614 温度读取时，显示摄氏度和开尔文温度
- [ ] 温度值在合理范围内（-70°C ~ 380°C）

### MPU6050 验证

- [ ] I2C 地址 0x68 的通信被正确解码
- [ ] 非 0x68 地址的通信被忽略并显示 warning
- [ ] 加速度/温度/陀螺仪数据寄存器正确解析
- [ ] 配置寄存器（GYRO_CONFIG, ACCEL_CONFIG 等）正确解析

### MXC6225XU 验证

- [ ] XOUT/YOUT 显示有符号加速度值
- [ ] STATUS 寄存器位域正确解析
- [ ] DETECTION 寄存器位域正确解析
- [ ] 写→重复起始→读流程正确处理

### Nunchuk 验证

- [ ] 6 字节读取序列正确解析
- [ ] 按键状态正确显示（Z/C 按钮）
- [ ] 加速度 10-bit 值正确拼接
- [ ] Summary 在完整读取后输出
- [ ] 初始化序列 [0x40, 0x00] 正确检测

### PCA9571 验证

- [ ] I2C 地址 0x25 的通信被正确解码
- [ ] 写操作显示 "Outputs set: XX"
- [ ] 读操作显示 "Outputs read: XX"
- [ ] 读值与写值不一致时显示 warning
