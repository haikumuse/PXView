# C 解码器 API v4 全面重设计 Spec

## Why

当前 C 解码器 API (v3) 无法将 Python 解码器机械翻译为 C，且部分现有 C 解码器实现存在错误。需要：1) 全面删除旧 API 并重建新 API，使每个 Python 表达式有且仅有一个 C 对应物；2) 建立自动化 Python/C 对比测试，确保 C 解码器输出与 Python 版本一致；3) 从 Python 版本重新迁移所有 215 个 C 解码器，消除现有实现中的错误。

## What Changes

- **BREAKING**: 全面删除旧 API（c_cond_* 系列、recv_proto、c_decoder_get_pin 等），不保留向后兼容
- **BREAKING**: 新增变参声明式 `c_wait()` 替代构建器模式
- **BREAKING**: 新增 `c_proto()` / `decode_upper()` 结构化协议数据传递，替代 cmd+扁平buffer
- **BREAKING**: 新增 `C_DECODER_STATE` / `C_DECODER_DEFINE` 宏消除样板代码
- **BREAKING**: 统一上层解码器回调为 `decode_upper()`
- **BREAKING**: `di_samplenum` / `di_matched` / `c_pin()` 直接访问
- **BREAKING**: API 版本升至 4
- **新增**: 适配现有测试框架（run_all_tests.py、decoder_test.c）支持 v4 API
- **新增**: 从 Python 重新迁移全部 215 个 C 解码器

## Impact

- Affected code:
  - `libsigrokdecode/libsigrokdecode.h` — 删除旧 API 声明，新增 v4 API，版本号升至 4
  - `libsigrokdecode/c_decoder_api.c` — 删除旧实现，新增 v4 实现
  - `libsigrokdecode/c_decoders/*.c` — 全部 215 个 C 解码器从 Python 重新迁移
  - `libsigrokdecode/decoder.c` — 适配新结构体
  - `libsigrokdecode/instance.c` — decode_upper 调用路径
  - `libsigrokdecode/tests/decoder_test.c` — 适配 v4 API
  - `CMakeLists.txt` — 更新 C_DECODERS 列表（如有新增/删除）

---

## ADDED Requirements

### Requirement: 变参声明式 c_wait()

C 解码器框架 SHALL 提供变参声明式 `c_wait()` 函数，用一行代码替代已删除的 c_cond_new/c_cond_*/c_cond_wait/c_cond_free。

```c
#define _CW(type, ch)  ((int)((type) << 16 | ((ch) & 0xFFFF)))
#define H(ch)   _CW(SRD_TERM_HIGH, ch)
#define L(ch)   _CW(SRD_TERM_LOW, ch)
#define R(ch)   _CW(SRD_TERM_RISING_EDGE, ch)
#define F(ch)   _CW(SRD_TERM_FALLING_EDGE, ch)
#define E(ch)   _CW(SRD_TERM_EITHER_EDGE, ch)
#define N(ch)   _CW(SRD_TERM_NO_EDGE, ch)
#define SKIP(n) _CW(SRD_TERM_SKIP, -1)
#define OR  (-1)
#define END (-2)

int c_wait(struct srd_decoder_inst *di, ...);
```

#### Scenario: 简单条件
- **WHEN** `c_wait(di, R(0), END)` → 等价 Python `self.wait({0: 'r'})`

#### Scenario: OR 条件
- **WHEN** `c_wait(di, R(0), OR, H(0),F(1), END)` → 等价 Python `self.wait([{0:'r'}, {0:'h',1:'f'}])`

#### Scenario: skip 条件
- **WHEN** `c_wait(di, E(0), OR, SKIP(n), END)` → 等价 Python `self.wait([{0:'e'}, {'skip': n}])`
- **AND** SKIP 的 n 值通过 va_arg 读取 uint64_t

#### Scenario: 空条件
- **WHEN** `c_wait(di, END)` → 等价 Python `self.wait({})`

#### 实现方案
va_list 遍历参数，栈上构建条件列表，调用底层 wait。c_wait() 执行后缓存引脚值到 di->c_pin_cache。

---

### Requirement: di_samplenum / di_matched / c_pin() 快捷访问

```c
#define di_samplenum(di)   ((di)->abs_cur_samplenum)
#define di_matched(di)     ((di)->match_array)
uint8_t c_pin(struct srd_decoder_inst *di, int ch);
```

- `di_samplenum(di)` 等价 Python `self.samplenum`
- `di_matched(di)` 等价 Python `self.matched`
- `c_pin(di, ch)` 等价 Python `wait()` 返回值元组元素，未连接通道返回 0xFF

---

### Requirement: c_put / c_put_v 简化注解宏

```c
#define c_put(di, ss, es, out_id, cls, ...)  C_ANN_PUT(di, ss, es, out_id, cls, __VA_ARGS__)
#define c_put_v(di, ss, es, out_id, cls, val, ...)  C_ANN_PUT_VAL(di, ss, es, out_id, cls, val, __VA_ARGS__)
```

---

### Requirement: c_field 结构体和 c_proto() 结构化协议输出

```c
enum c_field_type {
    C_FIELD_U8=0, C_FIELD_U16, C_FIELD_U32, C_FIELD_U64,
    C_FIELD_I8, C_FIELD_I16, C_FIELD_I32, C_FIELD_I64,
    C_FIELD_F64, C_FIELD_STR, C_FIELD_BYTES
};

typedef struct {
    uint8_t type;
    union {
        uint8_t  u8;  uint16_t u16; uint32_t u32; uint64_t u64;
        int8_t   i8;  int16_t  i16; int32_t  i32; int64_t  i64;
        double   f64;
        const char *str;
        struct { const uint8_t *data; uint32_t len; } bytes;
    };
} c_field;

#define C_U8(v)    ((c_field){.type=C_FIELD_U8, .u8=(uint8_t)(v)})
#define C_U16(v)   ((c_field){.type=C_FIELD_U16, .u16=(uint16_t)(v)})
#define C_U32(v)   ((c_field){.type=C_FIELD_U32, .u32=(uint32_t)(v)})
#define C_U64(v)   ((c_field){.type=C_FIELD_U64, .u64=(uint64_t)(v)})
#define C_I8(v)    ((c_field){.type=C_FIELD_I8, .i8=(int8_t)(v)})
#define C_I16(v)   ((c_field){.type=C_FIELD_I16, .i16=(int16_t)(v)})
#define C_I32(v)   ((c_field){.type=C_FIELD_I32, .i32=(int32_t)(v)})
#define C_I64(v)   ((c_field){.type=C_FIELD_I64, .i64=(int64_t)(v)})
#define C_F64(v)   ((c_field){.type=C_FIELD_F64, .f64=(double)(v)})
#define C_STR(v)   ((c_field){.type=C_FIELD_STR, .str=(const char*)(v)})
#define C_BYTES(d,n) ((c_field){.type=C_FIELD_BYTES, .bytes={.data=(d),.len=(n)}})

int c_proto(struct srd_decoder_inst *di, uint64_t ss, uint64_t es,
            int out_id, const char *cmd, ...);  // 以 NULL 结尾
```

#### Scenario: I2C 输出协议数据
- `c_proto(di, ss, es, out, "DATA WRITE", C_U8(d), NULL)` → Python `self.put(ss, es, out_py, ['DATA WRITE', d])`

#### Scenario: SPI 输出多字段
- `c_proto(di, ss, es, out, "DATA", C_U8(miso), C_U8(mosi), NULL)` → Python `self.put(ss, es, out_py, ['DATA', miso, mosi])`

#### Scenario: 无数据事件
- `c_proto(di, ss, es, out, "START", NULL)` → Python `self.put(ss, es, out_py, ['START', None])`

---

### Requirement: decode_upper 替代 recv_proto

```c
void (*decode_upper)(struct srd_decoder_inst *di, uint64_t start_sample,
                     uint64_t end_sample, const char *cmd,
                     const c_field *fields, int n_fields);
```

| Python | C v4 |
|--------|------|
| `def decode(self):` (底层) | `.decode = my_decode` |
| `def decode(self, ss, es, data):` (上层) | `.decode_upper = my_decode_upper` |
| `data[0]` | `cmd` |
| `data[1]` | `fields[0].u8` / `fields[0].str` |
| `data[2]` | `fields[1].u8` / `fields[1].str` |

---

### Requirement: C_DECODER_STATE 宏

```c
C_DECODER_STATE(my_state, {
    int state;
    int bitcount;
    uint8_t databyte;
    int out_ann;
    int out_python;
});
// 自动生成: typedef struct、reset(calloc+set_private)、destroy(free)
```

---

### Requirement: C_DECODER_DEFINE 宏

```c
C_DECODER_DEFINE(i2c_c,
    .id = "i2c_c",
    .name = "I²C(C)",
    // ... 所有字段 ...
    .decode = i2c_decode,
    .decode_upper = NULL,  // 底层解码器
);
// 自动生成: srd_c_decoder 结构体实例 + DLL 入口函数
```

---

### Requirement: c_opt_bool() 布尔选项读取

```c
int c_opt_bool(struct srd_decoder_inst *di, const char *key, int defval);
// "yes"/"true"/"1" → 1, "no"/"false"/"0" → 0
```

---

### Requirement: 快捷 API 别名

| 新名称 | 等价旧名称 |
|--------|-----------|
| `c_opt_int` | `c_decoder_get_option_int` |
| `c_opt_str` | `c_decoder_get_option_string` |
| `c_opt_dbl` | `c_decoder_get_option_double` |
| `c_has_ch` | `c_decoder_has_channel` |
| `c_samplerate` | `c_decoder_get_samplerate` |
| `c_last_samplenum` | `c_decoder_get_last_samplenum` |
| `c_init_pin` | `c_decoder_get_initial_pin` |
| `c_reg_out` | `c_decoder_register_output` |
| `c_reg_meta` | `c_decoder_register_output_meta` |
| `c_put_bin` | `c_decoder_put_binary` |
| `c_put_logic` | `c_decoder_put_logic` |
| `c_put_meta_int` | `c_decoder_put_meta_int` |
| `c_put_meta_dbl` | `c_decoder_put_meta_double` |

---

### Requirement: 自动化 Python/C 对比测试

适配现有测试框架支持 v4 API：

1. `decoder_test.c` — 适配 decode_upper 调用路径，确保 C 解码器输出正确
2. `run_all_tests.py` — 保持 Python/C 对比逻辑不变，仅更新 C→Python 解码器名称映射
3. `test_factory.py` — 保持测试数据生成不变
4. `protocol_synthesizer.py` — 保持协议合成不变

#### Scenario: 运行全量对比测试
- **WHEN** 执行 `python run_all_tests.py --all`
- **THEN** 对每个 C 解码器，运行 Python 版本生成 expected_py.json，运行 C 版本生成 actual_c.json
- **AND** 对比两者注解，输出 PASS/FAIL/WARN/SKIP/ERROR

---

### Requirement: API 版本升至 4

`SRD_C_DECODER_API_VERSION` 从 3 升至 4。DLL 加载时版本不匹配则拒绝。

---

## MODIFIED Requirements

### Requirement: srd_c_decoder 结构体

- 移除 `recv_proto`
- 新增 `decode_upper`
- 新增 `state_size`（供 C_DECODER_STATE 自动 reset 使用）

```c
struct srd_c_decoder {
    // ... 元数据字段不变 ...
    size_t state_size;  // C_DECODER_STATE 自动设置

    void (*reset)(struct srd_decoder_inst *di);
    void (*start)(struct srd_decoder_inst *di);
    void (*decode)(struct srd_decoder_inst *di);
    void (*end)(struct srd_decoder_inst *di);
    void (*metadata)(struct srd_decoder_inst *di, int key, uint64_t value);
    void (*destroy)(struct srd_decoder_inst *di);
    void (*decode_upper)(struct srd_decoder_inst *di,
                         uint64_t start_sample, uint64_t end_sample,
                         const char *cmd, const c_field *fields, int n_fields);
};
```

---

## REMOVED Requirements

### Requirement: c_cond_* 构建器系列 API
**Reason**: 被 `c_wait()` 完全替代。变参声明式更简洁，无需堆分配。
**Migration**: 所有 `c_cond_new/rise/fall/high/low/edge/noedge/skip/or/wait/free` 调用替换为 `c_wait()`。

### Requirement: recv_proto 回调
**Reason**: 被 `decode_upper` 替代。cmd+扁平buffer 无法与 Python 列表模型对齐。
**Migration**: 所有 `recv_proto` 实现改为 `decode_upper`，手动反序列化改为 `fields[N].u8`。

### Requirement: c_decoder_put_proto / c_decoder_put_python
**Reason**: 被 `c_proto()` 替代。
**Migration**: `c_decoder_put_python(di, ss, es, out, "CMD", &d, 1)` → `c_proto(di, ss, es, out, "CMD", C_U8(d), NULL)`

### Requirement: c_decoder_get_pin
**Reason**: 被 `c_pin()` 替代。c_pin 从缓存读取，无需 samplenum 参数。
**Migration**: `c_decoder_get_pin(di, ch, samplenum)` → `c_pin(di, ch)`

### Requirement: c_cond_wait_current
**Reason**: 被 `c_wait(di, END)` 替代。
**Migration**: `c_cond_wait_current(di, &samplenum)` → `c_wait(di, END)`

### Requirement: 旧式长名称 API
**Reason**: 被 `c_opt_int/c_opt_str/c_has_ch/c_samplerate` 等短名称替代。
**Migration**: 见快捷 API 别名表。

---

## Python → C v4 机械翻译对照表

| Python | C v4 | 行数比 |
|--------|------|--------|
| `self.wait({0: 'r'})` | `c_wait(di, R(0), END)` | 1:1 |
| `self.wait([{0:'r'}, {0:'h',1:'f'}])` | `c_wait(di, R(0), OR, H(0),F(1), END)` | 1:1 |
| `self.wait({})` | `c_wait(di, END)` | 1:1 |
| `self.wait({'skip': N})` | `c_wait(di, SKIP(N), END)` | 1:1 |
| `(clk, sda) = self.wait(...)` | `c_wait(...); clk=c_pin(di,0); sda=c_pin(di,1)` | 1:2 |
| `self.samplenum` | `di_samplenum(di)` | 1:1 |
| `self.matched & (1<<n)` | `di_matched(di) & (1<<n)` | 1:1 |
| `self.put(ss,es,out,[cls,[txt]])` | `c_put(di,ss,es,out,cls,txt)` | 1:1 |
| `self.put(ss,es,out,[cls,['@FF',255]])` | `c_put_v(di,ss,es,out,cls,255,"0xFF","FF")` | 1:1 |
| `self.put(ss,es,out_py,['CMD',val])` | `c_proto(di,ss,es,out_py,"CMD",C_U8(val),NULL)` | 1:1 |
| `self.put(ss,es,out_py,['CMD',None])` | `c_proto(di,ss,es,out_py,"CMD",NULL)` | 1:1 |
| `self.options['key']` (int) | `c_opt_int(di,"key",def)` | 1:1 |
| `self.options['key']` (string) | `c_opt_str(di,"key",def)` | 1:1 |
| `self.options['key']=='yes'` | `c_opt_bool(di,"key",def)` | 1:1 |
| `self.has_channel(n)` | `c_has_ch(di,n)` | 1:1 |
| `self.samplerate` | `c_samplerate(di)` | 1:1 |
| `self.xxx = val` | `s->xxx = val` | 1:1 |
| `data[0]` (上层) | `cmd` | 1:1 |
| `data[1]` (上层) | `fields[0].u8` | 1:1 |
| `data[2]` (上层) | `fields[1].u8` | 1:1 |

---

## 迁移策略：从 Python 重新迁移全部 215 个 C 解码器

### 原则
1. 每个 C 解码器从对应的 Python pd.py 重新翻译，不参考旧 C 实现
2. 使用新 API（c_wait、c_proto、C_DECODER_STATE、C_DECODER_DEFINE）
3. 按协议栈依赖顺序迁移：底层 → 上层
4. 每批迁移后运行自动化对比测试

### 迁移顺序

#### Phase 1: 核心底层解码器 (10 个) — 所有上层解码器的依赖

| 序号 | 解码器 | Python源 | 上层依赖数 |
|------|--------|---------|-----------|
| 1 | i2c_c | decoders/i2c/pd.py | 26 |
| 2 | spi_c | decoders/spi/pd.py | 28 |
| 3 | uart_c | decoders/uart/pd.py | 17 |
| 4 | jtag_c | decoders/jtag/pd.py | 3 |
| 5 | swd_c | decoders/swd/pd.py | 0 |
| 6 | can_c | decoders/can/pd.py | 0 |
| 7 | can_fd_c | decoders/can-fd/pd.py | 0 |
| 8 | onewire_link_c | decoders/onewire_link/pd.py | 3 |
| 9 | ps2_c | decoders/ps2/pd.py | 2 |
| 10 | usb_signalling_c | decoders/usb_signalling/pd.py | 2 |

#### Phase 2: 二级底层解码器 (15 个) — 深层协议栈依赖

| 序号 | 解码器 | Python源 | 上层依赖数 |
|------|--------|---------|-----------|
| 11 | nrzi_c | decoders/nrzi/pd.py | 1 |
| 12 | 4b5b_c | decoders/4b5b/pd.py | 1 |
| 13 | ethernet_c | decoders/ethernet/pd.py | 2 |
| 14 | ipv4_c | decoders/ipv4/pd.py | 1 |
| 15 | onewire_network_c | decoders/onewire_network/pd.py | 3 |
| 16 | usb_packet_c | decoders/usb_packet/pd.py | 1 |
| 17 | i2s_c | decoders/i2s/pd.py | 0 |
| 18 | hdlc_c | decoders/hdlc/pd.py | 0 |
| 19 | mdio_c | decoders/mdio/pd.py | 1 |
| 20 | microwire_c | decoders/microwire/pd.py | 1 |
| 21 | iso7816_c | decoders/iso7816/pd.py | 0 |
| 22 | iebus_c | decoders/iebus/pd.py | 1 |
| 23 | ook_c | decoders/ook/pd.py | 2 |
| 24 | afsk_c | decoders/afsk/pd.py | 1 |
| 25 | tmc_c | decoders/tmc/pd.py | 2 |

#### Phase 3: 剩余底层解码器 (88 个)

所有 inputs=['logic'] 的解码器，按字母顺序分批迁移。

#### Phase 4: 上层解码器 (102 个)

按输入类型分批：
- I2C 上层 (26个)
- SPI 上层 (28个)
- UART 上层 (17个)
- JTAG 上层 (3个)
- 其他上层 (28个)

#### Phase 5: 特殊前缀版本 (6 个)

0-i2c, 0-spi, 0-uart, 1-i2c, 1-spi, 1-uart — 这些是同一协议的不同版本号变体。

---

## 完整 API 清单 (v4)

### 核心 API

| API | Python 对应 |
|-----|-----------|
| `c_wait(di, ...END)` | `self.wait(conds)` |
| `c_pin(di, ch)` | wait() 返回值元组元素 |
| `di_samplenum(di)` | `self.samplenum` |
| `di_matched(di)` | `self.matched` |
| `c_put(di,ss,es,out,cls,...)` | `self.put(ss,es,out,[cls,[txts]])` |
| `c_put_v(di,ss,es,out,cls,val,...)` | `self.put(ss,es,out,[cls,['@FF',val]])` |
| `c_proto(di,ss,es,out,cmd,...NULL)` | `self.put(ss,es,out_py,[cmd,data])` |
| `c_opt_int(di,key,def)` | `self.options[key]` (int) |
| `c_opt_str(di,key,def)` | `self.options[key]` (str) |
| `c_opt_bool(di,key,def)` | `self.options[key]=='yes'` |
| `c_opt_dbl(di,key,def)` | `self.options[key]` (float) |
| `c_has_ch(di,ch)` | `self.has_channel(ch)` |
| `c_samplerate(di)` | `self.samplerate` |
| `c_last_samplenum(di)` | `self.last_samplenum` |
| `c_init_pin(di,ch)` | `self.initial_pins[ch]` |
| `c_reg_out(di,type,proto)` | `self.register(type)` |
| `c_reg_meta(di,type,proto,name,desc)` | `self.register(META,meta=...)` |
| `c_put_bin(di,ss,es,out,cls,size,data)` | `self.put(ss,es,out_bin,[cls,bytes])` |
| `c_put_logic(di,ss,es,out,mask,vals,n)` | `self.put(ss,es,out_logic,...)` |
| `c_put_meta_int(di,ss,es,out,val)` | `self.put(ss,es,out_meta,val)` |
| `c_put_meta_dbl(di,ss,es,out,val)` | `self.put(ss,es,out_meta,val)` |

### 条件宏

| 宏 | Python |
|----|--------|
| `H(ch)` | `{ch: 'h'}` |
| `L(ch)` | `{ch: 'l'}` |
| `R(ch)` | `{ch: 'r'}` |
| `F(ch)` | `{ch: 'f'}` |
| `E(ch)` | `{ch: 'e'}` |
| `N(ch)` | `{ch: 'n'}` |
| `SKIP(n)` | `{'skip': n}` |
| `OR` | 列表分隔 |
| `END` | (隐式) |

### 协议字段宏

| 宏 | Python |
|----|--------|
| `C_U8(v)` ~ `C_U64(v)` | 整数 |
| `C_I8(v)` ~ `C_I64(v)` | 整数 |
| `C_F64(v)` | 浮点数 |
| `C_STR(v)` | 字符串 |
| `C_BYTES(d,n)` | bytes |

### 解码器定义宏

| 宏 | 说明 |
|----|------|
| `C_DECODER_STATE(name, {fields})` | 自动生成状态结构体、reset、destroy |
| `C_DECODER_DEFINE(name, ...)` | 自动生成解码器结构体和 DLL 入口 |
