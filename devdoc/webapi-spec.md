# DSView Web API 规格书（UI 驱动）

## 设计原则

**从 UI 出发，而非从数据结构出发。**

旧方案按底层 Snapshot 类型（Logic/Dso/Analog）组织 API，导致：
- 前端必须知道当前是哪种 Snapshot 类型才能选择正确的端点
- 同一个视口请求被拆成多个 API 调用
- API 命名暴露内部实现（edges/envelope/raw），而非用户意图

新方案按 UI 面板/交互组织 API：
- **一个视口 = 一个请求**，服务端根据当前信号类型自动选择最优数据提取策略
- API 命名反映用户操作（缩放、平移、添加光标），而非内部数据结构
- 前端只需描述"我想看什么"，无需关心"数据怎么取"

---

## 〇、API 分类：Web 能做 vs Web 做不了

**核心判断标准**：这个功能是否可以在浏览器中用 JavaScript 独立完成，还是必须依赖服务端能力（硬件访问、文件系统、内存数据、计算引擎）？

### B 类 — Web 做不到（必须服务端实现，优先开发）

这些 API 涉及硬件控制、文件系统访问、GB 级内存数据读取、C 库调用等浏览器无法完成的能力，**必须优先实现**。

| API | 端点 | Web 做不了的原因 |
|-----|------|-----------------|
| **波形数据提取** | `GET /viewport/waveform` | 需读取 GB 级 Snapshot 内存，浏览器无法访问 |
| **深度缩放原始数据** | `GET /viewport/raw` | 同上，需逐采样读取 |
| **采集控制** | `POST /capture/start` `POST /capture/stop` | 需控制 USB 硬件启动/停止采集 |
| **采样参数** | `GET/PUT /capture/parameters` | 需配置硬件采样率/采样深度 |
| **设备列表/切换** | `GET /capture/devices` `PUT /capture/device` | 需枚举和切换 USB 设备 |
| **设备信息** | `GET /device/info` | 需读取硬件能力 |
| **设备模式切换** | `PUT /device/mode` | 需切换硬件工作模式（Logic/DSO/Analog） |
| **通道配置** | `GET/PUT /device/channels` | 需配置硬件通道使能/探头参数 |
| **设备高级选项** | `GET/PUT /device/options` | 需配置硬件（阈值/PWM/滤波/RLE/流模式等） |
| **校准** | `POST /device/calibrate/*` | 需控制硬件执行校准流程 |
| **触发配置** | `GET/PUT /trigger` | 需配置硬件触发条件（逻辑触发/DSO 触发） |
| **解码器管理** | `POST/PUT/DELETE /decoders` | 需调用 C 语言协议解码库 |
| **解码注解数据** | `GET /decoders/{id}/annotations` | 需从解码引擎读取结果 |
| **解码数据搜索** | `GET /decoders/{id}/search` | 需在服务端注解数据中搜索 |
| **解码数据导出** | `POST /decoders/{id}/export` | 需服务端生成 CSV/TXT 文件 |
| **模式搜索执行** | `PUT /search` | 需在 GB 级 Snapshot 中搜索边沿模式匹配 |
| **FFT 频谱数据** | `GET /spectrum/data` | 需从原始采样数据计算 FFT |
| **数学运算数据** | `GET /math`（数据部分） | 需从原始采样数据计算 CH1±CH2 |
| **李萨如图形数据** | `GET /math/lissajous/data` | 需从原始采样数据提取 XY 点对 |
| **DSO 测量值** | `GET /measurements`（dso_measure_slots） | 需从原始采样数据计算 Vpp/Vrms/频率等 |
| **状态查询** | `GET /status` | 需读取采集会话状态 |
| **文件打开** | `POST /file/open` | 需读取本地 .dsl ZIP 文件并解析 |
| **文件保存** | `POST /file/save` | 需将数据写入本地 .dsl ZIP 文件 |
| **数据导出** | `POST /file/export` | 需将采样数据导出为 CSV/VCD |
| **会话加载/保存** | `GET/PUT /file/session` | 需读写本地 .dsc JSON 文件 |
| **WebSocket 流式推送** | `ws://.../api/v1/ws` | 需从 Snapshot 增量提取数据并推送 |

### A 类 — Web 可以做到（前端独立实现，延后或无需服务端 API）

这些功能浏览器完全可以自己完成——用 JavaScript 管理状态、Canvas 渲染、本地计算。服务端不需要为它们提供专门的 API，或者只需在最终持久化时同步一次。

| API | 端点 | Web 能自己做的理由 | 服务端是否需要最终同步 |
|-----|------|-------------------|---------------------|
| **光标管理** | ~~`/cursors`~~ | 浏览器自己追踪光标位置、渲染、计算时间差，只需波形数据即可 | 保存会话时同步光标位置 |
| **视口缩放/平移** | ~~`/viewport/state`（PUT）~~ | 浏览器自己管理 scale/offset，滚轮/拖拽交互纯前端 | 无需同步，视口是临时状态 |
| **信号重命名** | ~~`PUT /signals/{id}` name~~ | 浏览器自己维护名称编辑状态 | 保存会话时同步 |
| **信号改色** | ~~`PUT /signals/{id}` color~~ | 浏览器自己维护颜色选择 | 保存会话时同步 |
| **信号排序** | ~~`PUT /signals/{id}` view_index~~ | 浏览器自己管理拖拽排序 | 保存会话时同步 |
| **解码格式选择** | ~~`PUT /decoders/{id}/format`~~ | hex/dec/bin/ascii 纯前端显示转换 | 无需同步 |
| **搜索导航** | ~~`POST /search/next`~~ | 在已返回的搜索结果中翻页，纯前端 | 无需同步 |
| **距离测量/边沿测量** | ~~`/measurements/distance`~~ | 从已获取的边缘数据本地计算 | 无需同步 |
| **鼠标悬浮测量** | ~~`/measurements` mouse_measure~~ | 从已获取的边缘/包络数据本地计算脉宽/周期/频率 | 无需同步 |
| **FFT 视口缩放** | ~~`/spectrum` 缩放部分~~ | 浏览器自己管理频率轴缩放/平移 | 无需同步 |
| **主题切换** | 无 API | 纯 CSS 切换亮色/暗色主题 | 无需同步 |
| **语言切换** | 无 API | 前端 i18n | 无需同步 |
| **截图** | ~~`GET /file/capture`~~ | 浏览器可用 canvas.toDataURL() 或 html2canvas | 无需同步 |

### 分类结论

```
B 类 API 数量: 26 个端点    → 优先实现，Web 没有这些就完全无法工作
A 类 API 数量: 13 个端点    → 延后实现，前端可自行处理

实现策略:
  1. B 类 API 全部实现后，Web 前端即可完整工作
  2. A 类功能由前端 JavaScript 独立实现
  3. A 类中需要持久化的数据（光标位置/信号名称/颜色/排序），
     在"保存会话"时随 B 类的 /file/session 一起提交即可，
     不需要单独的 PUT 端点
```

---

## 一、UI 面板与 API 映射总览

| UI 面板 | API 前缀 | 核心操作 |
|---------|---------|---------|
| SamplingBar（采样控制栏） | `/api/v1/capture` | 设备选择、采样参数、开始/停止采集 |
| DeviceOptions（设备选项对话框） | `/api/v1/device` | 通道启用、探头配置、校准 |
| Viewport（波形视口） | `/api/v1/viewport` | 波形数据获取、缩放/平移 |
| Header（信号标签面板） | `/api/v1/signals` | 信号属性（名称/颜色/排序/触发） |
| Ruler（时间标尺） | `/api/v1/viewport/ruler` | 时间刻度信息 |
| Cursor（光标） | `/api/v1/cursors` | 添加/删除/移动光标、光标测量 |
| TriggerDock（触发面板） | `/api/v1/trigger` | 触发条件设置 |
| ProtocolDock（协议面板） | `/api/v1/decoders` | 解码器管理、注解数据、导出 |
| MeasureDock（测量面板） | `/api/v1/measurements` | 测量项管理、测量结果 |
| SearchDock（搜索面板） | `/api/v1/search` | 模式搜索、搜索导航 |
| FFT Viewport（频谱视图） | `/api/v1/spectrum` | FFT 数据、频谱选项 |
| Math/Lissajous（数学/李萨如） | `/api/v1/math` | 数学运算、李萨如图形 |
| ViewStatus（状态栏） | `/api/v1/status` | 采集状态、触发时间、DSO 测量值 |
| FileBar（文件操作栏） | `/api/v1/file` | 打开/保存/导出 |

---

## 二、传输协议架构

### 2.1 双通道协议

```
浏览器
  ├─ HTTP/REST  ← 查询/控制类请求（对应 UI 面板的读取/操作）
  └─ WebSocket  ← 实时推送（对应 Viewport 实时刷新、状态栏更新）
```

| 场景 | 协议 | 对应 UI 行为 |
|------|------|-------------|
| 读取视口波形 | HTTP GET | 用户打开页面/缩放/平移 |
| 读取信号列表 | HTTP GET | Header 面板渲染 |
| 读取光标列表 | HTTP GET | Ruler/Viewport 光标渲染 |
| 启动/停止采集 | HTTP POST | SamplingBar 按钮点击 |
| 设置触发 | HTTP PUT | TriggerDock 参数修改 |
| 添加/删除解码器 | HTTP POST/DELETE | ProtocolDock 按钮操作 |
| **流式波形推送** | **WebSocket** | Viewport 实时刷新（LOOP 模式） |
| **采集状态变更** | **WebSocket** | SamplingBar/ViewStatus 状态更新 |
| **解码进度** | **WebSocket** | ProtocolDock 进度条更新 |
| **测量值更新** | **WebSocket** | MeasureDock/ViewStatus 数值更新 |

### 2.2 数据编码

| 通道 | 编码格式 | 理由 |
|------|---------|------|
| HTTP REST | JSON | 可读性好，调试方便 |
| WebSocket 控制消息 | JSON | 结构化，易解析 |
| WebSocket 波形数据 | 二进制帧 | 高效，避免序列化开销 |

---

## 三、SamplingBar — 采集控制 API

对应 UI：顶部采样控制栏，包含设备选择、采样率/采样深度、运行/停止/单次按钮、采集模式。

### 3.1 设备列表

```
GET /api/v1/capture/devices
→ {
    "current": "pxlogic-16pro-0",
    "devices": [
      {
        "handle": "pxlogic-16pro-0",
        "name": "PX-Logic U3 16 Pro",
        "type": "usb3.0",          // "demo" | "file" | "usb2.0" | "usb3.0"
        "driver": "pxlogic",
        "modes": ["logic", "dso", "analog"]
      }
    ]
  }
```

### 3.2 切换设备

```
PUT /api/v1/capture/device
Body: {"handle": "pxlogic-16pro-0"}
→ {"status": "ok"}
```

> 对应 SamplingBar 中设备选择下拉框的切换操作。

### 3.3 采样参数

```
GET /api/v1/capture/parameters
→ {
    "mode": "logic",                // "logic" | "dso" | "analog"
    "collect_mode": "single",       // "single" | "repeat" | "loop"
    "samplerate": 100000000,
    "sample_count": 10000000,
    "timebase": null,               // DSO 模式下有效，单位秒/格
    "duration": 0.1,                // 采集时长（秒）
    "is_stream": false,
    "is_instant": false,
    "repeat_interval": 0,           // 重复模式间隔（秒）

    "available_samplerates": [      // 可选采样率列表
      {"value": 10000, "text": "10 kHz"},
      {"value": 100000000, "text": "100 MHz"}
    ],
    "available_durations": [        // 可选采集时长列表
      {"value": 0.001, "text": "1 ms"},
      {"value": 0.1, "text": "100 ms", "rle": true}
    ],
    "available_timebases": null,    // DSO 模式下有效
    "available_collect_modes": ["single", "repeat", "loop"]
  }
```

```
PUT /api/v1/capture/parameters
Body: {
  "samplerate": 100000000,
  "duration": 0.1,                 // 非 DSO 模式用 duration
  "timebase": null,                // DSO 模式用 timebase
  "collect_mode": "single",
  "repeat_interval": 0
}
→ {"status": "ok"}
```

> 对应 SamplingBar 中采样率下拉框、采样数量下拉框、模式按钮的操作。

### 3.4 采集控制

```
POST /api/v1/capture/start
Body: {"instant": false}           // true=单次采集, false=正常采集
→ {"status": "ok"}
```

```
POST /api/v1/capture/stop
→ {"status": "ok"}
```

> 对应 SamplingBar 中 Start/Stop/Instant 按钮的点击。

---

## 四、DeviceOptions — 设备选项 API

对应 UI：设备选项对话框，包含通道启用/禁用、探头参数、操作模式、校准。

### 4.1 设备信息

```
GET /api/v1/device/info
→ {
    "name": "PX-Logic U3 16 Pro",
    "driver": "pxlogic",
    "mode": "logic",
    "type": "usb3.0",
    "is_hardware": true,
    "is_demo": false,
    "is_file": false
  }
```

### 4.2 设备模式切换

```
PUT /api/v1/device/mode
Body: {"mode": "dso"}             // "logic" | "dso" | "analog"
→ {"status": "ok"}
```

> 对应 DevMode 下拉菜单的模式切换。

### 4.3 通道配置

```
GET /api/v1/device/channels
→ {
    "channel_mode": "8ch",         // 当前通道模式
    "available_modes": [           // 可选通道模式
      {"id": 0, "name": "4ch"},
      {"id": 1, "name": "8ch"},
      {"id": 2, "name": "16ch"}
    ],
    "channels": [
      {
        "index": 0,
        "type": "logic",           // "logic" | "dso" | "analog"
        "enabled": true,
        "name": "CH0",

        // DSO/Analog 通道额外字段
        "vdiv": 1000,              // 伏特/格 (mV)
        "vfactor": 1,              // 探头衰减系数
        "coupling": "dc",          // "dc" | "ac" | "gnd"
        "map_unit": "V",           // 映射单位 (Analog)
        "map_min": -5.0,           // 映射最小值 (Analog)
        "map_max": 5.0,            // 映射最大值 (Analog)
        "map_default": true        // 使用默认映射 (Analog)
      }
    ]
  }
```

```
PUT /api/v1/device/channels
Body: {
  "channel_mode": "8ch",
  "channels": [
    {"index": 0, "enabled": true},
    {"index": 1, "enabled": false},
    {"index": 2, "enabled": true, "vdiv": 500, "coupling": "ac", "vfactor": 10}
  ]
}
→ {"status": "ok"}
```

> 对应 DeviceOptions 对话框中的通道勾选、探头参数设置。

### 4.4 设备高级选项

```
GET /api/v1/device/options
→ {
    "operation_mode": {"value": 0, "options": [
      {"id": 0, "name": "Buffer"},
      {"id": 1, "name": "Stream"},
      {"id": 2, "name": "Internal Test"},
      {"id": 3, "name": "External Test"},
      {"id": 4, "name": "Low Power Test"}
    ]},
    "buffer_options": {"value": 0, "options": [...]},
    "threshold": {"value": 0, "options": [...]},
    "vth": {"value": 1.5, "min": 0.0, "max": 6.0, "step": 0.1, "unit": "V"},
    "filter": {"value": 0, "options": [...]},
    "rle": {"value": false},
    "stream": {"value": false},
    "clock_type": {"value": false},
    "clock_edge": {"value": false},
    "trigger_out": {"value": false},
    "pwm0_en": {"value": false},
    "pwm0_freq": {"value": 0, "min": 0, "max": 1000000, "unit": "Hz"},
    "pwm0_duty": {"value": 0, "min": 0, "max": 100, "unit": "%"},
    "pwm1_en": {"value": false},
    "pwm1_freq": {"value": 0, "min": 0, "max": 1000000, "unit": "Hz"},
    "pwm1_duty": {"value": 0, "min": 0, "max": 100, "unit": "%"},
    "stream_buff": {"value": 1, "min": 1, "max": 128, "unit": "GB"},
    "bandwidth_limit": {"value": 0, "options": [...]}
  }
```

```
PUT /api/v1/device/options
Body: {
  "operation_mode": 1,
  "vth": 1.5,
  "rle": true,
  "stream": true
}
→ {"status": "ok"}
```

> 对应 DeviceOptions 对话框中 Mode GroupBox 的各项配置。

### 4.5 校准

```
POST /api/v1/device/calibrate/auto
→ {"status": "ok"}

POST /api/v1/device/calibrate/manual
→ {"status": "ok"}
```

> 对应 DeviceOptions 对话框中的 Auto/Manual Calibration 按钮。

---

## 五、Viewport — 波形视口 API

对应 UI：中央波形显示区域。这是最核心的 API——前端只需告诉服务端"视口范围"，服务端自动返回适合渲染的数据。

### 5.1 核心设计：视口驱动

```
前端描述: "我要看从第 A 个采样到第 B 个采样，视口宽度 W 像素"
服务端自动: 根据每个信号类型选择最优数据提取策略
  - 逻辑信号 → 边缘序列 (get_display_edges)
  - DSO 信号 → 包络数据 (get_envelope_section)
  - 模拟信号 → 包络数据 (get_envelope_section)
  - 解码轨迹 → 注解数据 (get_annotation_subset)
```

### 5.2 获取视口数据

```
GET /api/v1/viewport/waveform?start=0&end=10000000&width=1920
  参数:
    start:  视口起始样本索引
    end:    视口结束样本索引
    width:  视口像素宽度（用于决定数据精度）
→ {
    "samplerate": 100000000,
    "start_sample": 0,
    "end_sample": 10000000,
    "trigger_pos": 5000000,
    "signals": [
      {
        "index": 0,
        "type": "logic",
        "name": "CH0",
        "color": "#ff0000",
        "data": {
          "first_value": 1,
          "edges": [
            {"sample": 0, "value": 1},
            {"sample": 150, "value": 0},
            {"sample": 320, "value": 1}
          ]
        }
      },
      {
        "index": 1,
        "type": "dso",
        "name": "CH1",
        "color": "#00ff00",
        "data": {
          "start_sample": 0,
          "scale": 256,
          "envelope": [
            {"min": 45, "max": 52},
            {"min": 48, "max": 55}
          ]
        }
      },
      {
        "index": 2,
        "type": "analog",
        "name": "CH2",
        "color": "#0000ff",
        "data": {
          "start_sample": 0,
          "scale": 16,
          "envelope": [
            {"min": 100, "max": 120},
            {"min": 105, "max": 125}
          ]
        }
      }
    ],
    "decoders": [
      {
        "stack_id": 0,
        "label": "SPI",
        "rows": [
          {
            "index": 0,
            "title": "MOSI",
            "annotations": [
              {"start": 1000, "end": 1010, "texts": ["0x1A"]},
              {"start": 1010, "end": 1020, "texts": ["0x2B"]}
            ]
          }
        ]
      }
    ]
  }
```

**关键优势**：
- 一次请求获取视口内所有信号数据，前端无需分别调用 logic/dso/analog 端点
- 服务端根据信号类型自动选择最优提取策略，前端无需知道内部实现
- 解码注解也包含在视口数据中，与波形数据同步

### 5.3 视口状态

```
GET /api/v1/viewport/state
→ {
    "scale": 5e-9,                 // 秒/像素
    "offset": 0,                   // 像素偏移
    "max_scale": 1e-3,
    "min_scale": 1e-12,
    "total_samples": 10000000,
    "total_time": 0.1,             // 总采集时长（秒）
    "trigger_pos": 5000000,
    "trigger_time": "2024-01-15T10:30:00",
    "pixel_per_sample": 0.192,     // 当前缩放下的像素/采样比
    "sample_per_pixel": 5208       // 当前缩放下的采样/像素比
  }
```

```
PUT /api/v1/viewport/state
Body: {
  "scale": 5e-9,                   // 缩放（秒/像素）
  "offset": 100                    // 平移（像素偏移）
}
→ {"status": "ok"}
```

> 对应 Viewport 中的鼠标滚轮缩放、左键拖拽平移操作。

### 5.4 深度缩放原始数据

当用户深度缩放（每像素 < 1 个采样点）时，需要逐采样数据：

```
GET /api/v1/viewport/raw?signal=0&start=0&count=1000
→ {
    "signal": 0,
    "start_sample": 0,
    "samplerate": 100000000,
    "type": "logic",               // "logic" | "dso" | "analog"
    "data": [0, 1, 1, 0, 1, ...]  // logic: 0/1; dso/analog: uint8 值
  }
```

> 对应深度缩放时 Viewport 逐像素渲染的需求。count 上限 10000。

---

## 六、Header — 信号标签 API

对应 UI：左侧信号标签面板，包含信号名称、颜色、排序、触发按钮、DSO 旋钮。

### 6.1 信号列表

```
GET /api/v1/signals
→ {
    "signals": [
      {
        "index": 0,
        "type": "logic",
        "name": "CH0",
        "color": "#ff0000",
        "enabled": true,
        "view_index": 0,           // 显示顺序

        // Logic 信号
        "trigger": "rising",       // "none" | "rising" | "falling" | "high" | "low" | "edge"

        // DSO 信号
        "vdiv": 1000,              // mV/div
        "vfactor": 1,              // 探头衰减系数
        "coupling": "dc",          // "dc" | "ac" | "gnd"
        "zero_pos": 0.5,           // 零点位置比例 (0~1)
        "trig_value": 0.5,         // 触发电平比例 (0~1)
        "auto_mode": false,        // AUTO 模式是否激活

        // Analog 信号
        "vdiv": 500,
        "map_unit": "V",
        "map_min": -5.0,
        "map_max": 5.0,
        "map_default": true
      }
    ]
  }
```

### 6.2 修改信号属性

```
PUT /api/v1/signals/{index}
Body: {
  "name": "SPI_CLK",              // 重命名
  "color": "#00ff00",             // 改色
  "view_index": 2,                // 拖拽排序
  "trigger": "rising",            // 设置触发（Logic）
  "vdiv": 500,                    // 调整垂直刻度（DSO/Analog）
  "coupling": "ac",               // 切换耦合方式（DSO）
  "zero_pos": 0.3,                // 移动零点位置（DSO/Analog）
  "trig_value": 0.6,              // 调整触发电平（DSO）
  "vfactor": 10                   // 设置探头衰减系数（DSO/Analog）
}
→ {"status": "ok"}
```

> 对应 Header 中双击重命名、点击改色、拖拽排序、触发按钮、DSO 旋钮等操作。

### 6.3 DSO AUTO 调节

```
POST /api/v1/signals/{index}/auto
→ {"status": "ok"}
```

> 对应 DSO 信号 Header 中的 AUTO 按钮。

### 6.4 DSO 通道使能切换

```
POST /api/v1/signals/{index}/toggle
→ {"status": "ok"}
```

> 对应 DSO 信号 Header 中的 EN/DIS 按钮。

---

## 七、Cursor — 光标 API

对应 UI：Viewport/Ruler 中的 Y 光标（时间光标）和 X 光标（电压光标）。

### 7.1 光标列表

```
GET /api/v1/cursors
→ {
    "y_cursors": [
      {
        "index": 0,
        "sample_pos": 5000000,
        "time": "50.000 ms",
        "color": "#ff6600"
      },
      {
        "index": 1,
        "sample_pos": 7500000,
        "time": "75.000 ms",
        "color": "#ffcc00"
      }
    ],
    "y_cursor_deltas": [
      {"i": 0, "j": 1, "delta_samples": 2500000, "delta_time": "25.000 ms"}
    ],
    "x_cursors": [
      {
        "index": 0,
        "signal_index": 0,
        "y_ratio": 0.3,           // 垂直位置比例
        "x0_ratio": 0.4,          // 水平线0位置比例
        "x1_ratio": 0.7,          // 水平线1位置比例
        "voltage_x0": "1.20 V",   // X0 线电压值
        "voltage_x1": "3.40 V",   // X1 线电压值
        "voltage_delta": "2.20 V" // 电压差值
      }
    ],
    "trigger_cursor": {
      "sample_pos": 5000000,
      "time": "50.000 ms"
    }
  }
```

### 7.2 添加光标

```
POST /api/v1/cursors/y
Body: {"sample_pos": 5000000}      // 可选，默认当前视口中心
→ {"index": 2, "sample_pos": 5000000, "time": "50.000 ms", "color": "#33cc33"}

POST /api/v1/cursors/x
Body: {"signal_index": 0, "y_ratio": 0.3, "x0_ratio": 0.4, "x1_ratio": 0.7}
→ {"index": 1, "signal_index": 0, ...}
```

> 对应 Viewport 中双击左键添加 Y 光标、右键菜单添加 X 光标。

### 7.3 移动光标

```
PUT /api/v1/cursors/y/{index}
Body: {"sample_pos": 6000000}
→ {"index": 2, "sample_pos": 6000000, "time": "60.000 ms"}

PUT /api/v1/cursors/x/{index}
Body: {"y_ratio": 0.35, "x0_ratio": 0.45, "x1_ratio": 0.75}
→ {"index": 1, ...}
```

> 对应 Viewport/Ruler 中拖拽光标线的操作。

### 7.4 删除光标

```
DELETE /api/v1/cursors/y/{index}
→ {"status": "ok"}

DELETE /api/v1/cursors/x/{index}
→ {"status": "ok"}
```

> 对应 Ruler 中点击光标标签的关闭按钮。

---

## 八、TriggerDock — 触发面板 API

对应 UI：TriggerDock（逻辑触发面板）和 DsoTriggerDock（DSO 触发面板）。

### 8.1 获取触发设置

```
GET /api/v1/trigger
→ {
    "mode": "logic",               // "logic" | "dso"

    // 逻辑触发
    "logic_trigger": {
      "type": "simple",            // "simple" | "advanced"
      "position": 50,              // 触发位置百分比 (0~100)
      "stages": 1,                 // 触发级数
      "simple_trigger": {
        "value0": "X",             // 每通道触发值: "0"|"1"|"X"(无关)
        "value1": "R",             // "R"(上升沿)|"F"(下降沿)|"X"
        ...
      },
      "advanced_trigger": {
        "stages": [
          {
            "logic": "and",        // "and" | "or" | "nand" | "nor"
            "value0": "R",
            "value1": "F",
            "count": 1,
            "inv": false,
            "serial": null         // 串行触发设置（高级模式）
          }
        ]
      }
    },

    // DSO 触发
    "dso_trigger": {
      "position": 50,              // 触发位置百分比
      "holdoff": 0,                // 触发保持时间 (ms)
      "margin": 0,                 // 触发余量百分比
      "source": "auto",            // "auto" | "ch0" | "ch1" | "ch0_and_ch1" | "ch0_or_ch1"
      "channel": 0,                // 触发通道索引
      "type": "rising"             // "rising" | "falling"
    }
  }
```

### 8.2 设置触发

```
PUT /api/v1/trigger
Body: {
  "logic_trigger": {
    "type": "simple",
    "position": 50,
    "simple_trigger": {
      "value0": "R",
      "value1": "F"
    }
  }
}
→ {"status": "ok"}
```

> 对应 TriggerDock/DsoTriggerDock 中各项参数的修改操作。

---

## 九、ProtocolDock — 协议解码面板 API

对应 UI：ProtocolDock 面板，包含解码器列表、添加/删除按钮、解码数据表格、搜索导航、导出。

### 9.1 解码器列表

```
GET /api/v1/decoders
→ {
    "decoders": [
      {
        "stack_id": 0,
        "decoder_id": "spi",
        "label": "SPI",
        "progress": 100,
        "is_running": false,
        "rows_count": 4,
        "rows": [
          {"index": 0, "title": "MOSI", "visible": true},
          {"index": 1, "title": "MISO", "visible": true},
          {"index": 2, "title": "CLK", "visible": false},
          {"index": 3, "title": "CS", "visible": true}
        ]
      }
    ]
  }
```

### 9.2 可用解码器列表

```
GET /api/v1/decoders/available
→ {
    "decoders": [
      {"id": "spi", "name": "SPI", "category": "bus"},
      {"id": "i2c", "name": "I²C", "category": "bus"},
      {"id": "uart", "name": "UART", "category": "bus"},
      {"id": "can", "name": "CAN", "category": "bus"}
    ]
  }
```

> 对应 ProtocolDock 中"添加协议"按钮弹出的解码器选择菜单。

### 9.3 添加解码器

```
POST /api/v1/decoders
Body: {
  "decoder_id": "spi",
  "channels": {"MOSI": 0, "CLK": 1, "CS": 2},
  "options": {"cpol": 0, "cpha": 0}
}
→ {
    "stack_id": 0,
    "decoder_id": "spi",
    "label": "SPI"
  }
```

> 对应 ProtocolDock 中点击"添加协议"按钮后的操作。

### 9.4 修改解码器配置

```
PUT /api/v1/decoders/{stack_id}
Body: {
  "channels": {"MOSI": 3},
  "options": {"cpol": 1},
  "label": "SPI-2",
  "row_visibility": {"0": true, "2": true}
}
→ {"status": "ok"}
```

> 对应 ProtocolDock 中解码器的设置按钮、格式选择、行显示/隐藏。

### 9.5 删除解码器

```
DELETE /api/v1/decoders/{stack_id}
→ {"status": "ok"}
```

> 对应 ProtocolDock 中解码器的删除按钮。

### 9.6 解码注解数据

```
GET /api/v1/decoders/{stack_id}/annotations?row=0&start=0&end=10000000
→ {
    "stack_id": 0,
    "row": 0,
    "samplerate": 100000000,
    "annotations": [
      {"start": 1000, "end": 1010, "texts": ["0x1A"]},
      {"start": 1010, "end": 1020, "texts": ["0x2B"]}
    ]
  }
```

> 对应 ProtocolDock 中解码数据表格的显示。

### 9.7 解码注解分页查询

```
GET /api/v1/decoders/{stack_id}/annotations/all?page=1&per_page=100
→ {
    "stack_id": 0,
    "page": 1,
    "per_page": 100,
    "total": 1500,
    "annotations": [...]
  }
```

### 9.8 解码数据搜索

```
GET /api/v1/decoders/{stack_id}/search?keyword=0x1A&row=0
→ {
    "stack_id": 0,
    "matches": [
      {"row": 0, "start": 1000, "end": 1010, "texts": ["0x1A"]},
      {"row": 0, "start": 5000, "end": 5010, "texts": ["0x1A"]}
    ],
    "total": 2
  }
```

> 对应 ProtocolDock 中搜索框和搜索导航按钮。

### 9.9 解码数据导出

```
POST /api/v1/decoders/{stack_id}/export
Body: {
  "format": "csv",                 // "csv" | "txt"
  "rows": [0, 1, 3]               // 要导出的行索引
}
→ 文件下载 (Content-Disposition: attachment)
```

> 对应 ProtocolExp 导出对话框中的操作。

### 9.10 设置解码格式

```
PUT /api/v1/decoders/{stack_id}/format
Body: {"format": "hex"}           // "hex" | "dec" | "oct" | "bin" | "ascii"
→ {"status": "ok"}
```

> 对应 ProtocolDock 中解码器的格式选择下拉框。

---

## 十、MeasureDock — 测量面板 API

对应 UI：MeasureDock 面板，包含鼠标测量、距离测量、边沿测量、光标管理。

### 10.1 获取测量数据

```
GET /api/v1/measurements
→ {
    "mouse_measure": {
      "signal_index": 0,
      "width": null,               // 脉宽
      "period": null,              // 周期
      "frequency": null,           // 频率
      "duty": null                 // 占空比
    },
    "distance_measures": [
      {
        "id": 0,
        "signal_index": 0,
        "start_sample": 1000,
        "end_sample": 2000,
        "distance": "10.000 us",
        "samples": 1000
      }
    ],
    "edge_measures": [
      {
        "id": 0,
        "signal_index": 0,
        "start_sample": 1000,
        "rising_count": 5,
        "falling_count": 4,
        "total_edges": 9
      }
    ],
    "dso_measure_slots": [         // DSO 模式 ViewStatus 中的测量槽位
      {"slot": 0, "signal_index": 0, "type": "vpp", "value": "3.20 V"},
      {"slot": 1, "signal_index": 0, "type": "freq", "value": "1.000 kHz"},
      {"slot": 2, "signal_index": 1, "type": "vmax", "value": "4.80 V"},
      {"slot": 3, "signal_index": 1, "type": "vmin", "value": "0.20 V"}
    ]
  }
```

### 10.2 添加测量项

```
POST /api/v1/measurements/distance
Body: {"signal_index": 0, "start_sample": 1000, "end_sample": 2000}
→ {"id": 1, "signal_index": 0, "distance": "10.000 us", "samples": 1000}

POST /api/v1/measurements/edge
Body: {"signal_index": 0, "start_sample": 1000}
→ {"id": 1, "signal_index": 0, "rising_count": 5, "falling_count": 4, "total_edges": 9}
```

> 对应 MeasureDock 中"添加距离测量"/"添加边沿测量"按钮。

### 10.3 删除测量项

```
DELETE /api/v1/measurements/distance/{id}
→ {"status": "ok"}

DELETE /api/v1/measurements/edge/{id}
→ {"status": "ok"}
```

### 10.4 设置 DSO 测量槽位

```
PUT /api/v1/measurements/dso-slot/{slot_index}
Body: {"signal_index": 0, "type": "vpp"}
→ {"slot": 0, "signal_index": 0, "type": "vpp", "value": "3.20 V"}
```

> 对应 ViewStatus 中点击 DSO 测量槽位弹出 DsoMeasure 对话框的操作。

### 10.5 DSO 可用测量类型

```
GET /api/v1/measurements/dso-types
→ {
    "types": [
      {"id": "vpp", "name": "Vpp"},
      {"id": "vmax", "name": "Vmax"},
      {"id": "vmin", "name": "Vmin"},
      {"id": "vamp", "name": "Vamp"},
      {"id": "vhigh", "name": "Vhigh"},
      {"id": "vlow", "name": "Vlow"},
      {"id": "vrms", "name": "Vrms"},
      {"id": "vmean", "name": "Vmean"},
      {"id": "period", "name": "Period"},
      {"id": "freq", "name": "Freq"},
      {"id": "rise", "name": "Rise"},
      {"id": "fall", "name": "Fall"},
      {"id": "pwidth", "name": "Pwidth"},
      {"id": "nwidth", "name": "Nwidth"},
      {"id": "duty", "name": "Duty"},
      {"id": "overshoot", "name": "Overshoot"},
      {"id": "preshoot", "name": "Preshoot"}
    ]
  }
```

---

## 十一、SearchDock — 搜索面板 API

对应 UI：SearchDock 面板和 Search 对话框，包含模式搜索和导航。

### 11.1 搜索设置

```
GET /api/v1/search
→ {
    "pattern": [                   // 每通道搜索模式
      {"signal_index": 0, "value": "R"},  // "R"=上升沿, "F"=下降沿, "0"=低, "1"=高, "X"=无关
      {"signal_index": 1, "value": "X"}
    ],
    "results_count": 15,
    "current_index": 0,
    "current_sample": 5000000
  }
```

```
PUT /api/v1/search
Body: {
  "pattern": [
    {"signal_index": 0, "value": "R"},
    {"signal_index": 1, "value": "F"}
  ]
}
→ {
    "results_count": 15,
    "current_index": 0,
    "current_sample": 5000000
  }
```

> 对应 Search 对话框中每通道边沿标志的设置。

### 11.2 搜索导航

```
POST /api/v1/search/next
→ {
    "current_index": 1,
    "current_sample": 7500000,
    "total": 15
  }

POST /api/v1/search/prev
→ {
    "current_index": 0,
    "current_sample": 5000000,
    "total": 15
  }
```

> 对应 SearchDock 中上一个/下一个按钮。

---

## 十二、Spectrum — 频谱视图 API

对应 UI：FFT 频谱视口（Viewport 的 FFT_VIEW 模式）和 FftOptions 对话框。

### 12.1 FFT 选项

```
GET /api/v1/spectrum/options
→ {
    "enabled": true,
    "channel": 0,
    "window_type": "hann",         // "rectangle" | "hann" | "hamming" | "blackman" | "flat_top"
    "length": 8192,                // FFT 长度: 1024|2048|4096|8192|16384
    "dc_remove": false,
    "view_mode": "dbv",            // "linear" | "dbv"
    "dbv_range": 100,              // 100 | 120 | 150 | 200
    "available_lengths": [1024, 2048, 4096, 8192, 16384],
    "available_windows": ["rectangle", "hann", "hamming", "blackman", "flat_top"]
  }
```

```
PUT /api/v1/spectrum/options
Body: {
  "enabled": true,
  "channel": 0,
  "window_type": "hann",
  "length": 8192,
  "dc_remove": false,
  "view_mode": "dbv",
  "dbv_range": 100
}
→ {"status": "ok"}
```

> 对应 FftOptions 对话框中的参数设置。

### 12.2 频谱数据

```
GET /api/v1/spectrum/data?start_ratio=0&end_ratio=1&width=1920
  参数:
    start_ratio: 频率起始比例 (0~1)
    end_ratio:   频率结束比例 (0~1)
    width:       视口像素宽度
→ {
    "channel": 0,
    "samplerate": 100000000,
    "fft_length": 8192,
    "freq_resolution": 12207.03,   // Hz
    "start_ratio": 0,
    "end_ratio": 1,
    "view_mode": "dbv",
    "data": [
      {"freq": 0, "value": -60.5},
      {"freq": 12207, "value": -45.2},
      ...
    ]
  }
```

> 对应 FFT Viewport 中的频谱渲染。

---

## 十三、Math — 数学运算与李萨如 API

对应 UI：MathOptions 对话框和 LissajousOptions 对话框。

### 13.1 数学运算

```
GET /api/v1/math
→ {
    "enabled": false,
    "source1": 0,
    "source2": 1,
    "operator": "add",             // "add" | "sub" | "mul" | "div"
    "vdiv": 1000,
    "zero_pos": 0.5
  }
```

```
PUT /api/v1/math
Body: {
  "enabled": true,
  "source1": 0,
  "source2": 1,
  "operator": "sub"
}
→ {"status": "ok"}
```

> 对应 MathOptions 对话框中的设置。

### 13.2 李萨如图形

```
GET /api/v1/math/lissajous
→ {
    "enabled": false,
    "x_channel": 0,
    "y_channel": 1,
    "percent": 100
  }
```

```
PUT /api/v1/math/lissajous
Body: {
  "enabled": true,
  "x_channel": 0,
  "y_channel": 1,
  "percent": 50
}
→ {"status": "ok"}
```

> 对应 LissajousOptions 对话框中的设置。

### 13.3 李萨如图形数据

```
GET /api/v1/math/lissajous/data?percent=100
→ {
    "x_channel": 0,
    "y_channel": 1,
    "points": [
      {"x": 128, "y": 64},
      {"x": 130, "y": 66},
      ...
    ]
  }
```

> 对应 Lissajous 图形的渲染数据。

---

## 十四、ViewStatus — 状态栏 API

对应 UI：底部状态栏，显示触发时间、RLE 深度、采集状态、DSO 测量值。

```
GET /api/v1/status
→ {
    "capture_state": "idle",       // "idle" | "capturing" | "stopped" | "triggered" | "waiting_trigger"
    "collect_mode": "single",
    "mode": "logic",               // "logic" | "dso" | "analog"
    "samplerate": 100000000,
    "sample_count": 10000000,
    "total_sample_count": 10000000,
    "ring_sample_count": 10000000,
    "ring_start": 0,
    "ring_end": 9999999,
    "trigger_pos": 5000000,
    "trigger_time": "2024-01-15T10:30:00",
    "rle_depth": "10M Samples Captured!",
    "capture_progress": 75,        // 0~100
    "is_stream": false,
    "is_single_buffer": true
  }
```

---

## 十五、FileBar — 文件操作 API

对应 UI：FileBar 和 QRibbon File 分类中的文件操作。

### 15.1 会话操作

```
GET /api/v1/file/session
→ 当前会话配置 JSON（同 .dsc 格式）

PUT /api/v1/file/session
Body: {会话配置 JSON}
→ {"status": "ok"}

POST /api/v1/file/session/default
→ {"status": "ok"}                 // 恢复默认会话
```

> 对应 FileBar 中的 Load/Store/Default 会话操作。

### 15.2 数据文件操作

```
POST /api/v1/file/open
Body: {"path": "C:/data/test.dsl"}
→ {"status": "ok", "mode": "logic", "samplerate": 100000000, "sample_count": 10000000}

POST /api/v1/file/save
Body: {"path": "C:/data/test.dsl", "start_cursor": null, "end_cursor": null}
→ {"status": "ok"}
```

> 对应 FileBar 中的 Open/Save 操作。

### 15.3 数据导出

```
POST /api/v1/file/export
Body: {
  "path": "C:/data/test.csv",
  "format": "csv",                 // "csv" | "vcd"
  "original_data": true,           // true=原始数据, false=压缩数据
  "start_cursor": null,            // 起始光标索引，null=从头
  "end_cursor": null               // 结束光标索引，null=到尾
}
→ 文件下载
```

> 对应 StoreProgress 导出对话框中的操作。

### 15.4 截图

```
GET /api/v1/file/capture?format=png
→ 图片文件下载 (image/png 或 image/jpeg)
```

> 对应 FileBar 中的 Capture 截图按钮。

---

## 十六、WebSocket 实时推送协议

### 16.1 连接

```
ws://localhost:8900/api/v1/ws
```

连接后服务端发送初始状态：

```json
{
  "type": "init",
  "capture_state": "idle",
  "mode": "logic",
  "samplerate": 100000000,
  "sample_count": 0,
  "signals": [...]
}
```

### 16.2 客户端→服务端消息

```json
// 订阅视口实时数据（对应 Viewport 实时刷新）
{
  "type": "subscribe_viewport",
  "start_sample": 0,
  "end_sample": 10000000,
  "width": 1920
}

// 更新视口范围（对应用户缩放/平移）
{
  "type": "update_viewport",
  "start_sample": 5000000,
  "end_sample": 6000000,
  "width": 1920
}

// 取消订阅
{
  "type": "unsubscribe_viewport"
}
```

### 16.3 服务端→客户端消息

**二进制帧**：波形数据，格式见第十七章。

**JSON 帧**：状态事件。

```json
// 采集状态变更（对应 SamplingBar/ViewStatus 更新）
{"type": "capture_state", "state": "capturing", "samplerate": 100000000}

// 数据更新（对应 Viewport 重绘）
{"type": "data_updated", "sample_count": 10000000}

// 采集进度（对应 ViewStatus 进度条）
{"type": "capture_progress", "progress": 75}

// 解码进度（对应 ProtocolDock 进度条）
{"type": "decode_progress", "stack_id": 0, "progress": 75}

// 解码完成
{"type": "decode_done", "stack_id": 0}

// DSO 测量值更新（对应 ViewStatus 测量槽位）
{"type": "measure_updated", "slots": [
  {"slot": 0, "value": "3.20 V"},
  {"slot": 1, "value": "1.000 kHz"}
]}

// 环形缓冲区重置
{"type": "ring_reset", "sample_count": 10000000, "ring_start": 5000000}

// 错误
{"type": "error", "message": "Device disconnected"}
```

### 16.4 流式推送流程

```
浏览器                          DSView Server
  │                                │
  │── subscribe_viewport ─────────→│  记录视口参数
  │                                │
  │                                │← 定时器触发 (33ms)
  │                                │   读取 _view_data 缓存
  │                                │   按信号类型提取增量数据
  │←── 二进制帧(波形数据) ─────────│  逻辑=边缘, DSO/Analog=包络
  │                                │
  │── update_viewport ────────────→│  用户缩放/平移
  │←── 二进制帧(新视口完整数据) ───│
  │                                │
  │←── JSON(measure_updated) ─────│  DSO 测量值更新
  │←── JSON(decode_progress) ─────│  解码进度更新
```

---

## 十七、WebSocket 二进制帧格式

### 17.1 帧头（8 字节）

```
字节 0:    帧类型
  0x01 = 逻辑边缘数据
  0x02 = DSO/Analog 包络数据
  0x03 = 解码注解更新
  0x04 = 状态事件
  0x05 = 错误事件
  0x06 = 视口重置

字节 1:    信号掩码 (bit0=信号0, bit1=信号1, ...)
字节 2-3:  保留
字节 4-7:  时间戳 (uint32, ms)
```

### 17.2 逻辑边缘帧 (type=0x01)

```
帧头 (8字节)
---
信号0边缘数 (uint16)
信号0边缘数据:
  位置0 (varint) — 相对于推送起始的偏移
  值0   (uint8)  — 0或1
  ...
信号1边缘数 (uint16)
信号1边缘数据:
  ...
```

### 17.3 DSO/Analog 包络帧 (type=0x02)

```
帧头 (8字节)
---
信号0:
  start_sample (uint64)
  scale        (uint32)
  length       (uint32)
  samples      (length × 2字节) — min/max 对
信号1:
  ...
```

---

## 十八、服务端缓存策略

### 18.1 核心原则

```
DSView 是缓存主人，Web API 只是读取者。
浏览器永远不缓存原始采样数据。
所有 Mipmap/Envelope 计算在 DSView 服务端完成。
```

### 18.2 视口数据提取策略（服务端自动选择）

| 信号类型 | 缩放级别 | 提取方法 | 数据量 |
|---------|---------|---------|--------|
| Logic | 全局概览 | RootNode.first/last | KB 级 |
| Logic | 中等缩放 | get_display_edges() | KB 级 |
| Logic | 深度缩放 | get_sample() 逐点 | KB~百KB 级 |
| DSO | 全局/中等 | get_envelope_section() 高层级 | <1KB~KB 级 |
| DSO | 深度缩放 | get_samples() 逐点 | KB~百KB 级 |
| Analog | 全局/中等 | get_envelope_section() | KB 级 |
| Analog | 深度缩放 | get_samples() 逐点 | KB~百KB 级 |

> 前端无需知道这些细节，只需发送视口参数，服务端自动选择最优策略。

### 18.3 浏览器端缓存

| 数据类型 | 缓存策略 |
|---------|---------|
| 视口波形数据 | 缓存当前视口（边缘序列/包络数据） |
| 解码注解 | 全部缓存（数据量极小） |
| 信号列表/状态 | 缓存 + WebSocket 事件同步 |
| 原始采样数据 | 不缓存 |

---

## 十九、对 SigSession 的最小改动

### 19.1 新增公共访问方法

```cpp
// sigsession.h
public:
    SessionData* get_view_data() const { return _view_data; }
    SessionData* get_capture_data() const { return _capture_data; }
```

### 19.2 新增数据监听器接口

```cpp
// sigsession.h
public:
    class IWebDataListener {
    public:
        virtual ~IWebDataListener() {}
        virtual void on_data_updated() = 0;
        virtual void on_frame_ended() = 0;
        virtual void on_frame_began() = 0;
        virtual void on_decode_done(int stack_id) = 0;
        virtual void on_capture_state_changed(int state) = 0;
        virtual void on_measure_updated() = 0;
    };

    void add_web_data_listener(IWebDataListener* listener);
    void remove_web_data_listener(IWebDataListener* listener);
```

### 19.3 Snapshot 访问权限

```cpp
// snapshot.h 新增:
public:
    std::mutex& mutex() const { return _mutex; }

// logicsnapshot.h 新增:
public:
    int get_channel_order(int sig_index) { return get_ch_order(sig_index); }

// dsosnapshot.h 新增:
public:
    int get_channel_order(int sig_index) { return get_ch_order(sig_index); }
```

---

## 二十、性能预算

### 20.1 单次查询延迟

| API | 目标延迟 | 数据量 |
|-----|---------|--------|
| GET /status | <5ms | <1KB |
| GET /device/info | <5ms | <1KB |
| GET /viewport/waveform | <50ms | <500KB |
| GET /signals | <10ms | <5KB |
| GET /cursors | <5ms | <2KB |
| GET /decoders/{id}/annotations | <20ms | <100KB |
| GET /spectrum/data | <30ms | <200KB |

### 20.2 流式推送带宽

| 场景 | 推送频率 | 每帧数据量 | 带宽需求 |
|------|---------|-----------|---------|
| 逻辑信号(稳定, 8ch) | 30fps | ~10KB | ~300KB/s |
| 逻辑信号(活跃, 8ch) | 30fps | ~240KB | ~7.2MB/s |
| DSO(2ch, envelope) | 30fps | ~8KB | ~240KB/s |
| Analog(2ch, envelope) | 30fps | ~16KB | ~480KB/s |

---

## 二十一、实施优先级（B 类优先）

按"Web 做不到的先做"原则排列。B 类是服务端必须实现的 API，A 类由前端自行处理。

### Phase 1 — 最小可用（B 类核心：看波形）

没有这些，Web 前端什么都做不了。

| API | 说明 |
|-----|------|
| `GET /status` | 采集状态，页面加载首先需要 |
| `GET /device/info` | 设备信息，决定 UI 布局 |
| `GET /capture/devices` | 设备列表 |
| `GET /viewport/waveform` | **核心**：波形数据，没有它就没有波形 |
| `GET /signals` | 信号列表（只读，前端自行管理名称/颜色/排序） |

### Phase 2 — 采集控制（B 类：能采数据）

| API | 说明 |
|-----|------|
| `POST /capture/start` | 启动采集 |
| `POST /capture/stop` | 停止采集 |
| `GET/PUT /capture/parameters` | 采样率/采样深度 |
| `PUT /capture/device` | 切换设备 |
| `PUT /device/mode` | 切换 Logic/DSO/Analog |

### Phase 3 — 设备配置（B 类：配硬件）

| API | 说明 |
|-----|------|
| `GET/PUT /device/channels` | 通道使能/探头参数 |
| `GET/PUT /device/options` | 操作模式/阈值/PWM/滤波等 |
| `POST /device/calibrate/*` | 校准 |

### Phase 4 — 触发（B 类：能触发采集）

| API | 说明 |
|-----|------|
| `GET/PUT /trigger` | 触发条件设置 |

### Phase 5 — 解码（B 类：协议分析）

| API | 说明 |
|-----|------|
| `GET /decoders` | 解码器列表 |
| `GET /decoders/available` | 可用解码器 |
| `POST /decoders` | 添加解码器 |
| `PUT /decoders/{id}` | 修改解码器配置 |
| `DELETE /decoders/{id}` | 删除解码器 |
| `GET /decoders/{id}/annotations` | 解码注解数据 |
| `GET /decoders/{id}/annotations/all` | 分页查询 |
| `GET /decoders/{id}/search` | 搜索注解 |
| `POST /decoders/{id}/export` | 导出解码数据 |

### Phase 6 — 搜索/测量/FFT/数学（B 类：高级分析）

| API | 说明 |
|-----|------|
| `PUT /search` | 执行模式搜索（搜索结果导航是 A 类，前端做） |
| `GET /measurements`（dso_measure_slots） | DSO 测量值 |
| `PUT /measurements/dso-slot/{slot}` | 设置测量槽位 |
| `GET /measurements/dso-types` | 可用测量类型 |
| `GET /spectrum/options` | FFT 选项 |
| `PUT /spectrum/options` | 设置 FFT 选项 |
| `GET /spectrum/data` | FFT 频谱数据 |
| `GET /math` | 数学运算配置+数据 |
| `PUT /math` | 设置数学运算 |
| `GET /math/lissajous` | 李萨如配置 |
| `PUT /math/lissajous` | 设置李萨如 |
| `GET /math/lissajous/data` | 李萨如数据 |
| `GET /viewport/raw` | 深度缩放原始数据 |

### Phase 7 — 文件操作（B 类：持久化）

| API | 说明 |
|-----|------|
| `POST /file/open` | 打开 .dsl 文件 |
| `POST /file/save` | 保存 .dsl 文件 |
| `POST /file/export` | 导出 CSV/VCD |
| `GET /file/session` | 读取会话配置 |
| `PUT /file/session` | 保存会话配置（含 A 类同步数据） |
| `POST /file/session/default` | 恢复默认会话 |

### Phase 8 — WebSocket 流式推送（B 类：实时）

| API | 说明 |
|-----|------|
| `ws://.../api/v1/ws` | WebSocket 连接 |
| subscribe_viewport | 订阅视口 |
| update_viewport | 更新视口 |
| 二进制帧推送 | 流式波形数据 |

### Phase 9 — 前端网页（A 类实现）

前端自行实现以下功能，无需服务端 API：

| 功能 | 实现方式 |
|------|---------|
| 光标管理 | JavaScript 状态管理 + Canvas 渲染 |
| 视口缩放/平移 | JavaScript 管理 scale/offset |
| 信号重命名/改色/排序 | JavaScript 本地状态，保存时随 /file/session 同步 |
| 解码格式选择 | 前端 hex/dec/bin/ascii 转换 |
| 搜索导航 | 在搜索结果数组中翻页 |
| 距离/边沿/鼠标测量 | 从边缘/包络数据本地计算 |
| FFT 视口缩放 | JavaScript 管理频率轴 |
| 主题切换 | CSS 变量切换 |
| 语言切换 | 前端 i18n |
| 截图 | canvas.toDataURL() / html2canvas |

### Phase 10 — 性能优化

| 内容 |
|------|
| 二进制帧 varint 编码 |
| 自适应降频 |
| 增量推送优化 |
| 环形缓冲区回绕处理 |

---

## 附录：新旧 API 对照表（含分类标记）

| 旧 API（底层驱动） | 新 API（UI 驱动） | 分类 | 设计理由 |
|-------------------|-------------------|------|---------|
| `GET /waveform/logic/edges` | `GET /viewport/waveform` | **B** | 前端无需区分信号类型，服务端自动处理 |
| `GET /waveform/dso/envelope` | `GET /viewport/waveform` | **B** | 同上 |
| `GET /waveform/analog/envelope` | `GET /viewport/waveform` | **B** | 同上 |
| `GET /waveform/dso/raw` | `GET /viewport/raw` | **B** | 统一深度缩放数据获取 |
| `GET /waveform/analog/raw` | `GET /viewport/raw` | **B** | 同上 |
| `GET /device/channels` | `GET /signals` + `GET /device/channels` | **B** | 信号属性和设备通道分离 |
| 无 | `GET /signals` | **B** | 信号列表（只读） |
| 无 | `PUT /signals/{id}` | **A** | 前端自行管理，保存时同步 |
| 无 | `GET/POST/PUT/DELETE /cursors` | **A** | 前端自行管理光标，保存时同步 |
| 无 | `GET/PUT /trigger` | **B** | 需配置硬件触发 |
| 无 | `GET /measurements` | **B**（DSO）/ **A**（其他） | DSO 测量需原始数据；距离/边沿测量前端可算 |
| 无 | `GET/PUT /search` | **B**（执行搜索）/ **A**（导航） | 搜索需 Snapshot 访问；导航前端可做 |
| 无 | `GET/PUT /spectrum/*` | **B** | FFT 需原始数据计算 |
| 无 | `GET/PUT /math/*` | **B** | 数学运算需原始数据 |
| 无 | `GET/PUT /file/*` | **B** | 文件操作需服务端 |
| 无 | `GET/PUT /device/options` | **B** | 需配置硬件 |
| `POST /capture/start` | `POST /capture/start` | **B** | 保留，增加 instant 参数 |
| `GET /status` | `GET /status` | **B** | 保留，扩展字段 |
| 无 | `PUT /viewport/state` | **A** | 前端自行管理缩放/平移 |
| 无 | `PUT /decoders/{id}/format` | **A** | 前端 hex/dec 转换 |
| 无 | `GET /file/capture` | **A** | 前端 canvas.toDataURL() |
