# 修复5个C解码器annotation数量差异 Spec

## Why
5个C解码器与对应Python解码器相比存在annotation数量差异，根本原因各不相同但都导致解码结果不正确。需要逐一修复使C解码器输出与Python版本一致的annotation数量。

## What Changes
- 修复 nrzi_c.c：STATE_DECODE中每个bit annotation覆盖2个symbol_len而非1个，导致annotation数量减半
- 修复 opentherm_c.c：IDLE状态缺少"Sync error: silence too short"annotation输出（handle_timing_error未设置last_frame_edge）
- 修复 sent_c.c：主循环每次迭代消耗2个下降沿而非1个，导致annotation数量减半
- 修复 timing_c.c：format==0（full模式）时错误输出TERSE annotation，导致annotation数量偏多
- 修复 morse_c.c：process_symbol()中错误输出SYMBOL annotation，导致annotation数量偏多

## Impact
- Affected code: `libsigrokdecode/c_decoders/` 下5个C解码器
- 无破坏性变更

## ADDED Requirements

### Requirement: nrzi_c bit annotation范围必须覆盖1个symbol_len
C解码器STATE_DECODE中每个bit annotation必须覆盖恰好1个symbol_len，与Python版本一致。当前实现覆盖2个symbol_len，导致每2个symbol周期只输出1个annotation，annotation总数约为Python的一半。

#### Scenario: bit annotation范围正确
- **WHEN** C解码器处理NRZ-I信号
- **THEN** 每个bit annotation覆盖恰好1个symbol_len，annotation总数与Python版本一致

### Requirement: opentherm_c必须在timing error后设置last_frame_edge
C解码器的handle_timing_error()必须设置last_frame_edge，与Python版本一致。当前实现未设置，导致IDLE状态的"silence too short"检查条件永远不满足，缺少sync error annotation输出。

#### Scenario: timing error后IDLE状态能检测silence过短
- **WHEN** C解码器发生timing error后进入IDLE状态
- **THEN** last_frame_edge已被设置，IDLE状态能正确检测silence过短并输出sync error annotation

### Requirement: sent_c主循环每次迭代只消耗1个下降沿
C解码器主循环必须与Python版本一致，每次迭代只消耗1个下降沿。当前实现每次迭代消耗2个下降沿（第一个设last_samplenum，第二个测period），导致annotation数量减半。

#### Scenario: 每次迭代消耗1个下降沿
- **WHEN** C解码器处理SENT信号
- **THEN** 每次循环迭代只等待1个下降沿，用上一次迭代的位置作为last_samplenum

### Requirement: timing_c full模式不输出TERSE annotation
C解码器format==0（full模式）时只输出ANN_TIME annotation，不输出ANN_TERSE。TERSE annotation仅在非full格式时输出，与Python版本一致。

#### Scenario: full模式只输出TIME annotation
- **WHEN** C解码器format选项为full
- **THEN** 每个边沿只输出ANN_TIME和ANN_AVG（及可选ANN_DELTA），不输出ANN_TERSE

### Requirement: morse_c process_symbol不输出SYMBOL annotation
C解码器process_symbol()函数不应输出ANN_SYMBOL annotation。SYMBOL级别的信息由更高层的flush_letter/flush_word处理。process_symbol()只负责跟踪符号序列，与Python版本的decode_symbols()一致（只输出TIME和UNITS）。

#### Scenario: process_symbol不输出SYMBOL annotation
- **WHEN** C解码器处理morse信号
- **THEN** process_symbol()只更新内部序列状态，不输出ANN_SYMBOL annotation

## MODIFIED Requirements
无

## REMOVED Requirements
无
