# Python → C 解码器移植规格书 (Batch 20)

## 概述

本规格书涵盖 5 个 I2C 上层协议解码器的 Python → C 移植工作。所有解码器均以 `inputs=['i2c']` 作为输入，使用 `recv_proto()` 回调接收 I2C 协议数据，而非直接调用 `decode()` 处理原始信号。

### 移植目标解码器

| # | Python id | C id | 芯片名称 | 复杂度 | I2C 地址 |
|---|-----------|------|----------|--------|----------|
| 1 | mlx90614 | mlx90614_c | Melexis MLX90614 红外温度计 | ★☆☆ | 0x5A |
| 2 | mpu6050 | mpu6050_c | InvenSense MPU6050 加速度计/陀螺仪 | ★★★ | 0x68 |
| 3 | mxc6225xu | mxc6225xu_c | MEMSIC MXC6225XU 方向传感器 | ★★☆ | 0x2A |
| 4 | nunchuk | nunchuk_c | Nintendo Wii Nunchuk 控制器 | ★★☆ | 0x52 |
| 5 | pca9571 | pca9571_c | NXP PCA9571 8-bit I/O 扩展器 | ★☆☆ | 0x25 |

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| i2c_c.c | 底层协议输出范本 | START/STOP条件检测、c_decoder_put_python()输出I2C协议数据 |
| c_decoder_utils.h | BITS v2 格式文档 | BITS 消息格式定义及解析示例代码 |

参考实现要点：
- `recv_proto()` 回调实现
- 状态机模式
- `c_decoder_register_output()` 注册输出
- `C_ANN_PUT` 宏输出 annotation
- `c_decoder_get_private()` / `c_decoder_set_private()` 管理私有状态
- `srd_c_decoder_entry()` / `srd_c_decoder_api_version()` 导出入口
- `c_decoder_put_logic()` 输出逻辑信号（PCA9571 使用）


## C 解码器架构规范

### 文件命名

文件路径：`libsigrokdecode/c_decoders/<id>_c.c`

例如：`libsigrokdecode/c_decoders/mlx90614_c.c`

### 核心结构

每个 C 解码器必须包含以下组件：

#### 1. Annotation 枚举

```c
enum {
    ANN_XXX = 0,
    ANN_YYY,
    NUM_ANN,
};
```

#### 2. 状态机枚举

```c
enum <decoder>_state {
    STATE_IDLE,
    STATE_GET_SLAVE_ADDR,
    STATE_XXX,
    ...
};
```

#### 3. 私有数据结构

```c
typedef struct {
    enum <decoder>_state state;
    int reg;
    uint64_t ss, es;
    uint64_t ss_block, es_block;
    int out_ann;
    // 解码器特有字段
} <decoder>_priv;
```

#### 4. 静态元数据表

- `inputs[]` — 必须为 `{"i2c", NULL}`
- `tags[]` — 从 Python `tags` 字段映射
- `ann_labels[][3]` — 第一列必须为 `""`，第二列为 Python annotation id，第三列为描述
- `annotation_rows[]` — 所有 annotation class 必须映射到某个 row
- `options[]` — 从 Python `options` 字段映射（如有）

#### 5. 核心回调函数

| 回调 | 用途 | 说明 |
|------|------|------|
| `reset()` | 初始化/重置私有状态 | 首次调用时 `g_malloc0` 分配私有数据 |
| `start()` | 注册输出、读取选项 | 调用 `c_decoder_register_output()` |
| `decode()` | 空函数 | I2C 上层解码器不使用，留空 |
| `recv_proto()` | **核心** — 接收 I2C 协议数据 | 实现状态机逻辑 |
| `destroy()` | 释放私有数据 | `g_free()` 释放 |

#### 6. `srd_c_decoder` 结构体

```c
struct srd_c_decoder <decoder>_c_decoder = {
    .id = "<id>_c",
    .name = "<Name>(C)",
    .longname = "...",
    .desc = "... (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = ...,
    .num_options = ...,
    .num_annotations = NUM_ANN,
    .ann_labels = ...,
    .num_annotation_rows = ...,
    .annotation_rows = ...,
    .inputs = ...,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = ...,
    .num_tags = ...,
    .reset = <decoder>_reset,
    .start = <decoder>_start,
    .decode = <decoder>_decode,
    .destroy = <decoder>_destroy,
    .recv_proto = <decoder>_recv_proto,
};
```

#### 7. 导出入口

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    // 初始化 options 的 def 和 values
    return &<decoder>_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

### `recv_proto()` 签名

```c
static void <decoder>_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

**参数说明：**
- `cmd` — I2C 协议命令字符串，可能值：
  - `"START"` / `"START REPEAT"` — 起始条件
  - `"STOP"` — 停止条件
  - `"ADDRESS READ"` / `"ADDRESS WRITE"` — 地址字节（data[0] 为从机地址）
  - `"DATA READ"` / `"DATA WRITE"` — 数据字节（data[0] 为数据值）
  - `"ACK"` / `"NACK"` — 应答
  - `"BITS"` — 位级数据（BITS v2 格式，含 per-bit ss/es 时间戳；通常忽略，详见通用注意事项） <!-- Updated: BITS v2 数据实际可通过 recv_proto 获取，i2c_c.c 会输出 BITS 命令 -->
- `data` / `data_len` — 附加数据字节
- `start_sample` / `end_sample` — 采样点范围

### Annotation 输出宏

```c
C_ANN_PUT(di, start_sample, end_sample, out_ann, ann_class, text_string);
```

---

## 解码器详细规格

---

### 1. MLX90614 — 红外温度计

#### Python 元数据

| 字段 | 值 |
|------|-----|
| id | `mlx90614` |
| name | `MLX90614` |
| longname | `Melexis MLX90614` |
| desc | `Melexis MLX90614 infrared thermometer protocol.` |
| inputs | `['i2c']` |
| outputs | `[]` |
| tags | `['IC', 'Sensor']` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |

#### Python Annotations

| Index | id | 描述 |
|-------|-----|------|
| 0 | celsius | Temperature / °C |
| 1 | kelvin | Temperature / K |

#### Python Annotation Rows

| Row id | 描述 | 包含的 annotation class 索引 |
|--------|------|------------------------------|
| temps-celsius | Temperature / °C | (0,) |
| temps-kelvin | Temperature / K | (1,) |

#### C 映射

```c
enum {
    ANN_CELSIUS = 0,
    ANN_KELVIN,
    NUM_ANN,
};

enum mlx90614_state {  <!-- Updated: 状态名从 IGNORE_ADDRESS_WRITE 改为 IGNORE_ADDRESS_READ，匹配 MLX90614 读温度协议 -->
    MLX90614_IGNORE_START_REPEAT,
    MLX90614_IGNORE_ADDRESS_READ,
    MLX90614_GET_TEMPERATURE,
};

typedef struct {
    enum mlx90614_state state;
    uint8_t data[2];
    int data_count;
    uint64_t ss, es;
    int out_ann;
} mlx90614_priv;

static const char *mlx90614_ann_labels[][3] = {
    {"", "celsius", "Temperature / °C"},
    {"", "kelvin", "Temperature / K"},
};

static const int mlx90614_row_celsius_classes[] = {ANN_CELSIUS};
static const int mlx90614_row_kelvin_classes[] = {ANN_KELVIN};
static const struct srd_c_ann_row mlx90614_ann_rows[] = {
    {"temps-celsius", "Temperature / °C", mlx90614_row_celsius_classes, 1},
    {"temps-kelvin", "Temperature / K", mlx90614_row_kelvin_classes, 1},
};
```

#### 状态机逻辑

```
IGNORE_START_REPEAT:  <!-- Updated: Python 原始逻辑等待 ADDRESS WRITE + DATA WRITE，但 MLX90614 读温度协议实际使用 ADDRESS READ + DATA READ，C 实现应修正 -->
  → 收到 "START REPEAT" → IGNORE_ADDRESS_READ
  → 其他 → 忽略

IGNORE_ADDRESS_READ:
  → 收到 "ADDRESS READ" → GET_TEMPERATURE
  → 其他 → 忽略

GET_TEMPERATURE:
  → 收到 "DATA READ":
      data_count == 0 → 保存 data[0] 到 data[0]，记录 ss
      data_count == 1 → 保存 data[0] 到 data[1]，记录 es，计算温度并输出:
          kelvin = (data[0] | (data[1] << 8)) * 0.02
          celsius = kelvin - 273.15
          输出 ANN_CELSIUS: "Temperature: %3.2f °C"
          输出 ANN_KELVIN: "Temperature: %3.2f K"
          重置状态为 IGNORE_START_REPEAT，清空 data
      data_count++
  → 其他 → 忽略
```

**注意：** Python 实现中 data_count 从 0 开始，第一个字节记录 ss，第二个字节记录 es，第三个字节触发计算。但实际只有 2 个数据字节，第三个 "字节" 实际上不存在——Python 代码有 bug（注释说 "Quick hack implementation"）。正确逻辑应为：收集 2 字节后即输出温度。C 实现应修正此逻辑。

<!-- Updated: Python 代码还有另一个协议级 bug：MLX90614 读温度的标准流程是 START→ADDRESS WRITE→命令字节→START REPEAT→ADDRESS READ→DATA READ×2，但 Python 代码在 START REPEAT 后等待 ADDRESS WRITE + DATA WRITE，与实际协议不符。C 实现应修正为等待 ADDRESS READ + DATA READ。 -->

**C 实现修正方案：** <!-- Updated: 修正 Python 的两个 bug：(1) ADDRESS WRITE→ADDRESS READ, DATA WRITE→DATA READ; (2) 2字节后立即计算 -->

```c
// 收集 2 字节后立即输出（修正 Python 的 data_count==2 bug）
// 使用 DATA READ（修正 Python 的 DATA WRITE bug）
if (s->data_count == 0) {
    s->data[0] = databyte;
    s->ss = start_sample;
    s->data_count = 1;
} else if (s->data_count == 1) {
    s->data[1] = databyte;
    s->es = end_sample;
    // 计算温度
    double kelvin = (double)(s->data[0] | (s->data[1] << 8)) * 0.02;
    double celsius = kelvin - 273.15;
    char buf[64];
    snprintf(buf, sizeof(buf), "Temperature: %.2f °C", celsius);
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_CELSIUS, buf);
    snprintf(buf, sizeof(buf), "Temperature: %.2f K", kelvin);
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_KELVIN, buf);
    // 重置
    s->state = MLX90614_IGNORE_START_REPEAT;
    s->data_count = 0;
}
```

#### `srd_c_decoder` 结构体

```c
struct srd_c_decoder mlx90614_c_decoder = {
    .id = "mlx90614_c",
    .name = "MLX90614(C)",
    .longname = "Melexis MLX90614 (C)",
    .desc = "Melexis MLX90614 infrared thermometer protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL, .num_channels = 0,
    .optional_channels = NULL, .num_optional_channels = 0,
    .options = NULL, .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = mlx90614_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = mlx90614_ann_rows,
    .inputs = mlx90614_inputs, .num_inputs = 1,
    .outputs = NULL, .num_outputs = 0,
    .binary = NULL, .num_binary = 0,
    .tags = mlx90614_tags, .num_tags = 2,
    .reset = mlx90614_reset,
    .start = mlx90614_start,
    .decode = mlx90614_decode,
    .destroy = mlx90614_destroy,
    .recv_proto = mlx90614_recv_proto,
};
```

---

### 2. MPU6050 — 加速度计/陀螺仪

#### Python 元数据

| 字段 | 值 |
|------|-----|
| id | `mpu6050` |
| name | `MPU6050` |
| longname | `InvenSense MPU6050` |
| desc | `Accelerometer module protocol.` |
| inputs | `['i2c']` |
| outputs | `['mpu6050']` |
| tags | `['Gyroscope']` |
| I2C 地址 | `0x68` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |

#### Python Annotations（29 个）

**寄存器 annotations (0-8)：**

| Index | id | 描述 |
|-------|-----|------|
| 0 | reg-smplrt-div | SMPLRT_DIV register |
| 1 | reg-config | CONFIG register |
| 2 | reg-gyro-config | GYRO_CONFIG register |
| 3 | reg-accel-config | ACCEL_CONFIG register |
| 4 | reg-int-enable | INT_ENABLE register |
| 5 | reg-pwr-mgmt-1 | PWR_MGMT_1 register |
| 6 | reg-who-am-i | WHO_AM_I register |
| 7 | reg-data | Data register |
| 8 | reg-reserved | Reserved register |

**位 annotations (9-23)：**

| Index | id | 描述 |
|-------|-----|------|
| 9 | bit-fs-sel | FS_SEL (gyro full scale select) |
| 10 | bit-afs-sel | AFS_SEL (accel full scale select) |
| 11 | bit-clk-sel | CLK_SEL (clock select) |
| 12 | bit-sleep | SLEEP bit |
| 13 | bit-cycle | CYCLE bit |
| 14 | bit-temp-dis | TEMP_DIS bit |
| 15 | bit-reset | DEVICE_RESET bit |
| 16 | bit-dlpf | Digital low pass filter bits |
| 17 | bit-ext-sync | External sync bits |
| 18 | bit-int-en | Interrupt enable bits |
| 19 | bit-xg-st | XG_ST (gyro X self-test) |
| 20 | bit-yg-st | YG_ST (gyro Y self-test) |
| 21 | bit-zg-st | ZG_ST (gyro Z self-test) |
| 22 | bit-xa-st | XA_ST (accel X self-test) |
| 23 | bit-ya-st | YA_ST (accel Y self-test) |

**功能 annotations (24-28)：** <!-- Updated: Python 原始定义的 indices 24-28 为 data-out/write-datetime/reg-read/reg-write/warning，但 Python 代码实际只用 index 25 输出所有 accel/gyro/temp 数据，与 annotation_rows 的 'AccelXYZ/Temp/GyroXYZ' 行定义不匹配。C 实现对此进行了修正，将 24-26 拆分为 accel-data/gyro-data/temp-data，与 annotation_rows 语义一致。 -->

| Index | id | 描述 |
|-------|-----|------|
| 24 | accel-data | Accelerometer data |
| 25 | gyro-data | Gyroscope data |
| 26 | temp-data | Temperature data |
| 27 | reg-write | Register write |
| 28 | warning | Warning |

#### Python Annotation Rows

| Row id | 描述 | 包含的 annotation class 索引 |
|--------|------|------------------------------|
| bits | Bits | (9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23) |
| regs | Registers | (0, 1, 2, 3, 4, 5, 6, 7, 8) |
| accel-gyro | AccelXYZ/Temp/GyroXYZ | (24, 25, 26, 27) |
| warnings | Warnings | (28,) |

#### C 映射

```c
enum {
    ANN_REG_SMPLRT_DIV = 0,
    ANN_REG_CONFIG,
    ANN_REG_GYRO_CONFIG,
    ANN_REG_ACCEL_CONFIG,
    ANN_REG_INT_ENABLE,
    ANN_REG_PWR_MGMT_1,
    ANN_REG_WHO_AM_I,
    ANN_REG_DATA,
    ANN_REG_RESERVED,
    ANN_BIT_FS_SEL,
    ANN_BIT_AFS_SEL,
    ANN_BIT_CLK_SEL,
    ANN_BIT_SLEEP,
    ANN_BIT_CYCLE,
    ANN_BIT_TEMP_DIS,
    ANN_BIT_RESET,
    ANN_BIT_DLPF,
    ANN_BIT_EXT_SYNC,
    ANN_BIT_INT_EN,
    ANN_BIT_XG_ST,
    ANN_BIT_YG_ST,
    ANN_BIT_ZG_ST,
    ANN_BIT_XA_ST,
    ANN_BIT_YA_ST,
    ANN_ACCEL_DATA,
    ANN_GYRO_DATA,
    ANN_TEMP_DATA,
    ANN_REG_WRITE,
    ANN_WARNING,
    NUM_ANN,
};

enum mpu6050_state {
    MPU6050_IDLE,
    MPU6050_GET_SLAVE_ADDR,
    MPU6050_GET_REG_ADDR,
    MPU6050_WRITE_REGS,
};

typedef struct {
    enum mpu6050_state state;
    int reg;
    int acc_x, acc_y, acc_z;
    int temp;
    int gyro_x, gyro_y, gyro_z;
    uint64_t ss, es;
    uint64_t ss_block;
    int out_ann;
} mpu6050_priv;
```

#### 状态机逻辑

```
IDLE:
  → "START" → GET_SLAVE_ADDR

GET_SLAVE_ADDR:
  → "ADDRESS WRITE" 或 "ADDRESS READ":
      检查地址 == 0x68
      匹配 → GET_REG_ADDR
      不匹配 → IDLE + 输出 warning
  → 其他 → 忽略

GET_REG_ADDR:
  → "START REPEAT" → GET_SLAVE_ADDR
  → "STOP" → IDLE
  → "DATA WRITE" → 保存 databyte 为 reg，进入 WRITE_REGS
  → "DATA READ" → handle_reg(databyte)

WRITE_REGS:
  → "STOP" → IDLE
  → "START REPEAT" → GET_SLAVE_ADDR
  → "DATA WRITE" → handle_reg(databyte)
```

<!-- Updated: MPU6050 Python 代码的 putd() 使用 self.bits 数据进行位级 annotation（如 FS_SEL、SLEEP 等位域），C 实现中这些位级 annotation 应使用当前 ss/es 范围输出，或解析 BITS v2 数据获取精确时间戳。寄存器级 annotation（如 GYRO_CONFIG、ACCEL_CONFIG）可直接使用当前 ss/es。 -->

#### 寄存器处理函数

**特殊寄存器处理：**

| 寄存器 | 函数 | 说明 |
|--------|------|------|
| 0x1B | `handle_reg_0x1b()` | GYRO_CONFIG — 自检 + FS_SEL 量程 |
| 0x1C | `handle_reg_0x1c()` | ACCEL_CONFIG — 自检 + AFS_SEL 量程 |
| 0x38 | `handle_reg_0x38()` | INT_ENABLE — 中断使能 |
| 0x6B | `handle_reg_0x6b()` | PWR_MGMT_1 — 电源管理 |
| 0x75 | `handle_reg_0x75()` | WHO_AM_I — 设备 ID |
| 0x3B-0x48 | `handle_reg_data()` | 加速度/温度/陀螺仪数据 |

**数据寄存器映射 (0x3B-0x48)：**

| 寄存器 | 含义 | 处理 |
|--------|------|------|
| 0x3B | ACCEL_XOUT[15:8] | acc_x = b; ss_block = ss |
| 0x3C | ACCEL_XOUT[7:0] | acc_x += b/1000000; 输出 ACCEL_XOUT |
| 0x3D | ACCEL_YOUT[15:8] | acc_y = b; ss_block = ss |
| 0x3E | ACCEL_YOUT[7:0] | acc_y += b/1000000; 输出 ACCEL_YOUT |
| 0x3F | ACCEL_ZOUT[15:8] | acc_z = b; ss_block = ss |
| 0x40 | ACCEL_ZOUT[7:0] | acc_z += b/1000000; 输出 ACCEL_ZOUT |
| 0x41 | TEMP_OUT[15:8] | temp = b << 8; ss_block = ss |
| 0x42 | TEMP_OUT[7:0] | temp += b; temp = temp/340 + 36.53; 输出 TEMP |
| 0x43 | GYRO_XOUT[15:8] | gyro_x = b; ss_block = ss |
| 0x44 | GYRO_XOUT[7:0] | gyro_x += b/1000000; 输出 GYRO_XOUT |
| 0x45 | GYRO_YOUT[15:8] | gyro_y = b; ss_block = ss |
| 0x46 | GYRO_YOUT[7:0] | gyro_y += b/1000000; 输出 GYRO_YOUT |
| 0x47 | GYRO_ZOUT[15:8] | gyro_z = b; ss_block = ss |
| 0x48 | GYRO_ZOUT[7:0] | gyro_z += b/1000000; 输出 GYRO_ZOUT |

**注意：** Python 代码中 `acc_x += b / 1000000` 的写法看起来有误（应该是 `acc_x = (acc_x << 8) | b` 或 `acc_x = acc_x * 256 + b`），C 实现应修正为正确的 16 位有符号整数拼接。

**C 实现修正方案（数据寄存器）：**

```c
static void mpu6050_handle_reg_data(struct srd_decoder_inst *di, mpu6050_priv *s, uint8_t b)
{
    char buf[64];
    switch (s->reg) {
    case 0x3b:
        s->acc_x = (int8_t)b;  // 高字节有符号
        s->ss_block = s->ss;
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_REG_DATA, "ACCEL_XOUT[15:8]");
        break;
    case 0x3c:
        s->acc_x = (s->acc_x << 8) | b;  // 拼接低字节
        snprintf(buf, sizeof(buf), "ACCEL_XOUT: %d", s->acc_x);
        C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_ACCEL_DATA, buf);
        break;
    // ... 类似处理 0x3d-0x48
    case 0x41:
        s->temp = (int16_t)(b << 8);
        s->ss_block = s->ss;
        break;
    case 0x42:
        s->temp |= b;
        {
            double temp_c = (double)s->temp / 340.0 + 36.53;
            snprintf(buf, sizeof(buf), "TEMP: %.1f°C", temp_c);
            C_ANN_PUT(di, s->ss_block, s->es, s->out_ann, ANN_TEMP_DATA, buf);
        }
        break;
    }
    if (s->reg >= 0x3b && s->reg <= 0x48)
        s->reg++;
}
```

#### `srd_c_decoder` 结构体

```c
struct srd_c_decoder mpu6050_c_decoder = {
    .id = "mpu6050_c",
    .name = "MPU6050(C)",
    .longname = "InvenSense MPU6050 (C)",
    .desc = "Accelerometer module protocol. (C implementation)",
    .license = "gplv2+",
    // ... 其余字段按规范填写
};
```

---

### 3. MXC6225XU — 方向传感器

#### Python 元数据

| 字段 | 值 |
|------|-----|
| id | `mxc6225xu` |
| name | `MXC6225XU` |
| longname | `MEMSIC MXC6225XU` |
| desc | `Digital Thermal Orientation Sensor (DTOS) protocol.` |
| inputs | `['i2c']` |
| outputs | `[]` |
| tags | `['IC', 'Sensor']` |
| I2C 地址 | `0x2A` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |

#### Python Annotations

| Index | id | 描述 |
|-------|-----|------|
| 0 | text | Human-readable text |

#### Python Annotation Rows

Python 源码未定义 `annotation_rows`（只有 1 个 annotation class 时可省略）。C 实现仍需定义至少 1 个 row。

#### C 映射

```c
enum {
    ANN_TEXT = 0,
    NUM_ANN,
};

enum mxc6225xu_state {
    MXC6225XU_IDLE,
    MXC6225XU_GET_SLAVE_ADDR,
    MXC6225XU_GET_REG_ADDR,
    MXC6225XU_WRITE_REGS,
    MXC6225XU_READ_REGS,
    MXC6225XU_READ_REGS2,
};

typedef struct {
    enum mxc6225xu_state state;
    int reg;
    uint64_t ss, es;
    int out_ann;
} mxc6225xu_priv;

static const char *mxc6225xu_ann_labels[][3] = {
    {"", "text", "Human-readable text"},
};

static const int mxc6225xu_row_text_classes[] = {ANN_TEXT};
static const struct srd_c_ann_row mxc6225xu_ann_rows[] = {
    {"text", "Human-readable text", mxc6225xu_row_text_classes, 1},
};
```

#### 状态机逻辑

```
IDLE:
  → "START" → GET_SLAVE_ADDR

GET_SLAVE_ADDR:
  → "ADDRESS WRITE" → GET_REG_ADDR
  → 其他 → 忽略

GET_REG_ADDR:
  → "DATA WRITE" → 保存 databyte 为 reg，进入 WRITE_REGS
  → 其他 → 忽略

WRITE_REGS:
  → "START REPEAT" → READ_REGS
  → "DATA WRITE" → handle_reg(databyte), reg++
  → "STOP" → IDLE

READ_REGS:
  → "ADDRESS READ" → READ_REGS2
  → 其他 → 忽略

READ_REGS2:
  → "DATA READ" → handle_reg(databyte), reg++
  → "STOP" → IDLE
```

#### 寄存器处理函数

| 寄存器 | 函数 | 说明 |
|--------|------|------|
| 0x00 | `handle_reg_0x00()` | XOUT — 8-bit x 轴加速度（2's complement） |
| 0x01 | `handle_reg_0x01()` | YOUT — 8-bit y 轴加速度（2's complement） |
| 0x02 | `handle_reg_0x02()` | STATUS — 方向和摇动状态（复杂位域解析） |
| 0x03 | `handle_reg_0x03()` | DETECTION — 掉电/方向/摇动检测参数（只写） |

**STATUS 寄存器 (0x02) 位域：**

| Bits | 字段 | 含义 |
|------|------|------|
| [7] | INT | 0=方向未变且无摇动, 1=方向改变或有摇动 |
| [6:5] | SH[1:0] | 摇动事件: 00=none, 01=shake left, 10=shake right, 11=undefined |
| [4] | TILT | 0=方向测量有效, 1=方向测量无效 |
| [3:2] | ORI[1:0] | 方向: 00=竖直正立, 01=顺时针90°, 10=竖直倒立, 11=逆时针90° |
| [1:0] | OR[1:0] | 方向结果（同 ORI 格式） |

**DETECTION 寄存器 (0x03) 位域：**

| Bits | 字段 | 含义 |
|------|------|------|
| [7] | PD | 0=不掉电, 1=掉电 |
| [6] | SHM | 摇动模式 |
| [5:4] | SHTH[1:0] | 摇动阈值: 00=0.5g, 01=1.0g, 10=1.5g, 11=2.0g |
| [3:2] | SHC[1:0] | 摇动计数: 00=16, 01=32, 10=64, 11=128 |
| [1:0] | ORC[1:0] | 方向计数: 00=16, 01=32, 10=64, 11=128 |

**C 代码片段 — STATUS 寄存器处理：**

```c
static const char *mxc6225xu_sh_str[] = {"none", "shake left", "shake right", "undefined"};
static const char *mxc6225xu_ori_str[] = {
    "vertical in upright orientation",
    "rotated 90 degrees clockwise",
    "vertical in inverted orientation",
    "rotated 90 degrees counterclockwise",
};

static void mxc6225xu_handle_reg_0x02(struct srd_decoder_inst *di, mxc6225xu_priv *s, uint8_t b)
{
    char buf[256];
    int pos = 0;

    int int_val = (b >> 7) & 1;
    const char *int_s = (int_val == 0) ? "unchanged and no" : "changed or";
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "INT = %d: Orientation %s shake event occurred\n", int_val, int_s);

    int sh = (((b >> 6) & 1) << 1) | ((b >> 5) & 1);
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "SH[1:0] = %d: Shake event: %s\n", sh, mxc6225xu_sh_str[sh]);

    int tilt = (b >> 4) & 1;
    const char *tilt_s = (tilt == 0) ? "" : "not ";
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "TILT = %d: Orientation measurement is %svalid\n", tilt, tilt_s);

    int ori = (((b >> 3) & 1) << 1) | ((b >> 2) & 1);
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "ORI[1:0] = %d: %s\n", ori, mxc6225xu_ori_str[ori]);

    int or_val = (((b >> 1) & 1) << 1) | ((b >> 0) & 1);
    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "OR[1:0] = %d: %s\n", or_val, mxc6225xu_ori_str[or_val]);

    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, buf);
}
```

---

### 4. Nunchuk — Wii 控制器

#### Python 元数据

| 字段 | 值 |
|------|-----|
| id | `nunchuk` |
| name | `Nunchuk` |
| longname | `Nintendo Wii Nunchuk` |
| desc | `Nintendo Wii Nunchuk controller protocol.` |
| inputs | `['i2c']` |
| outputs | `[]` |
| tags | `['Sensor']` |
| I2C 地址 | `0x52`（标准 Nunchuk 地址，Python 代码未显式过滤） |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |

#### Python Annotations（15 个）

| Index | id | 描述 |
|-------|-----|------|
| 0 | reg-0x00 | Register 0x00 — Analog stick X |
| 1 | reg-0x01 | Register 0x01 — Analog stick Y |
| 2 | reg-0x02 | Register 0x02 — Accelerometer X[9:2] |
| 3 | reg-0x03 | Register 0x03 — Accelerometer Y[9:2] |
| 4 | reg-0x04 | Register 0x04 — Accelerometer Z[9:2] |
| 5 | reg-0x05 | Register 0x05 — 按键 + 加速度低位 |
| 6 | bit-bz | BZ bit — Z 按键状态 |
| 7 | bit-bc | BC bit — C 按键状态 |
| 8 | bit-ax | AX bits — 加速度 X[1:0] |
| 9 | bit-ay | AY bits — 加速度 Y[1:0] |
| 10 | bit-az | AZ bits — 加速度 Z[1:0] |
| 11 | nunchuk-write | Nunchuk write |
| 12 | cmd-init | Init command |
| 13 | summary | Summary |
| 14 | warnings | Warnings |

#### Python Annotation Rows

| Row id | 描述 | 包含的 annotation class 索引 |
|--------|------|------------------------------|
| regs | Registers | (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12) |
| summary | Summary | (13,) |
| warnings | Warnings | (14,) |

#### C 映射

```c
enum {
    ANN_REG_0X00 = 0,
    ANN_REG_0X01,
    ANN_REG_0X02,
    ANN_REG_0X03,
    ANN_REG_0X04,
    ANN_REG_0X05,
    ANN_BIT_BZ,
    ANN_BIT_BC,
    ANN_BIT_AX,
    ANN_BIT_AY,
    ANN_BIT_AZ,
    ANN_NUNCHUK_WRITE,
    ANN_CMD_INIT,
    ANN_SUMMARY,
    ANN_WARNINGS,
    NUM_ANN,
};

enum nunchuk_state {
    NUNCHUK_IDLE,
    NUNCHUK_GET_SLAVE_ADDR,
    NUNCHUK_READ_REGS,
    NUNCHUK_WRITE_REGS,
};

typedef struct {
    enum nunchuk_state state;
    int reg;
    int sx, sy, ax, ay, az, bz, bc;
    uint8_t init_seq[2];
    int init_seq_count;
    uint64_t ss, es;
    uint64_t ss_block, es_block;
    int out_ann;
} nunchuk_priv;
```

#### 状态机逻辑

```
IDLE:
  → "START" → GET_SLAVE_ADDR, 记录 ss_block

GET_SLAVE_ADDR:
  → "ADDRESS READ" → READ_REGS
  → "ADDRESS WRITE" → WRITE_REGS
  → 其他 → 忽略

READ_REGS:
  → "DATA READ" → handle_reg(reg, databyte), reg++
  → "STOP" → output_full_block_if_possible(), 重置传感器值, IDLE

WRITE_REGS:
  → "DATA WRITE" → handle_reg_write(databyte)
  → "STOP" → output_init_seq(), 清空 init_seq, IDLE
```

#### 寄存器读取处理

| 寄存器 | 处理 |
|--------|------|
| 0x00 | sx = databyte; 输出 "Analog stick X position: 0x%02X" |
| 0x01 | sy = databyte; 输出 "Analog stick Y position: 0x%02X" |
| 0x02 | ax = databyte << 2; 输出 "Accelerometer X value bits[9:2]: 0x%03X" |
| 0x03 | ay = databyte << 2; 输出 "Accelerometer Y value bits[9:2]: 0x%03X" |
| 0x04 | az = databyte << 2; 输出 "Accelerometer Z value bits[9:2]: 0x%03X" |
| 0x05 | 解析 BZ/BC/AX[1:0]/AY[1:0]/AZ[1:0]; 合并到 ax/ay/az; 输出按键和低位 |

#### Summary 输出

当所有值有效（非 -1）时，在 STOP 时输出完整摘要：
```
"Analog stick: %d/%d, accelerometer: %d/%d/%d, Z: %s, C: %s"
```

#### 初始化序列检测

写操作收集最多 2 字节，STOP 时检查：
- 长度 != 2 → 输出 warning
- 内容 != [0x40, 0x00] → 输出 warning
- 内容 == [0x40, 0x00] → 输出 "Initialize Nunchuk"

**注意：** Python 代码中 `putd()` 使用了 `self.bits` 数据（来自 BITS 包）进行位级 annotation。C `recv_proto` 可通过 "BITS" 命令获取 BITS v2 格式数据（含 per-bit ss/es 时间戳），但解析复杂。C 实现建议使用 `C_ANN_PUT` 直接输出到当前 ss/es 范围，忽略位级 annotation 的精确时间戳。如需精确位级时间戳，可解析 BITS v2 数据（参见 `c_decoder_utils.h`）。 <!-- Updated: 原文说"C recv_proto 不提供位级数据"不准确，i2c_c.c 实际会输出 BITS v2 命令 -->

---

### 5. PCA9571 — I/O 扩展器

#### Python 元数据

| 字段 | 值 |
|------|-----|
| id | `pca9571` |
| name | `PCA9571` |
| longname | `NXP PCA9571` |
| desc | `NXP PCA9571 8-bit I²C output expander.` |
| inputs | `['i2c']` |
| outputs | `[]` |
| tags | `['Embedded/industrial', 'IC']` |
| I2C 地址 | `0x25` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |

#### Python Annotations

| Index | id | 描述 |
|-------|-----|------|
| 0 | register | Register type |
| 1 | value | Register value |
| 2 | warning | Warning |

#### Python Annotation Rows

| Row id | 描述 | 包含的 annotation class 索引 |
|--------|------|------------------------------|
| regs | Registers | (0, 1) |
| warnings | Warnings | (2,) |

#### Python logic_output_channels

```python
logic_output_channels = logic_channels(8)
# = (('p0', 'P0'), ('p1', 'P1'), ..., ('p7', 'P7'))
```

**注意：** PCA9571 Python 解码器使用了 `OUTPUT_LOGIC` 输出类型，这是 5 个解码器中唯一使用逻辑输出的。C 解码器框架已支持 `SRD_OUTPUT_LOGIC` 和 `c_decoder_put_logic()` API（参见 `c_decoder_api.c`），但 `srd_c_decoder` 结构体缺少 `logic_output_channels` 字段，无法声明逻辑输出通道名称（P0-P7）。C 实现方案：(1) 在 `start()` 中通过 `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "pca9571")` 注册逻辑输出；(2) 在 `handle_io()` 中通过 `c_decoder_put_logic()` 输出逻辑数据；(3) 通道名称声明暂不可用，待框架扩展。 <!-- Updated: 原文说"C框架对OUTPUT_LOGIC的支持需要确认"已过时，API已实现但struct缺少通道名声明字段 -->

#### C 映射

```c
enum {
    ANN_REGISTER = 0,
    ANN_VALUE,
    ANN_WARNING,
    NUM_ANN,
};

enum pca9571_state {
    PCA9571_IDLE,
    PCA9571_GET_SLAVE_ADDR,
    PCA9571_READ_DATA,
    PCA9571_WRITE_DATA,
};

typedef struct {
    enum pca9571_state state;
    uint8_t last_write;
    uint64_t last_write_es;
    uint64_t ss, es;
    int out_ann;
    int out_logic;  <!-- Updated: SRD_OUTPUT_LOGIC API 已可用，取消注释 -->
} pca9571_priv;

static const char *pca9571_ann_labels[][3] = {
    {"", "register", "Register type"},
    {"", "value", "Register value"},
    {"", "warning", "Warning"},
};

static const int pca9571_row_regs_classes[] = {ANN_REGISTER, ANN_VALUE};
static const int pca9571_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row pca9571_ann_rows[] = {
    {"regs", "Registers", pca9571_row_regs_classes, 2},
    {"warnings", "Warnings", pca9571_row_warnings_classes, 1},
};
```

<!-- Updated: start() 函数需注册逻辑输出: s->out_logic = c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "pca9571"); -->

#### 状态机逻辑

```
IDLE:
  → "ACK" / "BITS" → 忽略
  → "START" / "START REPEAT" → GET_SLAVE_ADDR
  → "NACK" / "STOP" → IDLE

GET_SLAVE_ADDR:
  → "ADDRESS READ":
      地址 == 0x25 → READ_DATA
      其他 → IDLE + warning
  → "ADDRESS WRITE":
      地址 == 0x25 → WRITE_DATA
      其他 → IDLE + warning
  → 其他 → IDLE

READ_DATA:
  → "DATA READ" → handle_io(b): 输出 "Outputs read: %02X", 检查与 last_write 是否一致
  → "DATA WRITE" → handle_io(b) (不应出现)
  → "STOP" / "NACK" → IDLE

WRITE_DATA:
  → "DATA WRITE" → handle_io(b): 输出 "Outputs set: %02X", 更新 last_write
  → "STOP" / "NACK" → IDLE
```

#### handle_io 逻辑

```c
static void pca9571_handle_io(struct srd_decoder_inst *di, pca9571_priv *s, uint8_t b)
{
    char buf[64];
    if (s->state == PCA9571_READ_DATA) {
        snprintf(buf, sizeof(buf), "Outputs read: %02X", b);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_VALUE, buf);
        if (b != s->last_write) {
            snprintf(buf, sizeof(buf),
                "Warning: read value and last write value (%02X) are different", s->last_write);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARNING, buf);
        }
    } else {
        snprintf(buf, sizeof(buf), "Outputs set: %02X", b);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_VALUE, buf);
        s->last_write = b;
        s->last_write_es = s->es;
        /* 输出逻辑信号 — 从 last_write_es 到当前 es */
        /* <!-- Updated: c_decoder_put_logic() API 已可用，补充逻辑输出实现 --> */
        uint8_t logic_val = b;
        c_decoder_put_logic(di, s->last_write_es, s->es, s->out_logic, 0xFF, &logic_val, 8);
    }
}
```

<!-- Updated: Python 代码的 put_logic_states() 在写操作和 flush 时输出逻辑信号，C 实现应在 WRITE_DATA 的 handle_io 中输出逻辑信号，并在 destroy/end 回调中 flush 最后一段逻辑输出 -->

---

## 通用注意事项

### 1. I2C 地址过滤

Python 解码器对 I2C 地址的处理方式不一致：
- **MPU6050**：显式检查地址 == 0x68，不匹配输出 warning
- **PCA9571**：显式检查地址 == 0x25，不匹配输出 warning
- **MXC6225XU**：注释说"应该只处理目标从机"但未实现
- **MLX90614**：不检查地址
- **Nunchuk**：不检查地址

C 实现建议：所有解码器都应添加地址过滤，通过 option 配置默认地址。

### 2. BITS 包处理 <!-- Updated: 原文说"C recv_proto 不提供 BITS 数据"不准确 -->

Python 解码器中部分使用 `self.bits` 数据（来自 BITS 包）进行位级 annotation。C `recv_proto` **可以**通过 `"BITS"` 命令获取 BITS v2 格式数据（i2c_c.c 会输出此命令），数据格式见 `c_decoder_utils.h`。但 BITS v2 为二进制格式，解析较复杂。C 实现建议：
- 简单方案：忽略 BITS 相关逻辑，位级 annotation 改为使用当前 ss/es 范围输出
- 精确方案：解析 BITS v2 数据提取 per-bit ss/es 时间戳（参考 `c_decoder_utils.h` 中的解析示例）

### 3. Python 代码中的 print() 调试语句

MPU6050 Python 代码包含 `print()` 调试输出，C 实现中不应包含。

### 4. Python 代码中的 Bug 修正 <!-- Updated: 新增 MLX90614 协议级 bug -->

| 解码器 | Bug | C 修正 |
|--------|-----|--------|
| MLX90614 | data_count==2 时触发计算，但实际只有 2 字节 | 收集 2 字节后立即计算 |
| MLX90614 | 等待 ADDRESS WRITE + DATA WRITE，但 MLX90614 读温度协议使用 ADDRESS READ + DATA READ | 改为等待 ADDRESS READ + DATA READ |
| MPU6050 | `acc_x += b / 1000000` 应为 16 位拼接 | 使用 `(high << 8) \| low` |
| MPU6050 | annotation indices 24-27 定义为 data-out/write-datetime/reg-read/reg-write，但实际只用 index 25 输出所有数据，与 annotation_rows 不匹配 | 拆分为 accel-data/gyro-data/temp-data/reg-write |
| MXC6225XU | 未实现地址过滤 | 添加地址过滤 option |

### 5. CMakeLists.txt 更新

完成所有 5 个 C 解码器后，需在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：
```
mlx90614_c
mpu6050_c
mxc6225xu_c
nunchuk_c
pca9571_c
```

### 6. Python 解码器依赖 <!-- Updated: 新增章节，确认依赖无阻塞 -->

所有 5 个解码器的 `inputs=['i2c']` 依赖 i2c 底层解码器。i2c 已有 C 实现（`i2c_c.c`），因此 **无阻塞依赖**，所有解码器均可正常使用 `recv_proto` 回调接收 I2C 协议数据。
