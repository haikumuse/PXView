# libsigrok 设备兼容性问题

> **Workspace:** c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb

---

**User**

所以说现在这个项目为什么不兼容C:\Users\admin\Downloads\old\libsigrok的设备，本来就是脱胎于pluseview的

**Assistant**

我来帮你分析一下为什么可能不兼容旧版 libsigrok 的设备。让我先检查两个版本的差异。

我来详细说明一下为什么不兼容旧版 libsigrok 的设备。

根本原因是：**PXView 是商业定制版本，删除了标准 sigrok 的大部分设备驱动**。

## 驱动对比

**旧版 libsigrok (0.6.0 - sigrok 官方版本)**：
包含 **70+ 设备驱动**，支持众多厂商：
- Agilent/Keysight DMM
- ASIX Sigma
- ChronoVu LA
- FTDI LA
- FX2LAFW（Generic FX2 based）
- Hantek DSO/4032L/6xxx
- Ikalogic ScanAlogic2/ScanAPlus
- Kingst LA2016
- LeCroy LogicStudio/XStream
- OpenBench Logic Sniffer
- Rigol DS/DG
- Saleae Logic/Logic16/Logic Pro
- Sysclk LWLA/SLA5032
- Zeroplus Logic Cube
- Yokogawa DLM
- Raspberry Pi Pico
- 等等...

**当前 PXView 的 libsigrok**：
只保留 **5 个驱动**（默认）：
- `demo`（虚拟演示设备）
- `DSL/dslogic`（DreamSourceLab DSLogic）
- `DSL/dscope`（DreamSourceLab DSCope）
- `DSL/dsl`（DSL 核心）
- `pxlogic`（PXLogic）

可选驱动（需 `ENABLE_COMPAT_DRIVERS=ON`）：
- `fx2lafw`（Generic FX2）
- `saleae-logic16`
- `raspberrypi-pico`

## 构建选项

从 [CMakeLists.txt:597](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/CMakeLists.txt#L597) 可见：

```cmake
option(ENABLE_COMPAT_DRIVERS "Enable standard sigrok compatible drivers" OFF)
```

默认 **OFF**，即默认只支持 DreamSourceLab/PXLogic 硬件。

## 结论

虽然 PXView 脱胎于 PulseView/sigrok，但它：
1. **裁剪了 95% 的设备驱动**（从 70+ 降到 5 个）
2. **硬件绑定策略**——只支持 DreamSourceLab/PXLogic 的商业产品
3. **不是标准 sigrok 生态的一部分**——无法使用其他厂商的设备

如果你想支持旧版 libsigrok 的设备，需要：
1. 把旧版的驱动代码（`src/hardware/<driver>/`）复制到当前项目的 `libsigrok/hardware/`
2. 在 CMakeLists.txt 中添加对应的编译配置
3. 重新编译

但这会是一个大量的工作，因为驱动代码之间可能有 API 版本差异。

---

**User**

仔细查看api有什么不一样，我之前写了个兼容层看看有没有必要

**Assistant**

我来详细对比两个版本的 API 差异，并检查你之前写的兼容层。

好的，我来总结一下 API 差异和兼容层的必要性。

## API 核心差异对比

### 1. `sr_dev_driver` 结构体差异

| 字段 | 标准 sigrok (0.6.0) | PXView |
|------|---------------------|--------|
| `init` | `init(driver, sr_ctx)` | `init(sr_ctx)` |
| `cleanup` | `cleanup(driver)` | `cleanup(void)` |
| `scan` | `scan(driver, options)` | `scan(options)` |
| **`dev_list`** | ✓ 有 | ✗ 无 |
| **`dev_clear`** | ✓ 有 | ✗ 无 |
| **`dev_mode_list`** | ✗ 无 | ✓ 新增 |
| `config_get` | `(uint32_t key, data, sdi, cg)` | `(int id, data, sdi, ch, cg)` |
| `config_set` | `(uint32_t key, data, sdi, cg)` | `(int id, data, sdi, ch, cg)` |
| `config_list` | `(uint32_t key, data, sdi, cg)` | `(int info_id, data, sdi, cg)` |
| **`dev_destroy`** | ✗ 无 | ✓ 新增 |
| **`dev_status_get`** | ✗ 无 | ✓ 新增 `(sdi, status, prg)` |
| `dev_acquisition_start` | `(sdi)` | `(sdi, cb_data)` |
| `dev_acquisition_stop` | `(sdi)` | `(const sdi, cb_data)` |
| **私有数据字段** | `context` | `priv` |
| **驱动类型字段** | ✗ 无 | `driver_type` |

### 2. 驱动注册机制差异

**标准 sigrok**：使用宏自动注册
```c
static struct sr_dev_driver fx2lafw_driver_info = { ... };
SR_REGISTER_DEV_DRIVER(fx2lafw_driver_info);  // 自动注册到全局列表
```

**PXView**：手动维护数组
```c
static struct sr_dev_driver *drivers_list[] = {
    &demo_driver_info,
    &DSLogic_driver_info,
    &DSCope_driver_info,
    ...
};
```

### 3. `sr_dev_inst` 结构体差异

| 字段 | 标准 sigrok | PXView |
|------|------------|--------|
| 设备类型 | `inst_type` (USB/SERIAL/SCPI) | `mode` (LA/DAQ/OSC) |
| 设备标识 | `vendor/model/version/serial_num/conn_id` | `handle/name/path/dev_type` |

### 4. 公共 API 签名差异

| API | 标准 sigrok | PXView |
|-----|------------|--------|
| `sr_config_get` | `(driver, sdi, cg, uint32_t key, data)` | `(driver, sdi, ch, cg, int id, data)` |
| `sr_config_set` | `(sdi, cg, uint32_t key, data)` | `(sdi, ch, cg, int id, data)` |
| `sr_session_send` | ✓ 有 | `ds_data_forward` |
| 错误码 | 负数枚举 (SR_ERR=-1) | 正数枚举 (SR_ERR=1) |

---

## 兼容层评估

你的兼容层已经覆盖了以下关键差异：

1. ✅ **API 签名适配**：`compat_driver.h` 提供了包装函数模板
2. ✅ **缺失函数实现**：`compat_helpers.c` 实现了 `std_init/std_cleanup/std_dev_list/std_dev_clear`
3. ✅ **GVariant 辅助函数**：`std_gvar_samplerates` 等标准 sigrok 风格的辅助函数
4. ✅ **数据结构适配**：`compat_drv_context` 映射 `context` → `priv`
5. ✅ **默认回调实现**：`compat_dev_mode_list_default`/`compat_dev_status_get_default`

### 兼容层必要性结论

**非常必要！** 因为：

1. **API 分叉深度大**——PXView 的 `sr_dev_driver` 结构体字段数量、签名、语义都完全不同
2. **手动适配成本高**——每个标准驱动需要写大量包装代码，兼容层提供了模板化的解决方案
3. **SCPI/Serial 后端缺失**——标准 sigrok 的 SCPI/Serial 设备驱动依赖完整的通信后端，PXView 需要移植

### 尚需完善的部分

根据 spec.md，以下部分尚未完全实现：

1. **SCPI 后端移植**（`src/scpi/`）—— Rigol、Keysight 等 SCPI 设备需要
2. **Serial 后端移植**（`src/serial.c`、`src/serial_hid.c`）—— Fluke、UNI-T 等串口设备需要
3. **驱动注册自动化**—— 扩展 `hwdriver.c` 支持条件编译引入兼容驱动
4. **ds_* API 降级处理**—— `lib_main.c` 对非 DSL 设备的 `dev_mode_list` 等返回合理默认值

---

## 建议

你的兼容层设计思路正确——**薄包装层 + 辅助函数 + 默认实现**。接下来需要：

1. 移植 `src/scpi/` 和 `src/serial*.c`（参考 spec.md 第 52-61 行）
2. 完善 CMakeLists.txt 的兼容驱动编译选项（参考 spec.md 第 93-104 行）
3. 扩展 `hwdriver.c` 的驱动列表注册机制

---

**User**

补充一下方案的细节

**Assistant**

我来为你补充一份详细的兼容层方案设计文档。

## 兼容层架构总览

```
┌─────────────────────────────────────────────────────────────┐
│                    PXView Application                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────────────┐ │
│  │DeviceAgent│  │SigSession│  │         View Layer           │ │
│  └────┬─────┘  └────┬─────┘  └──────────────────────────────┘ │
│       │             │                                         │
│  ┌────▼─────────────▼────────────────────────────────────────┐ │
│  │                  ds_* Public API (lib_main.c)              │ │
│  │    - ds_scan_device() / ds_get_device() / ds_set_config()  │ │
│  │    - ds_get_actived_device_mode_list() (降级处理)          │ │
│  │    - ds_get_actived_device_status() (降级处理)             │ │
│  └────┬───────────────────────────────────────────────────────┘ │
│       │                                                        │
│  ┌────▼───────────────────────────────────────────────────────┐ │
│  │                  hwdriver.c                                  │ │
│  │    - drivers_list[] 静态数组 (扩展条件编译)                  │ │
│  │    - sr_config_get/set/list() (统一入口)                    │ │
│  └────┬───────────────────────────────────────────────────────┘ │
│       │                                                        │
│  ┌────┴───────────────────────────────────────────────────────┐ │
│  │                  Driver Callback Layer                       │ │
│  │  ┌─────────────────────────────────────────────────────────┐│ │
│  │  │   Native Drivers (DSL/pxlogic/demo)                     ││ │
│  │  │   - 直接调用，无需适配                                  ││ │
│  │  └─────────────────────────────────────────────────────────┘│ │
│  │  ┌─────────────────────────────────────────────────────────┐│ │
│  │  │   Compat Drivers (fx2lafw/saleae/raspberrypi-pico)      ││ │
│  │  │   ┌──────────────┐  ┌───────────────────────────────┐  ││ │
│  │  │  │ Wrapper func  │──►│ Standard sigrok driver code   │  ││ │
│  │  │  │ (signature    │   │ (unmodified or min-modified)  │  ││ │
│  │  │  │  adaptation)  │   └───────────────────────────────┘  ││ │
│  │  │  └──────────────┘                                       ││ │
│  │  └─────────────────────────────────────────────────────────┘│ │
│  └─────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │              compat/ 适配层基础设施                          │ │
│  │  ┌─────────────────────────────────────────────────────────┐│ │
│  │  │ compat.h          │ 统一入口头文件                       ││ │
│  │  │ compat_driver.h   │ 适配宏 + wrapper 函数模板            ││ │
│  │  │ compat_config.h   │ 配置键常量 + 触发匹配类型            ││ │
│  │  │ compat_helpers.c/h│ std_init/std_cleanup/STD_CONFIG_LIST ││ │
│  │  │ compat_serial.c/h │ 串口通信后端                         ││ │
│  │  └─────────────────────────────────────────────────────────┘│ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## 详细数据结构映射

### `sr_dev_driver` 结构体映射

| 标准 sigrok 字段 | PXView 字段 | 映射方式 | 说明 |
|---|---|---|---|
| `.name` | `.name` | 直接映射 | 字符串 |
| `.longname` | `.longname` | 直接映射 | 字符串 |
| `.api_version` | `.api_version` | 直接映射 | 整数 |
| **`.init(driver, sr_ctx)`** | **`.init(sr_ctx)`** | **Wrapper** | 无 driver 参数，用静态指针或闭包 |
| **`.cleanup(driver)`** | **`.cleanup(void)`** | **Wrapper** | 无参数，用静态指针 |
| **`.scan(driver, options)`** | **`.scan(options)`** | **Wrapper** | 无 driver 参数 |
| `.dev_list` | 不存在 | 由 `std_dev_list` 在 compat_drv_context 中模拟 | 通过 compat 辅助函数访问 |
| `.dev_clear` | 不存在 | 由 `std_dev_clear` 在 compat_drv_context 中模拟 | 同上 |
| 不存在 | `.dev_mode_list(sdi)` | **Default stub** | `compat_dev_mode_list_default` 返回 NULL |
| 不存在 | `.dev_destroy(sdi)` | **Default stub** | `compat_dev_destroy_default` 返回 SR_OK |
| 不存在 | `.dev_status_get(sdi, status, prg)` | **Default stub** | `compat_dev_status_get_default` 返回 SR_ERR_NA |
| **`.config_get(key, data, sdi, cg)`** | **`.config_get(id, data, sdi, ch, cg)`** | **Wrapper** | ch=NULL, uint32_t→int |
| **`.config_set(key, data, sdi, cg)`** | **`.config_set(id, data, sdi, ch, cg)`** | **Wrapper** | ch=NULL, uint32_t→int |
| **`.config_list(key, data, sdi, cg)`** | **`.config_list(info_id, data, sdi, cg)`** | **Wrapper** | uint32_t→int |
| `.dev_open(sdi)` | `.dev_open(sdi)` | 直接映射 | 签名相同 |
| `.dev_close(sdi)` | `.dev_close(sdi)` | 直接映射 | 签名相同 |
| `.dev_acquisition_start(sdi)` | `.dev_acquisition_start(sdi, cb_data)` | **Wrapper** | 忽略 cb_data |
| `.dev_acquisition_stop(sdi)` | `.dev_acquisition_stop(const sdi, cb_data)` | **Wrapper** | 忽略 cb_data, cast away const |
| **`.context` (private)** | **`.priv` (private)** | **重命名** | 宏 `#define DRIVER_CONTEXT(drv) ((drv)->priv)` |
| 不存在 | `.driver_type` | **新增** | 必须设为 `DRIVER_TYPE_HARDWARE` |

### `sr_dev_inst` 结构体映射

| 标准 sigrok 字段 | PXView 字段 | 映射方式 |
|---|---|---|
| `vendor` | `vendor` | 直接映射 ✓ |
| `model` | `name` + 新增 `model` 字段 | 双写: 设置 `name` 和 `model` |
| `version` | `version` | 直接映射 ✓ |
| `serial_num` | 新增 `serial_num` | 兼容层扩展字段 |
| `conn_id` | `path` + 新增 `connection_id` | 路径字符串 |
| `inst_type` | 新增 `inst_type` | 兼容层扩展字段 |
| `index` | `index` | 直接映射 ✓ |
| `status` | `status` | 直接映射 ✓ |
| `channels` | `channels` | 直接映射 ✓ |
| `channel_groups` | 新增 `channel_groups` | 兼容层扩展字段 |
| `conn` | `conn` | 直接映射 ✓ |
| `priv` | `priv` | 直接映射 ✓ |
| `driver` | `driver` | 直接映射 ✓ |
| `session` | 新增 `session` | 兼容层扩展字段 |

## 具体实现模式

### 模式 A: Wrapper 函数（用于 `sr_dev_driver` 结构体）

以 `fx2lafw` 为例，标准 sigrok 驱动定义：
```c
static int config_get(uint32_t key, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
```

PXView 适配包装：
```c
// 静态指针指向最终的 driver_info（用于 wrapper 内部访问 driver 上下文）
static struct sr_dev_driver *fx2lafw_drv_ptr;

static int fx2lafw_compat_config_get(int id, GVariant **data,
    const struct sr_dev_inst *sdi, const struct sr_channel *ch,
    const struct sr_channel_group *cg)
{
    (void)ch;  // 标准 sigrok 不支持 per-channel config
    return config_get((uint32_t)id, data, sdi, cg);
}

struct sr_dev_driver fx2lafw_driver_info = {
    .name = "fx2lafw",
    .driver_type = DRIVER_TYPE_HARDWARE,  // PXView 新增
    .init = fx2lafw_compat_init,           // wrapper
    .cleanup = fx2lafw_compat_cleanup,     // wrapper
    .scan = fx2lafw_compat_scan,           // wrapper
    .dev_mode_list = compat_dev_mode_list_default,  // default stub
    .config_get = fx2lafw_compat_config_get,        // wrapper
    .config_set = fx2lafw_compat_config_set,        // wrapper
    .config_list = fx2lafw_compat_config_list,      // wrapper
    .dev_open = dev_open,                  // 直接复用
    .dev_close = dev_close,                // 直接复用
    .dev_destroy = compat_dev_destroy_default,      // default stub
    .dev_status_get = compat_dev_status_get_default, // default stub
    .dev_acquisition_start = fx2lafw_compat_acquisition_start, // wrapper
    .dev_acquisition_stop = fx2lafw_compat_acquisition_stop,   // wrapper
};
```

### 模式 B: 宏自动生成 Wrapper（`compat_driver.h` 已提供模板）

每个新驱动只需填写 3 个地方：
1. 驱动源码的 `api.c` 末尾，用 wrapper 函数包裹原有回调
2. 用 `struct sr_dev_driver xxx_driver_info = { ... }` 声明 PXView 结构体
3. `hwdriver.c` 中添加 `extern` 和 `drivers_list[]` 条目

### 模式 C: 内部 API 宏重定向（`compat.h`）

标准 sigrok 驱动内部调用：
```c
sr_config_set(sdi, cg, SR_CONF_SAMPLERATE, gvar);
```

通过 `compat.h` 宏自动转接：
```c
// compat.h 中无直接宏，驱动应显式调用 _compat 版本
// 或者修改驱动源码：
sr_config_set_compat(sdi, cg, SR_CONF_SAMPLERATE, gvar);
// 内部实现：
// return sr_config_set((struct sr_dev_inst*)sdi, NULL, (struct sr_channel_group*)cg, (int)key, data);
```

## CMakeLists.txt 集成细节

```cmake
# 顶部选项定义
option(ENABLE_COMPAT_DRIVERS "Enable standard sigrok compatible drivers" OFF)
option(ENABLE_DRIVER_FX2LAFW "Enable fx2lafw driver" OFF)
option(ENABLE_DRIVER_SALEAE_LOGIC16 "Enable saleae-logic16 driver" OFF)
option(ENABLE_DRIVER_RASPBERRYPI_PICO "Enable raspberrypi-pico driver" OFF)

# 条件编译宏
if(ENABLE_COMPAT_DRIVERS)
    add_definitions(-DHAVE_COMPAT_DRIVERS)
    
    # 串口后端（libserialport）
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSERIALPORT libserialport)
    if(LIBSERIALPORT_FOUND)
        add_definitions(-DHAVE_LIBSERIALPORT)
        include_directories(${LIBSERIALPORT_INCLUDE_DIRS})
    endif()
    
    # 兼容层源文件
    list(APPEND libsigrok_SOURCES
        libsigrok/hardware/compat/compat_helpers.c
        libsigrok/hardware/compat/compat_serial.c
    )
    
    if(ENABLE_DRIVER_FX2LAFW)
        add_definitions(-DHAVE_DRIVER_FX2LAFW)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/fx2lafw/api.c
            libsigrok/hardware/fx2lafw/protocol.c
        )
    endif()
    
    if(ENABLE_DRIVER_SALEAE_LOGIC16)
        add_definitions(-DHAVE_DRIVER_SALEAE_LOGIC16)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/saleae-logic16/api.c
            libsigrok/hardware/saleae-logic16/protocol.c
        )
    endif()
    
    if(ENABLE_DRIVER_RASPBERRYPI_PICO)
        add_definitions(-DHAVE_DRIVER_RASPBERRYPI_PICO)
        list(APPEND libsigrok_SOURCES
            libsigrok/hardware/raspberrypi-pico/api.c
            libsigrok/hardware/raspberrypi-pico/protocol.c
        )
    endif()
endif()
```

## 新增驱动接入 Checklist

对于任何一个标准 sigrok 驱动（以 `xxx` 为例）：

### 步骤 1: 复制源码
```bash
cp -r old/libsigrok/src/hardware/xxx/ libsigrok/hardware/xxx/
```

### 步骤 2: 修改 `api.c` 头部
```c
// 替换原有 include
#include "compat.h"  // 包含 libsigrok-internal.h + 所有兼容定义
// 删除 <config.h> 和 <libsigrok/libsigrok.h> 的显式 include
```

### 步骤 3: 重命名 driver_info
```c
// 旧：
// static struct sr_dev_driver xxx_driver_info = { ... };
// SR_REGISTER_DEV_DRIVER(xxx_driver_info);

// 新：去除 static，去掉 SR_REGISTER_DEV_DRIVER
struct sr_dev_driver xxx_driver_info = { ... };
```

### 步骤 4: 添加 wrapper 函数（在原有回调下方）
```c
static struct sr_dev_driver *xxx_drv_ptr;

static int xxx_compat_init(struct sr_context *sr_ctx) {
    xxx_drv_ptr = &xxx_driver_info;
    return std_init(xxx_drv_ptr, sr_ctx);
}

static int xxx_compat_cleanup(void) {
    return std_cleanup(xxx_drv_ptr);
}

static GSList *xxx_compat_scan(GSList *options) {
    return scan(xxx_drv_ptr, options);
}

// ... config_get/config_set/config_list/acquisition_start/acquisition_stop
```

### 步骤 5: 修改 `hwdriver.c`
```c
#ifdef HAVE_DRIVER_XXX
extern SR_PRIV struct sr_dev_driver xxx_driver_info;
#endif

static struct sr_dev_driver *drivers_list[] = {
    // ...
#ifdef HAVE_DRIVER_XXX
    &xxx_driver_info,
#endif
    NULL,
};
```

### 步骤 6: 修改 CMakeLists.txt
```cmake
option(ENABLE_DRIVER_XXX "Enable xxx driver" OFF)
# 在 ENABLE_COMPAT_DRIVERS 块中：
if(ENABLE_DRIVER_XXX)
    add_definitions(-DHAVE_DRIVER_XXX)
    list(APPEND libsigrok_SOURCES
        libsigrok/hardware/xxx/api.c
        libsigrok/hardware/xxx/protocol.c
    )
endif()
```

## ds_* API 降级处理清单

| API 函数 | 降级逻辑 | 实现位置 |
|---|---|---|
| `ds_get_actived_device_mode_list()` | 无 `dev_mode_list` → 返回单元素列表 [sdi->mode] | `lib_main.c` |
| `ds_get_actived_device_status()` | 无 `dev_status_get` → memset(status, 0) + 返回 SR_OK | `lib_main.c` |
| `ds_get_actived_device_info()` | 兼容设备 → `model` 映射到 `name` | `lib_main.c` |
| `ds_open_device()` | 兼容设备 → 跳过 DSL 专有初始化 | `lib_main.c` |
| `ds_start_collect()` | 兼容设备 → 跳过 `dev_status_get` 预检 | `lib_main.c` |

## DeviceAgent 适配点

```cpp
// deviceagent.cpp 中需要区分的逻辑：

bool DeviceAgent::is_compat_device() {
    // 判断方式：驱动不是 DSL 系列
    return (_dev_driver && 
            _dev_driver != &DSLogic_driver_info &&
            _dev_driver != &DSCope_driver_info &&
            _dev_driver != &demo_driver_info);
}

// 零点校准：仅 DSL 设备
if (!is_compat_device()) {
    ds_set_actived_device_config(SR_CONF_ZERO_CALIBRATION, ...);
}

// VGA 增益：仅 DSL 设备
if (!is_compat_device()) {
    ds_set_actived_device_config(SR_CONF_VGA_GAIN, ...);
}

// PWM 输出：仅 DSL 设备
if (!is_compat_device()) {
    // ...
}
```

## 测试验证矩阵

| 测试项 | 验证内容 | 通过标准 |
|---|---|---|
| 编译测试 | `ENABLE_COMPAT_DRIVERS=ON` 编译通过 | 0 errors, 0 warnings |
| 原有功能 | DSL 驱动不受影响 | 正常扫描/采集/配置 |
| fx2lafw 扫描 | 连接 FX2 设备 | 出现在设备列表 |
| fx2lafw 采集 | 选择设备并采集 | 波形正确显示 |
| saleae-logic16 扫描 | 连接设备 | 出现在设备列表 |
| saleae-logic16 采集 | 选择设备并采集 | 波形正确显示 |
| raspberrypi-pico 扫描 | 连接设备 | 出现在设备列表 |
| 配置读写 | 兼容设备采样率/深度/通道设置 | UI 正常响应 |
| Tab 切换 | 兼容设备状态保持 | 不丢失配置 |

这份补充方案够详细吗？需要我针对某个具体部分再展开吗？

---

**User**

/spec 列出计划迁移所有设备

**Assistant**

我来检查现有的 spec 文档，并制定迁移所有设备驱动的计划。

我看到已有 `add-sigrok-driver-compat-layer` spec，它完成了基础设施和首批 3 个驱动的迁移。现在需要创建一个新 spec 来规划剩余所有驱动的迁移。

现在我来创建一个新 spec，规划剩余所有驱动的迁移计划。

已创建完整的驱动迁移规划文档：

## 规划概览

**总计**：84 个待迁移驱动，分为 5 个批次
- Batch 1: 12 个逻辑分析仪驱动（高优先级）
- Batch 2: 14 个示波器驱动（依赖 SCPI 后端）
- Batch 3: 20 个万用表驱动（依赖 Serial/USB-HID 后端）
- Batch 4: 12 个电源/负载驱动（依赖 SCPI/Serial 后端）
- Batch 5: 15 个其他设备驱动（依赖多种后端）

**工作量估算**：208 小时（单人 5 周）

**前置基础设施**：
1. SCPI 通信后端移植（16 小时）
2. 驱动迁移自动化工具开发（8 小时）
3. 驱动测试框架开发（12 小时）

**关键依赖**：
- 示波器驱动需要 SCPI 后端（从标准 sigrok 移植 `src/scpi/`）
- 万用表驱动部分需要 USB-HID/Bluetooth 后端
- Linux-only 驱动需要在 Linux 环境验证

**自动化策略**：
- 开发 `scripts/migrate_driver.py` 自动完成 80% 的迁移工作
- 批次内驱动可并行迁移
- 使用 driver_test 工具验证编译和基本功能

文档已保存在：
- [spec.md](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/.trae/specs/migrate-all-sigrok-drivers/spec.md)
- [tasks.md](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/.trae/specs/migrate-all-sigrok-drivers/tasks.md)
- [checklist.md](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/.trae/specs/migrate-all-sigrok-drivers/checklist.md)

---

