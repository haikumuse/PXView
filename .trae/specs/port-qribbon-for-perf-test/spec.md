# 移植 QRibbon 做帧率对比测试 Spec

## Why
当前 PXView 的 TitleBar (QMenuBar) Ribbon 动画卡顿，需要将旧版 DSView 的 QRibbon 移植到当前项目中做帧率对比测试，确认 QRibbon 在相同 Viewport 下的动画流畅度差异。

## What Changes
- **添加** QRibbon 源文件到当前项目（QRibbon.h、QRibbon.cpp、qribbon.ui）
- **修改** CMakeLists.txt 添加 QRibbon 源文件
- **修改** MainWindow 构造函数，创建 QMenuBar + QMenu，然后安装 QRibbon
- **保留** 当前 TitleBar 代码不变（仅用于对比，不删除）
- **添加** QRibbon 的 install() 调用，替代当前 TitleBar 的 Ribbon 功能

## Impact
- Affected code: CMakeLists.txt、mainwindow.h/cpp、新增 QRibbon 目录
- Affected specs: 无

## ADDED Requirements

### Requirement: QRibbon 移植
系统 SHALL 将旧版 DSView 的 QRibbon 类移植到当前项目中，作为 MainWindow 的 menuBar。

#### Scenario: QRibbon 安装
- **WHEN** MainWindow 构造时
- **THEN** 创建 QMenuBar + QMenu（File/Settings/Help），然后调用 QRibbon::install(this) 将菜单转换为 Ribbon

#### Scenario: 帧率对比
- **WHEN** 用户点击 QRibbon Tab 展开/收起
- **THEN** Viewport 的 paintEvent 日志输出帧率，可与 TitleBar 方案对比

## MODIFIED Requirements

### Requirement: MainWindow 构造
MainWindow 构造函数 SHALL 先创建 QMenuBar 和 QMenu，再安装 QRibbon，替代当前 TitleBar 的 Ribbon 功能。
