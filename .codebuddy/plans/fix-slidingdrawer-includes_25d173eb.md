---
name: fix-slidingdrawer-includes
overview: 修复 slidingdrawer.cpp 编译错误：添加缺失的 QPropertyAnimation 和 QEasingCurve 头文件包含
todos:
  - id: add-missing-includes
    content: "在 slidingdrawer.cpp 添加 #include QPropertyAnimation 和 QEasingCurve 头文件"
    status: completed
---

## 用户需求

修复 `slidingdrawer.cpp` 的编译错误，该文件使用了 `QPropertyAnimation` 和 `QEasingCurve` 但未包含对应头文件，导致编译失败。

## 核心问题

- `QPropertyAnimation` 未声明（缺少 `#include <QPropertyAnimation>`）
- `QEasingCurve` 为不完整类型（缺少 `#include <QEasingCurve>`）

## 修改方案

在 `DSView/pv/widgets/slidingdrawer.cpp` 的 include 区域添加两个缺失的 Qt 头文件：

- `#include <QPropertyAnimation>` — 提供 QPropertyAnimation 类定义
- `#include <QEasingCurve>` — 提供 QEasingCurve 完整类型定义

## 修改文件

- `DSView/pv/widgets/slidingdrawer.cpp` [MODIFY] — 在第 27 行 `#include <QScreen>` 之后添加两个 include