# 分离解码器通道标签色与注释六边形颜色 Spec

## Why
当前主题系统中，`@decoder-channel-0~15` 这组 token 同时用于解码器通道标签（左侧标签栏的圆角矩形和五边形箭头）和注释六边形（主视口区域的协议数据块）。两者视觉职责不同——标签需要高辨识度标识身份，注释需要柔和色调便于长时间阅读——却共用同一组颜色令牌，无法独立调节。此外，`OutlineColours[16]`（注释边框色）是硬编码的，不受主题系统控制。

## What Changes
- 新增 `@decoder-ann-0~15` 共16个注释填充色 token，专用于注释六边形绘制
- 新增 `@decoder-ann-outline-0~15` 共16个注释边框色 token，替代硬编码的 `OutlineColours[16]`
- `@decoder-channel-0~15` 保留，仅用于解码器通道标签（圆角矩形 + 五边形箭头）
- `theme-schema.json` 新增 `decoder.ann.colors` 分类，包含32个新 token
- `theme.qss` 顶部注释区新增32个 token 声明
- 4个主题 JSON 文件（dark.json、light.json、monokai.json、atom.json）各新增32个 token 值
  - dark.json 和 light.json：注释填充色与原 `@decoder-channel-N` 保持一致，边框色与原 `OutlineColours` 保持一致
  - monokai.json 和 atom.json：自定义注释色和边框色，适配各自风格
- `decodetrace.cpp`：`draw_annotation()` 改用 `getAnnColor()` 和 `getAnnOutlineColor()` 获取注释颜色
- `decodetrace.h`：新增 `getAnnColor(int)` 和 `getAnnOutlineColor(int)` 静态方法，`OutlineColours` 保留作为 fallback

## Impact
- Affected specs: 无
- Affected code:
  - `PXView/pv/view/decodetrace.cpp` — 注释颜色获取逻辑
  - `PXView/pv/view/decodetrace.h` — 新增静态方法声明
  - `PXView/themes/theme-schema.json` — 新增 token 定义
  - `PXView/themes/theme.qss` — 新增 token 注释声明
  - `PXView/themes/dark.json` — 新增 token 值
  - `PXView/themes/light.json` — 新增 token 值
  - `PXView/themes/monokai.json` — 新增 token 值
  - `PXView/themes/atom.json` — 新增 token 值

## ADDED Requirements

### Requirement: 独立的解码器注释填充色
系统 SHALL 提供 `@decoder-ann-0~15` 共16个主题 token，用于控制解码器注释六边形的填充色。当主题中未定义这些 token 时，SHALL 回退到 `@decoder-channel-N` 的值，再回退到 `defaultColours[16]` 硬编码值。

#### Scenario: 主题定义了注释色
- **WHEN** 主题 JSON 中定义了 `@decoder-ann-5`
- **THEN** `getAnnColor(5)` 返回该主题定义的颜色

#### Scenario: 主题未定义注释色
- **WHEN** 主题 JSON 中未定义 `@decoder-ann-5` 但定义了 `@decoder-channel-5`
- **THEN** `getAnnColor(5)` 回退到 `@decoder-channel-5` 的值

#### Scenario: 主题均未定义
- **WHEN** 主题 JSON 中 `@decoder-ann-5` 和 `@decoder-channel-5` 均未定义
- **THEN** `getAnnColor(5)` 回退到 `defaultColours[5]` 硬编码值

### Requirement: 独立的解码器注释边框色
系统 SHALL 提供 `@decoder-ann-outline-0~15` 共16个主题 token，用于控制解码器注释六边形的边框色。当主题中未定义这些 token 时，SHALL 回退到 `OutlineColours[16]` 硬编码值。

#### Scenario: 主题定义了边框色
- **WHEN** 主题 JSON 中定义了 `@decoder-ann-outline-3`
- **THEN** `getAnnOutlineColor(3)` 返回该主题定义的颜色

#### Scenario: 主题未定义边框色
- **WHEN** 主题 JSON 中未定义 `@decoder-ann-outline-3`
- **THEN** `getAnnOutlineColor(3)` 回退到 `OutlineColours[3]` 硬编码值

### Requirement: 通道标签色保持不变
`@decoder-channel-0~15` SHALL 继续用于解码器通道标签（圆角矩形和五边形箭头），行为不变。

### Requirement: 主题 schema 新增分类
`theme-schema.json` SHALL 新增 `decoder.ann.colors` 分类（`IDS_STYLE_CAT_DECODER_ANN`），包含32个新 token 定义。

### Requirement: dark/light 主题注释色兼容
dark.json 和 light.json 的 `@decoder-ann-N` 值 SHALL 与现有 `@decoder-channel-N` 值完全一致，`@decoder-ann-outline-N` 值 SHALL 与现有 `OutlineColours[N]` 硬编码值完全一致，确保视觉无变化。

### Requirement: monokai/atom 主题注释色自定义
monokai.json 和 atom.json 的 `@decoder-ann-N` 和 `@decoder-ann-outline-N` 值 SHALL 根据各自主题风格自定义，注释色比标签色更柔和（降低饱和度或增加透明度），边框色为注释色的暗色版本。
