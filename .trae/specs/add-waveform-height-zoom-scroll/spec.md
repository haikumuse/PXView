# 波形高度连续缩放与垂直滚动 Spec

## Why
当前 LOGIC 模式下波形高度仅支持 1X/2X/3X/4X/5X 五个离散档位，且所有信号被强制压缩在同一窗口内显示，无法放大查看细节。用户需要更灵活的连续缩放控制和垂直滚动能力，以及单独调整某个波形高度的能力。

## What Changes
- 将 Ctrl+鼠标滚轮 映射为波形高度缩放（替代原来的水平缩放），实现连续无级调整
- 启用垂直滚动条，当波形总高度超出窗口时允许垂直滚动浏览
- 支持拖拽信号边界线单独调整某个波形的高度
- 保留原有的 1X-5X 设置作为预设快捷选项，与 Ctrl+Wheel 互不冲突

## Impact
- Affected code: `view.h/view.cpp`, `viewport.h/viewport.cpp`, `header.h/header.cpp`, `trace.h/trace.cpp`
- Affected behavior: LOGIC 模式下的滚轮交互、信号高度计算、垂直滚动

## ADDED Requirements

### Requirement: Ctrl+Wheel 连续调整波形高度
系统 SHALL 在 LOGIC 模式下，当用户按下 Ctrl 并滚动鼠标滚轮时，连续调整所有信号的高度，而非执行水平缩放。

#### Scenario: Ctrl+Wheel 向上滚动增大高度
- **WHEN** 用户在波形区域按下 Ctrl 并向上滚动鼠标滚轮
- **THEN** 所有 LOGIC 信号的高度按固定步长（10像素）增大，最大不超过 MaxSignalHeight（500像素）

#### Scenario: Ctrl+Wheel 向下滚动减小高度
- **WHEN** 用户在波形区域按下 Ctrl 并向下滚动鼠标滚轮
- **THEN** 所有 LOGIC 信号的高度按固定步长（10像素）减小，最小不低于 MinSignalHeight（10像素）

#### Scenario: 非 Ctrl 时保持原有行为
- **WHEN** 用户在波形区域滚动鼠标滚轮（不按 Ctrl）
- **THEN** 执行原有的水平缩放/平移行为，不受影响

### Requirement: 垂直滚动条
系统 SHALL 在波形总高度超出可视区域时启用垂直滚动条，允许用户垂直滚动浏览所有信号。

#### Scenario: 波形总高度超出窗口
- **WHEN** 信号高度调整后总高度大于可视区域高度
- **THEN** 垂直滚动条自动出现，范围覆盖所有信号

#### Scenario: 波形总高度未超出窗口
- **WHEN** 信号高度调整后总高度小于等于可视区域高度
- **THEN** 垂直滚动条范围为 0，不显示或禁用

#### Scenario: 拖动垂直滚动条
- **WHEN** 用户拖动垂直滚动条
- **THEN** 波形区域和 Header 标签区域同步垂直滚动

### Requirement: 拖拽单独调整某个波形高度
系统 SHALL 支持用户通过拖拽信号之间的边界线来单独调整某个波形的高度。

#### Scenario: 鼠标悬停在信号边界线上
- **WHEN** 鼠标移动到两个信号之间的边界线附近（5像素容差）
- **THEN** 鼠标光标变为 Qt::SplitVCursor（上下调整大小样式）

#### Scenario: 拖拽边界线调整高度
- **WHEN** 用户按住左键拖拽信号边界线
- **THEN** 上方信号高度随鼠标移动增减，下方信号高度反向变化，总高度保持不变

#### Scenario: 高度限制
- **WHEN** 拖拽导致某个信号高度低于 MinSignalHeight（10像素）
- **THEN** 该信号高度被限制在 MinSignalHeight，不再继续缩小

#### Scenario: 双击边界线重置高度
- **WHEN** 用户双击信号边界线
- **THEN** 该信号恢复为全局统一高度（清除独立高度设置）

### Requirement: 垂直偏移坐标一致性
系统 SHALL 确保引入垂直偏移后，绘制、鼠标交互、命中测试的坐标体系统一。

#### Scenario: 鼠标点击信号区域
- **WHEN** 用户在垂直滚动后点击某个信号
- **THEN** 命中测试正确识别该信号，交互位置与视觉位置一致

#### Scenario: Header 标签同步
- **WHEN** 垂直滚动偏移发生变化
- **THEN** Header 中的通道标签与波形同步移动

## MODIFIED Requirements

### Requirement: LOGIC 模式信号高度计算
原逻辑：从 `SR_CONF_MAX_HEIGHT_VALUE` 读取离散档位（1X-5X），信号高度被 max_height 上限封顶，所有信号强制在窗口内显示。

修改为：信号高度由用户通过 Ctrl+Wheel 或拖拽独立设定，不再强制封顶。每个 Trace 可拥有独立的 `_ownHeight`，未单独设置时使用全局 `_signalHeight`。当总高度超出窗口时启用垂直滚动。

### Requirement: View::update_scroll() 垂直滚动条
原逻辑：`verticalScrollBar()->setRange(0, 0)` 禁用垂直滚动条。

修改为：根据信号总高度与可视区域高度的差值设置垂直滚动条范围，支持垂直滚动。

### Requirement: Viewport::wheelEvent() 滚轮事件
原逻辑：垂直滚轮 = 水平缩放，水平滚轮 = 左右平移。

修改为：Ctrl+垂直滚轮 = 波形高度缩放，无 Ctrl 时保持原有行为。

## REMOVED Requirements

### Requirement: SR_CONF_MAX_HEIGHT_VALUE 作为高度上限
**Reason**: 改为连续缩放后不再需要离散档位作为上限。但 1X-5X 预设选项保留在设备选项对话框中作为快捷预设。
**Migration**: 1X-5X 预设改为设置全局 `_signalHeight` 的快捷方式，而非驱动层配置上限。
