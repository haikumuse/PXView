# Python → C 解码器移植任务清单 (Batch 20)

## 任务总览

| 任务 ID | 解码器 | 源文件 | 目标文件 | 优先级 | 预估行数 |
|---------|--------|--------|----------|--------|----------|
| T1 | mlx90614 | `decoders/mlx90614/pd.py` | `c_decoders/mlx90614_c.c` | P1 | ~200 |
| T2 | mpu6050 | `decoders/mpu6050/pd.py` | `c_decoders/mpu6050_c.c` | P3 | ~600 |
| T3 | mxc6225xu | `decoders/mxc6225xu/pd.py` | `c_decoders/mxc6225xu_c.c` | P2 | ~350 |
| T4 | nunchuk | `decoders/nunchuk/pd.py` | `c_decoders/nunchuk_c.c` | P2 | ~400 |
| T5 | pca9571 | `decoders/pca9571/pd.py` | `c_decoders/pca9571_c.c` | P1 | ~200 |
| T6 | CMake | — | `CMakeLists.txt` | P1 | +5 行 |

**优先级说明：** P1=简单/基础，先完成可验证框架；P2=中等复杂度；P3=最复杂，放最后。

---

## T1: mlx90614_c — 红外温度计

### 子任务

| # | 子任务 | 详情 | 状态 |
|---|--------|------|------|
| T1.1 | 创建文件 | `libsigrokdecode/c_decoders/mlx90614_c.c` | ⬜ |
| T1.2 | 编写头文件包含 | `stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h` | ⬜ |
| T1.3 | 定义 annotation 枚举 | `ANN_CELSIUS=0`, `ANN_KELVIN=1`, `NUM_ANN=2` | ⬜ |
| T1.4 | 定义状态机枚举 | `MLX90614_IGNORE_START_REPEAT`, `MLX90614_IGNORE_ADDRESS_WRITE`, `MLX90614_GET_TEMPERATURE` | ⬜ |
| T1.5 | 定义私有结构体 | `mlx90614_priv`：state, data[2], data_count, ss, es, out_ann | ⬜ |
| T1.6 | 编写静态元数据 | inputs, tags, ann_labels, ann_rows | ⬜ |
| T1.7 | 实现 `mlx90614_reset()` | g_malloc0 分配，memset 清零，设置初始状态 | ⬜ |
| T1.8 | 实现 `mlx90614_start()` | `c_decoder_register_output(di, SRD_OUTPUT_ANN, "mlx90614")` | ⬜ |
| T1.9 | 实现 `mlx90614_recv_proto()` | 3 状态状态机，收集 2 字节后计算温度 | ⬜ |
| T1.10 | 实现 `mlx90614_decode()` | 空函数 | ⬜ |
| T1.11 | 实现 `mlx90614_destroy()` | g_free 释放私有数据 | ⬜ |
| T1.12 | 定义 `srd_c_decoder` 结构体 | `.id="mlx90614_c"`, `.name="MLX90614(C)"`, `.recv_proto=mlx90614_recv_proto` | ⬜ |
| T1.13 | 实现导出入口 | `srd_c_decoder_entry()`, `srd_c_decoder_api_version()` | ⬜ |

### 关键实现细节

- **温度计算修正：** Python 原始代码有 bug（等第 3 字节才计算），C 实现应在收集 2 字节后立即计算
- **温度公式：** `kelvin = (data[0] | (data[1] << 8)) * 0.02`，`celsius = kelvin - 273.15`
- **状态机起始状态：** `MLX90614_IGNORE_START_REPEAT`（等待 START REPEAT 条件）
- **不检查 I2C 地址：** 与 Python 原始行为一致

### 关键代码片段

```c
static void mlx90614_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    mlx90614_priv *s = (mlx90614_priv *)c_decoder_get_private(di);
    if (!s) return;
    s->ss = start_sample;
    s->es = end_sample;

    if (s->state == MLX90614_IGNORE_START_REPEAT) {
        if (strcmp(cmd, "START REPEAT") == 0)
            s->state = MLX90614_IGNORE_ADDRESS_WRITE;
    } else if (s->state == MLX90614_IGNORE_ADDRESS_WRITE) {
        if (strcmp(cmd, "ADDRESS WRITE") == 0)
            s->state = MLX90614_GET_TEMPERATURE;
    } else if (s->state == MLX90614_GET_TEMPERATURE) {
        if (strcmp(cmd, "DATA WRITE") == 0) {
            uint8_t databyte = (data_len > 0) ? data[0] : 0;
            if (s->data_count == 0) {
                s->data[0] = databyte;
                s->ss = start_sample;
                s->data_count = 1;
            } else if (s->data_count == 1) {
                s->data[1] = databyte;
                s->es = end_sample;
                double kelvin = (double)(s->data[0] | (s->data[1] << 8)) * 0.02;
                double celsius = kelvin - 273.15;
                char buf[64];
                snprintf(buf, sizeof(buf), "Temperature: %.2f °C", celsius);
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_CELSIUS, buf);
                snprintf(buf, sizeof(buf), "Temperature: %.2f K", kelvin);
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_KELVIN, buf);
                s->state = MLX90614_IGNORE_START_REPEAT;
                s->data_count = 0;
            }
        }
    }
}
```

---

## T2: mpu6050_c — 加速度计/陀螺仪

### 子任务

| # | 子任务 | 详情 | 状态 |
|---|--------|------|------|
| T2.1 | 创建文件 | `libsigrokdecode/c_decoders/mpu6050_c.c` | ⬜ |
| T2.2 | 编写头文件包含 | 同 T1.2 | ⬜ |
| T2.3 | 定义 annotation 枚举 | 29 个 annotation class (ANN_REG_SECONDS..ANN_WARNING) | ⬜ |
| T2.4 | 定义状态机枚举 | `MPU6050_IDLE`, `MPU6050_GET_SLAVE_ADDR`, `MPU6050_GET_REG_ADDR`, `MPU6050_WRITE_REGS` | ⬜ |
| T2.5 | 定义私有结构体 | `mpu6050_priv`：state, reg, acc_x/y/z, temp, gyro_x/y/z, ss, es, ss_block, out_ann | ⬜ |
| T2.6 | 编写静态元数据 | 29 个 ann_labels，4 个 ann_rows，inputs, tags | ⬜ |
| T2.7 | 实现 `mpu6050_reset()` | 分配+初始化，acc/gyro/temp 初始化为 -1 | ⬜ |
| T2.8 | 实现 `mpu6050_start()` | `c_decoder_register_output(di, SRD_OUTPUT_ANN, "mpu6050")` | ⬜ |
| T2.9 | 实现 `handle_reg_0x1b()` | GYRO_CONFIG：自检 + FS_SEL 量程解析 | ⬜ |
| T2.10 | 实现 `handle_reg_0x1c()` | ACCEL_CONFIG：自检 + AFS_SEL 量程解析 | ⬜ |
| T2.11 | 实现 `handle_reg_0x38()` | INT_ENABLE：中断使能位解析 | ⬜ |
| T2.12 | 实现 `handle_reg_0x6b()` | PWR_MGMT_1：电源管理位解析 | ⬜ |
| T2.13 | 实现 `handle_reg_0x75()` | WHO_AM_I：设备 ID | ⬜ |
| T2.14 | 实现 `handle_reg_data()` | 0x3B-0x48 数据寄存器处理（加速度/温度/陀螺仪） | ⬜ |
| T2.15 | 实现 `mpu6050_recv_proto()` | 4 状态状态机，地址过滤 0x68 | ⬜ |
| T2.16 | 实现 `mpu6050_decode()` | 空函数 | ⬜ |
| T2.17 | 实现 `mpu6050_destroy()` | g_free 释放 | ⬜ |
| T2.18 | 定义 `srd_c_decoder` 结构体 | `.id="mpu6050_c"`, `.name="MPU6050(C)"` | ⬜ |
| T2.19 | 实现导出入口 | `srd_c_decoder_entry()`, `srd_c_decoder_api_version()` | ⬜ |

### 关键实现细节

- **最复杂的解码器**：29 个 annotation class，多个寄存器处理函数
- **I2C 地址过滤**：必须检查 `addr == 0x68`，不匹配输出 warning
- **16 位数据拼接修正：** Python 的 `acc_x += b / 1000000` 是错误的，C 实现应使用 `(high << 8) | low`
- **温度计算：** `temp_c = raw / 340.0 + 36.53`
- **寄存器自增：** 数据寄存器 0x3B-0x48 在 handle_reg_data 中自动 reg++
- **位级 annotation 简化：** Python 使用 `putd(bit1, bit2, ...)` 基于位数据，C 实现改用 `C_ANN_PUT` 基于当前 ss/es

### 关键代码片段 — recv_proto

```c
static void mpu6050_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    mpu6050_priv *s = (mpu6050_priv *)c_decoder_get_private(di);
    if (!s) return;
    s->ss = start_sample;
    s->es = end_sample;

    if (s->state == MPU6050_IDLE) {
        if (strcmp(cmd, "START") == 0)
            s->state = MPU6050_GET_SLAVE_ADDR;
    } else if (s->state == MPU6050_GET_SLAVE_ADDR) {
        if (strcmp(cmd, "ADDRESS WRITE") == 0 || strcmp(cmd, "ADDRESS READ") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr == 0x68) {
                s->state = MPU6050_GET_REG_ADDR;
            } else {
                char buf[128];
                snprintf(buf, sizeof(buf),
                    "Ignoring non-MPU6050 data (slave 0x%02X)", addr);
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARNING, buf);
                s->state = MPU6050_IDLE;
            }
        }
    } else if (s->state == MPU6050_GET_REG_ADDR) {
        if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = MPU6050_GET_SLAVE_ADDR;
        } else if (strcmp(cmd, "STOP") == 0) {
            s->state = MPU6050_IDLE;
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            s->reg = (data_len > 0) ? data[0] : 0;
            s->state = MPU6050_WRITE_REGS;
        } else if (strcmp(cmd, "DATA READ") == 0) {
            uint8_t databyte = (data_len > 0) ? data[0] : 0;
            mpu6050_handle_reg(di, s, databyte);
        }
    } else if (s->state == MPU6050_WRITE_REGS) {
        if (strcmp(cmd, "STOP") == 0) {
            s->state = MPU6050_IDLE;
        } else if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = MPU6050_GET_SLAVE_ADDR;
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            uint8_t databyte = (data_len > 0) ? data[0] : 0;
            mpu6050_handle_reg(di, s, databyte);
        }
    }
}
```

### 关键代码片段 — handle_reg_data

```c
static void mpu6050_handle_reg_data(struct srd_decoder_inst *di, mpu6050_priv *s, uint8_t b)
{
    char buf[64];
    switch (s->reg) {
    case 0x3b:
        s->acc_x = (int8_t)b;
        s->ss_block = s->ss;
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_REG_SECONDS, "ACCEL_XOUT[15:8]");
        break;
    case 0x3c:
        s->acc_x = (s->acc_x << 8) | b;
        snprintf(buf, sizeof(buf), "ACCEL_XOUT: %d", s->acc_x);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_WRITE_DATETIME, buf);
        break;
    case 0x3d:
        s->acc_y = (int8_t)b;
        s->ss_block = s->ss;
        break;
    case 0x3e:
        s->acc_y = (s->acc_y << 8) | b;
        snprintf(buf, sizeof(buf), "ACCEL_YOUT: %d", s->acc_y);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_WRITE_DATETIME, buf);
        break;
    case 0x3f:
        s->acc_z = (int8_t)b;
        s->ss_block = s->ss;
        break;
    case 0x40:
        s->acc_z = (s->acc_z << 8) | b;
        snprintf(buf, sizeof(buf), "ACCEL_ZOUT: %d", s->acc_z);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_WRITE_DATETIME, buf);
        break;
    case 0x41:
        s->temp = (int16_t)(b << 8);
        s->ss_block = s->ss;
        break;
    case 0x42: {
        s->temp |= b;
        double temp_c = (double)s->temp / 340.0 + 36.53;
        snprintf(buf, sizeof(buf), "TEMP: %.1f°C", temp_c);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_WRITE_DATETIME, buf);
        break;
    }
    case 0x43:
        s->gyro_x = (int8_t)b;
        s->ss_block = s->ss;
        break;
    case 0x44:
        s->gyro_x = (s->gyro_x << 8) | b;
        snprintf(buf, sizeof(buf), "GYRO_XOUT: %d", s->gyro_x);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_WRITE_DATETIME, buf);
        break;
    case 0x45:
        s->gyro_y = (int8_t)b;
        s->ss_block = s->ss;
        break;
    case 0x46:
        s->gyro_y = (s->gyro_y << 8) | b;
        snprintf(buf, sizeof(buf), "GYRO_YOUT: %d", s->gyro_y);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_WRITE_DATETIME, buf);
        break;
    case 0x47:
        s->gyro_z = (int8_t)b;
        s->ss_block = s->ss;
        break;
    case 0x48:
        s->gyro_z = (s->gyro_z << 8) | b;
        snprintf(buf, sizeof(buf), "GYRO_ZOUT: %d", s->gyro_z);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_WRITE_DATETIME, buf);
        break;
    }
    if (s->reg >= 0x3b && s->reg <= 0x48)
        s->reg++;
}
```

---

## T3: mxc6225xu_c — 方向传感器

### 子任务

| # | 子任务 | 详情 | 状态 |
|---|--------|------|------|
| T3.1 | 创建文件 | `libsigrokdecode/c_decoders/mxc6225xu_c.c` | ⬜ |
| T3.2 | 编写头文件包含 | 同 T1.2 | ⬜ |
| T3.3 | 定义 annotation 枚举 | `ANN_TEXT=0`, `NUM_ANN=1` | ⬜ |
| T3.4 | 定义状态机枚举 | 6 个状态：IDLE, GET_SLAVE_ADDR, GET_REG_ADDR, WRITE_REGS, READ_REGS, READ_REGS2 | ⬜ |
| T3.5 | 定义私有结构体 | `mxc6225xu_priv`：state, reg, ss, es, out_ann | ⬜ |
| T3.6 | 编写静态元数据 | 1 个 ann_label, 1 个 ann_row, inputs, tags | ⬜ |
| T3.7 | 编写位域查找表 | `sh_str[]`, `ori_str[]`, `shth_str[]`, `shc_str[]`, `orc_str[]` | ⬜ |
| T3.8 | 实现 `mxc6225xu_reset()` | 分配+初始化 | ⬜ |
| T3.9 | 实现 `mxc6225xu_start()` | 注册输出 | ⬜ |
| T3.10 | 实现 `handle_reg_0x00()` | XOUT — 8-bit 有符号 x 轴加速度 | ⬜ |
| T3.11 | 实现 `handle_reg_0x01()` | YOUT — 8-bit 有符号 y 轴加速度 | ⬜ |
| T3.12 | 实现 `handle_reg_0x02()` | STATUS — 复杂位域解析（INT/SH/TILT/ORI/OR） | ⬜ |
| T3.13 | 实现 `handle_reg_0x03()` | DETECTION — 掉电/摇动/方向参数 | ⬜ |
| T3.14 | 实现 `mxc6225xu_recv_proto()` | 6 状态状态机 | ⬜ |
| T3.15 | 实现 `mxc6225xu_decode()` | 空函数 | ⬜ |
| T3.16 | 实现 `mxc6225xu_destroy()` | g_free 释放 | ⬜ |
| T3.17 | 定义 `srd_c_decoder` 结构体 | `.id="mxc6225xu_c"`, `.name="MXC6225XU(C)"` | ⬜ |
| T3.18 | 实现导出入口 | `srd_c_decoder_entry()`, `srd_c_decoder_api_version()` | ⬜ |

### 关键实现细节

- **6 状态状态机**：最复杂的状态机，包含写寄存器→重复起始→读寄存器流程
- **位域查找表**：5 个静态字符串数组用于 STATUS/DETECTION 寄存器解析
- **2's complement 处理**：XOUT/YOUT 为 8-bit 有符号数，使用 `(int8_t)b` 转换
- **建议添加 I2C 地址过滤**：默认地址 0x2A

---

## T4: nunchuk_c — Wii 控制器

### 子任务

| # | 子任务 | 详情 | 状态 |
|---|--------|------|------|
| T4.1 | 创建文件 | `libsigrokdecode/c_decoders/nunchuk_c.c` | ⬜ |
| T4.2 | 编写头文件包含 | 同 T1.2 | ⬜ |
| T4.3 | 定义 annotation 枚举 | 15 个 annotation class (ANN_REG_0X00..ANN_WARNINGS) | ⬜ |
| T4.4 | 定义状态机枚举 | `NUNCHUK_IDLE`, `NUNCHUK_GET_SLAVE_ADDR`, `NUNCHUK_READ_REGS`, `NUNCHUK_WRITE_REGS` | ⬜ |
| T4.5 | 定义私有结构体 | `nunchuk_priv`：state, reg, sx/sy/ax/ay/az/bz/bc, init_seq[2], init_seq_count, ss, es, ss_block, es_block, out_ann | ⬜ |
| T4.6 | 编写静态元数据 | 15 个 ann_labels, 3 个 ann_rows, inputs, tags | ⬜ |
| T4.7 | 实现 `nunchuk_reset()` | 分配+初始化，传感器值初始化为 -1 | ⬜ |
| T4.8 | 实现 `nunchuk_start()` | 注册输出 | ⬜ |
| T4.9 | 实现 `handle_reg_0x00()` | SX — 摇杆 X 位置 | ⬜ |
| T4.10 | 实现 `handle_reg_0x01()` | SY — 摇杆 Y 位置 | ⬜ |
| T4.11 | 实现 `handle_reg_0x02()` | AX[9:2] — 加速度 X 高 8 位 | ⬜ |
| T4.12 | 实现 `handle_reg_0x03()` | AY[9:2] — 加速度 Y 高 8 位 | ⬜ |
| T4.13 | 实现 `handle_reg_0x04()` | AZ[9:2] — 加速度 Z 高 8 位 | ⬜ |
| T4.14 | 实现 `handle_reg_0x05()` | 按键 + 加速度低位解析，合并到 ax/ay/az | ⬜ |
| T4.15 | 实现 `output_full_block_if_possible()` | 所有值有效时输出摘要 | ⬜ |
| T4.16 | 实现 `handle_reg_write()` | 记录写数据，收集 init 序列 | ⬜ |
| T4.17 | 实现 `output_init_seq()` | 检查并输出初始化序列结果 | ⬜ |
| T4.18 | 实现 `nunchuk_recv_proto()` | 4 状态状态机 | ⬜ |
| T4.19 | 实现 `nunchuk_decode()` | 空函数 | ⬜ |
| T4.20 | 实现 `nunchuk_destroy()` | g_free 释放 | ⬜ |
| T4.21 | 定义 `srd_c_decoder` 结构体 | `.id="nunchuk_c"`, `.name="Nunchuk(C)"` | ⬜ |
| T4.22 | 实现导出入口 | `srd_c_decoder_entry()`, `srd_c_decoder_api_version()` | ⬜ |

### 关键实现细节

- **6 字节读取序列**：reg 从 0x00 自动递增到 0x05
- **按键 + 低位解析**：reg 0x05 包含 BZ(1bit) + BC(1bit) + AX[1:0](2bit) + AY[1:0](2bit) + AZ[1:0](2bit)
- **按键逻辑反转**：BZ=0 表示按下，BZ=1 表示未按下
- **Summary 输出**：STOP 时检查所有值有效，输出完整摘要
- **Init 序列检测**：写操作收集 2 字节，检查是否为 [0x40, 0x00]
- **位级 annotation 简化**：`putd(bit1, bit2, ...)` 改为 `C_ANN_PUT` 使用当前 ss/es

---

## T5: pca9571_c — I/O 扩展器

### 子任务

| # | 子任务 | 详情 | 状态 |
|---|--------|------|------|
| T5.1 | 创建文件 | `libsigrokdecode/c_decoders/pca9571_c.c` | ⬜ |
| T5.2 | 编写头文件包含 | 同 T1.2 | ⬜ |
| T5.3 | 定义 annotation 枚举 | `ANN_REGISTER=0`, `ANN_VALUE=1`, `ANN_WARNING=2`, `NUM_ANN=3` | ⬜ |
| T5.4 | 定义状态机枚举 | `PCA9571_IDLE`, `PCA9571_GET_SLAVE_ADDR`, `PCA9571_READ_DATA`, `PCA9571_WRITE_DATA` | ⬜ |
| T5.5 | 定义私有结构体 | `pca9571_priv`：state, last_write, last_write_es, ss, es, out_ann | ⬜ |
| T5.6 | 编写静态元数据 | 3 个 ann_labels, 2 个 ann_rows, inputs, tags | ⬜ |
| T5.7 | 实现 `pca9571_reset()` | 分配+初始化，last_write=0xFF | ⬜ |
| T5.8 | 实现 `pca9571_start()` | 注册输出 | ⬜ |
| T5.9 | 实现 `pca9571_handle_io()` | 读/写处理 + 读值与写值一致性检查 | ⬜ |
| T5.10 | 实现 `pca9571_recv_proto()` | 4 状态状态机，地址过滤 0x25 | ⬜ |
| T5.11 | 实现 `pca9571_decode()` | 空函数 | ⬜ |
| T5.12 | 实现 `pca9571_destroy()` | g_free 释放 | ⬜ |
| T5.13 | 定义 `srd_c_decoder` 结构体 | `.id="pca9571_c"`, `.name="PCA9571(C)"` | ⬜ |
| T5.14 | 实现导出入口 | `srd_c_decoder_entry()`, `srd_c_decoder_api_version()` | ⬜ |

### 关键实现细节

- **最简单的解码器**：只有 1 字节读/写操作
- **I2C 地址过滤**：必须检查 `addr == 0x25`
- **START REPEAT 处理**：与 START 等价，都进入 GET_SLAVE_ADDR
- **NACK/STOP 处理**：都重置状态机到 IDLE
- **读值一致性检查**：读取值与上次写入值不同时输出 warning
- **OUTPUT_LOGIC 省略**：C 框架可能不支持，暂不实现逻辑输出
- **last_write 初始值**：0xFF（芯片端口默认高电平）

---

## T6: CMakeLists.txt 更新

### 子任务

| # | 子任务 | 详情 | 状态 |
|---|--------|------|------|
| T6.1 | 在 C_DECODERS 列表中添加 5 个解码器名 | `mlx90614_c`, `mpu6050_c`, `mxc6225xu_c`, `nunchuk_c`, `pca9571_c` | ⬜ |

### 修改位置

在 `CMakeLists.txt` 中搜索 `set(C_DECODERS` 列表，按字母顺序插入新解码器名。

---

## 建议执行顺序

1. **T1 (mlx90614_c)** — 最简单，验证框架正确性
2. **T5 (pca9571_c)** — 简单，验证地址过滤
3. **T3 (mxc6225xu_c)** — 中等，验证多状态机
4. **T4 (nunchuk_c)** — 中等，验证多寄存器+summary
5. **T2 (mpu6050_c)** — 最复杂，最后实现
6. **T6 (CMakeLists.txt)** — 所有解码器完成后统一更新

## 依赖关系

- T1-T5 之间无依赖，可并行实现
- T6 依赖 T1-T5 全部完成
