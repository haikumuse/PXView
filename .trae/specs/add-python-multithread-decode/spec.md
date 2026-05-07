# Python解码器多线程并行 Spec

## Why
当前所有DecodeTrace解码任务在同一个`std::thread`中串行执行（`decode_task_proc`的while循环），即使多个解码器处理完全不同的信号通道也无法并行。需要将多个独立的Python解码任务改为多线程并行执行，加快整体解码速度。

## What Changes
- 修改`SigSession`的解码任务调度机制，从单线程串行改为多线程并行
- 为每个独立的`DecodeTrace`解码任务创建独立线程（或使用线程池）
- 增加共享数据（LogicSnapshot读取、RowData写入）的线程安全保护
- 保留解码器删除/停止/清理的正确性

## Impact
- Affected code:
  - `DSView/pv/sigsession.h` — 线程变量声明改为多线程
  - `DSView/pv/sigsession.cpp` — `add_decode_task()`、`decode_task_proc()`、`remove_decode_task()`、`clear_all_decode_task()`、`get_top_decode_task()` 重写
  - `DSView/pv/data/decode/rowdata.h/cpp` — `push_annotation()` 需加锁保护
  - `DSView/pv/data/decoderstack.cpp` — `annotation_callback()` 需加锁保护
  - `DSView/pv/data/logicsnapshot.h/cpp` — `get_samples()` 读取线程安全性确认

## ADDED Requirements

### Requirement: 多线程解码任务调度
系统应当支持多个DecodeTrace解码任务并行执行，而非串行排队。

#### Scenario: 添加多个独立解码器时并行执行
- **WHEN** 用户依次添加3个解码器（如SPI、I2C、UART）到不同的信号通道
- **THEN** 3个解码任务应分别在不同线程中并行启动执行，而非等第一个完成后再执行第二个

#### Scenario: 单个解码器添加时正常执行
- **WHEN** 用户只添加1个解码器
- **THEN** 该解码器在独立线程中正常执行，行为与改动前一致

### Requirement: 线程安全的注解写入
多个解码线程并发调用`annotation_callback`时，对`RowData::push_annotation()`的写入必须线程安全。

#### Scenario: 多线程并发写入注解
- **WHEN** 两个解码线程同时产生注解结果并调用`annotation_callback`
- **THEN** 注解数据应正确写入各自的`RowData`，不发生数据竞争或崩溃

### Requirement: 解码任务停止与清理
在多线程环境下，停止/删除解码器必须正确终止对应线程并清理资源。

#### Scenario: 停止正在并行解码的某个解码器
- **WHEN** 3个解码器正在并行解码，用户删除其中1个
- **THEN** 被删除的解码器线程应正确终止，其余2个解码器不受影响继续运行

#### Scenario: 清除所有解码器
- **WHEN** 用户点击"清除所有解码器"
- **THEN** 所有正在运行的解码线程应全部终止，资源正确释放

### Requirement: Python GIL兼容性
多线程并行执行Python解码器时，必须正确处理Python GIL。

#### Scenario: 多个Python解码器并行运行
- **WHEN** 多个Python解码器在不同线程中并行执行
- **THEN** 每个线程在调用`libsigrokdecode` API时正确获取/释放GIL，不发生死锁

### Requirement: UI进度通知
多线程解码时，每个解码任务的进度应独立更新到UI。

#### Scenario: 多个解码器并行解码时的进度显示
- **WHEN** 3个解码器并行解码
- **THEN** 每个解码器的进度条独立更新，UI不卡顿

## MODIFIED Requirements

### Requirement: 解码任务调度
原要求：所有DecodeTrace在单个`_decode_thread`中串行执行。
修改为：每个DecodeTrace在独立线程中并行执行，使用线程池管理线程生命周期。

### Requirement: 解码任务队列管理
原要求：`_decode_tasks`队列由`_decode_task_mutex`保护，单线程消费。
修改为：`_decode_tasks`队列仍由`_decode_task_mutex`保护，但每个任务启动独立线程执行。需要新增线程跟踪机制（如`std::vector<std::thread>`或线程池），以便在停止/清理时正确join线程。
