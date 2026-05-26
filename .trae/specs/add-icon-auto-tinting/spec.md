# 图标自动着色映射 Spec

## Why
当前大量 SVG 图标（如 next.svg、add.svg、close.svg 等）的颜色在文件内部硬编码，不响应主题 token 变化。用户在样式面板修改颜色后，这些图标仍显示原始硬编码颜色，导致主题系统不完整。

## What Changes
- 在 `IconCache::icon()` 中新增"文件名 → 主题 token"的自动着色映射表，加载图标时自动根据映射表调用 `tintedIcon()` 着色
- 在 `theme-schema.json` 的 `accent.colors` 分类中新增两个 token：`@icon-accent`（强调色图标颜色）和 `@icon-foreground`（前景色图标颜色）
- 在所有主题 JSON 文件中添加这两个 token 的默认值
- 在语言文件（cn/en/traditional）中添加对应的 IDS 字符串

## Impact
- Affected specs: accent.colors token 体系
- Affected code: `iconcache.cpp`、`theme-schema.json`、`dark.json`、`light.json`、`atom.json`、`monokai.json`、语言文件

## ADDED Requirements

### Requirement: 图标自动着色映射
系统 SHALL 在 `IconCache::icon()` 中维护一个静态映射表，将 SVG 文件名映射到主题 token。当加载图标时，若文件名在映射表中，则自动使用对应 token 的颜色调用 `tintedIcon()` 着色；若不在映射表中，则保持原色。

#### Scenario: 强调色图标自动着色
- **WHEN** 用户加载 `next.svg`、`add.svg`、`gear.svg` 等强调色图标
- **THEN** 系统自动使用 `@icon-accent` token 的颜色对图标进行 `CompositionMode_SourceIn` 着色

#### Scenario: 前景色图标自动着色
- **WHEN** 用户加载 `close.svg`、`minimize.svg`、`search.svg` 等前景色图标
- **THEN** 系统自动使用 `@icon-foreground` token 的颜色对图标进行 `CompositionMode_SourceIn` 着色

#### Scenario: 无映射图标保持原色
- **WHEN** 用户加载 `start.svg`、`dsl_logo.svg` 等品牌/多色图标
- **THEN** 系统不进行着色，保持 SVG 原始颜色

#### Scenario: token 值无效时回退
- **WHEN** 映射的 token 值为空或无效
- **THEN** 系统回退到 SVG 原始颜色，不进行着色

### Requirement: 新增 @icon-accent 和 @icon-foreground 主题 token
系统 SHALL 在 `accent.colors` 分类中新增两个 color 类型 token：

| Token ID | 用途 | dark 默认值 | light 默认值 |
|----------|------|------------|-------------|
| `@icon-accent` | 强调色图标颜色（原 `#1E90FF` 蓝色图标） | `#1E90FF` | `#1E90FF` |
| `@icon-foreground` | 前景色图标颜色（窗口控件、搜索等） | `#E0E0E0` | `#424242` |

#### Scenario: 用户在样式面板修改图标颜色
- **WHEN** 用户在样式面板修改 `@icon-accent` 或 `@icon-foreground` 的值
- **THEN** 所有对应类别的图标在实时预览和确认后更新为新颜色

### Requirement: 映射表覆盖的图标清单

**Category A — 强调色图标（映射到 `@icon-accent`）：**
next, add, gear, del, open, shown, hidden, save, nav, pre, search, trigger, capture, dark, light, measure, about, bug, display, export, fft, file, function, log, manual, math, once, params, protocol, repeat, search-bar, settings, sliders, ruler, binary, step-forward, scroll-bottom, logo_noColor, loop, update, modes

**Category B — 前景色图标（映射到 `@icon-foreground`）：**
close, minimize, maximize, restore, pin, unpin, search (Feather 风格), stop, play, zap, audio-waveform, header-expand, moder, single

**排除（Category C — 不着色）：**
start, dsl_logo, win_title_logo, demo, data, usb2, Chinese, English, logo, logo_color, math (根目录), lissajous, square-la, square-osc, square-daq, caret-down-square

#### Scenario: 映射表查找
- **WHEN** `IconCache::icon()` 被调用时
- **THEN** 从路径中提取文件名（不含目录前缀），在映射表中查找对应的 token

## MODIFIED Requirements

### Requirement: IconCache::icon() 行为变更
原有 `IconCache::icon()` 直接加载 SVG 不做任何着色。修改后，`icon()` 方法 SHALL 先检查文件名是否在映射表中，若在则自动着色。`tintedIcon()` 方法保持不变，仍可用于显式指定颜色的场景。

### Requirement: 主题切换时缓存失效
已有 `IconCache::clearCache()` 在 `UI_UPDATE_ACTION_THEME` 时被调用。由于 `icon()` 现在会根据当前主题 token 着色，缓存失效后重新加载时将自动使用新 token 值，无需额外处理。
