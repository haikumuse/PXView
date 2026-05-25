# C解码器第三轮审查修复 Spec

## Why
前两轮审查修复了大部分严重问题，但第三轮审查仍发现多个逻辑bug、缺失功能和一致性问题需要修复，包括：SI前缀条件错误、除零风险、缺失的采样率检查、注解ID格式不一致、缺失的ADDRESS READ处理等。

## What Changes
- 修复 graycode_c.c 的 `forced_exp > 0` 条件为 `forced_exp >= 0`
- 修复 c2_c.c 缺失的采样率检查（防止除零崩溃）
- 修复 c2_c.c 注解标签缺少ATK颜色代码
- 修复 pwm_c.c 微秒格式字符串多余空格
- 修复 ds1307_c.c 缺失的 ADDRESS READ 处理
- 修复 ds3231_c.c 缺失的非DS3231地址警告
- 修复 ds3231_c.c 注解ID使用下划线而非连字符
- 补充 lpc_c.c、counter_c.c、seven_segment_c.c、pwm_c.c、can_fd_c.c 的idn字段

## Impact
- Affected code: `libsigrokdecode/c_decoders/` 下11个C解码器
- 无破坏性变更

## ADDED Requirements

### Requirement: graycode_c.c SI前缀条件修正
`format_si_value` 中 `forced_exp > 0` 必须改为 `forced_exp >= 0`，以匹配Python的 `emin is not None and e < emin` 语义。当 `emin=0` 时，指数低于0的值应被强制为0（如RPM显示"0.5 rpm"而非"500 mrpm"）。同时需区分"无强制"和"强制到0"两种语义。

### Requirement: c2_c.c 采样率检查
`c2_decode` 函数入口必须检查采样率，为0时安全返回，防止除零崩溃。

### Requirement: c2_c.c ATK颜色代码
`c2_ann_labels` 前两个条目的第一个元素（ATK颜色/点）必须从空字符串改为 `"106"` 以匹配Python版本。

### Requirement: pwm_c.c 微秒格式修正
周期格式化字符串中 `"%.1f \xce\xbc s"` 的μ和s之间多余空格必须移除，改为 `"%.1f\xce\xbcs"`。

### Requirement: ds1307_c.c ADDRESS READ处理
`STATE_GET_SLAVE_ADDR` 状态必须同时处理 `ADDRESS WRITE` 和 `ADDRESS READ`，匹配Python版本行为。

### Requirement: ds3231_c.c 非DS3231地址警告
地址不匹配时必须输出警告注解，而非静默忽略。

### Requirement: ds3231_c.c 注解ID连字符
注解ID中的下划线必须改为连字符（如 `reg_date_time` → `reg-date-time`），匹配Python版本。

### Requirement: 多个解码器补充idn字段
以下解码器的通道/选项idn字段必须补充：
- lpc_c.c: 13个通道的idn
- counter_c.c: 通道idn
- seven_segment_c.c: 通道和选项idn
- pwm_c.c: 通道idn
- can_fd_c.c: 通道和选项idn
