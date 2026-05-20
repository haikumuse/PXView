# Stream 模式磁盘缓存（分层存储）Spec

## Why
当前 Stream 模式下逻辑分析仪的采集数据完全缓存在内存中，缓存深度受限于物理内存大小（默认 16GB）。在 2Gbps（250MB/s）带宽下，16GB 内存仅能缓存约 0.64 秒的数据。通过引入"热数据内存 + 冷数据磁盘"的分层存储架构，可将缓存深度从秒级提升到分钟级（12-128 倍），大幅扩展 Stream 模式的可用采集时长。

## What Changes
- 新增 `DiskBufferManager` 类：管理磁盘文件的创建、写入、读取、环形覆盖和清理
- 新增 `DiskWriteThread` 类：后台异步写入线程，将溢出叶块从内存刷写到磁盘
- 新增 `DiskReadCache` 类：LRU 读缓存，回看历史数据时从磁盘按需加载叶块到内存
- 修改 `LogicSnapshot`：在叶块分配/释放/读取路径中集成磁盘缓存逻辑
- 修改 `Snapshot` 基类：新增磁盘缓存模式相关状态标志
- 修改 `SigSession`：采集启动时初始化磁盘缓存，采集结束时刷写并关闭
- 修改 `PX_context` / `pxlogic.c`：新增磁盘缓存配置项（路径、大小上限）
- 修改 `SR_CONF_STREAM_BUFF` 语义：从纯内存大小变为"内存+磁盘"总缓存大小
- 新增 `SR_CONF_DISK_CACHE_PATH` 配置项：磁盘缓存文件存储路径
- 新增 `SR_CONF_DISK_CACHE_ENABLE` 配置项：是否启用磁盘缓存
- 修改 `deviceoptions.cpp`：UI 支持磁盘缓存路径选择和启用开关
- **BREAKING** `SR_CONF_STREAM_BUFF` 的含义从"内存缓冲区大小"变为"总缓存深度（内存+磁盘）"

## Impact
- Affected specs: Stream 模式数据采集、循环采集、数据回看、协议解码
- Affected code:
  - `PXView/pv/data/logicsnapshot.h` / `logicsnapshot.cpp` — 核心数据结构改造
  - `PXView/pv/data/snapshot.h` / `snapshot.cpp` — 基类扩展
  - `PXView/pv/sigsession.h` / `sigsession.cpp` — 会话管理集成
  - `libsigrok/hardware/pxlogic/pxlogic.h` / `pxlogic.c` — 硬件驱动配置
  - `libsigrok/libsigrok.h` — 新增配置项常量
  - `libsigrok/hwdriver.c` — 配置项注册
  - `PXView/pv/prop/binding/deviceoptions.cpp` — UI 绑定
  - `PXView/pv/data/analogsnapshot.h` / `analogsnapshot.cpp` — 模拟信号可选支持（Phase 2）

## ADDED Requirements

### Requirement: 磁盘缓存管理器（DiskBufferManager）
系统 SHALL 提供 `DiskBufferManager` 类，负责管理磁盘上的缓存文件。

#### Scenario: 创建缓存文件
- **WHEN** Stream 模式采集启动且磁盘缓存启用
- **THEN** 系统在指定路径下创建每通道一个缓存文件（如 `ch_0.bin`, `ch_1.bin`），以及一个索引文件 `index.bin`
- **AND** 缓存文件采用预分配策略，初始大小为 0，随写入动态扩展

#### Scenario: 磁盘空间不足
- **WHEN** 可用磁盘空间低于配置的缓存大小上限的 10%
- **THEN** 系统停止向磁盘写入新数据，但继续在内存中采集
- **AND** 通过回调通知 UI 显示磁盘空间不足警告

#### Scenario: 清理缓存文件
- **WHEN** 采集结束或应用退出
- **THEN** 系统删除所有临时缓存文件
- **AND** 如果用户选择保留数据，则将缓存文件转换为标准 .pxl 格式

### Requirement: 异步磁盘写入线程（DiskWriteThread）
系统 SHALL 提供后台异步写入线程，将溢出内存的叶块数据写入磁盘。

#### Scenario: 叶块溢出写入
- **WHEN** 内存中的叶块完成 Mip-map 计算且该叶块属于"冷数据"区域（不在当前查看窗口内）
- **THEN** 系统将该叶块的完整数据（含 Mip-map 索引，共 LeafBlockSpace = ~2MB）异步写入磁盘文件
- **AND** 写入完成后，该叶块的内存可被环形缓冲区复用

#### Scenario: 写入速度跟不上采集速度
- **WHEN** 异步写入队列积压超过阈值（默认 64 个叶块，约 128MB）
- **THEN** 系统在状态栏显示写入延迟警告
- **AND** 如果队列积压超过 256 个叶块（约 512MB），系统暂停采集并提示用户

#### Scenario: 采集结束刷写
- **WHEN** 采集结束（用户停止或达到采样限制）
- **THEN** 系统等待所有待写入叶块刷写到磁盘完成后才标记采集结束

### Requirement: 磁盘读缓存（DiskReadCache）
系统 SHALL 提供 LRU 读缓存，支持从磁盘按需加载历史叶块数据。

#### Scenario: 回看历史数据
- **WHEN** 用户滚动查看不在内存中的历史数据区域
- **THEN** 系统从磁盘加载对应叶块到 LRU 缓存中
- **AND** 加载延迟应不超过 10ms（单叶块 2MB 读取）

#### Scenario: LRU 缓存淘汰
- **WHEN** LRU 缓存达到容量上限（默认 256MB）
- **THEN** 系统淘汰最久未访问的叶块，释放其内存

#### Scenario: 缓存命中
- **WHEN** 请求的叶块已在 LRU 缓存中
- **THEN** 直接返回内存指针，无磁盘 I/O

### Requirement: 分层存储 LogicSnapshot 改造
系统 SHALL 改造 `LogicSnapshot` 类，使其支持"热数据内存 + 冷数据磁盘"的分层存储。

#### Scenario: 叶块状态管理
- **WHEN** 叶块被写入数据
- **THEN** 叶块处于以下状态之一：
  - `HOT`：在内存中，属于当前查看窗口，不可换出到磁盘
  - `WARM`：在内存中，已完成 Mip-map 计算，等待写入磁盘
  - `COLD`：已写入磁盘，内存已释放，需要时从磁盘加载
- **AND** 状态转换路径为 `HOT → WARM → COLD`，`COLD` 状态的叶块被访问时加载回 `HOT`

#### Scenario: 热数据窗口大小
- **WHEN** 磁盘缓存模式启用
- **THEN** 内存中始终保留最近 N 个叶块作为热数据窗口（N 由内存缓冲区大小决定，默认约 1-4GB 对应的叶块数）
- **AND** 超出热数据窗口的叶块标记为 WARM 并加入异步写入队列

#### Scenario: 环形缓冲区与磁盘覆盖
- **WHEN** 循环模式下环形缓冲区回绕
- **THEN** 内存中的叶块按现有逻辑复用
- **AND** 磁盘上对应的旧数据区域标记为可覆盖

#### Scenario: malloc 失败时的降级
- **WHEN** 内存分配失败且磁盘缓存启用
- **THEN** 系统尝试释放 WARM/COLD 状态的叶块内存后重试分配
- **AND** 如果仍然失败，设置 `_memory_failed` 标志

### Requirement: 磁盘缓存配置项
系统 SHALL 提供以下配置项控制磁盘缓存行为。

#### Scenario: 启用/禁用磁盘缓存
- **WHEN** 用户在设备选项中切换磁盘缓存开关
- **THEN** 系统启用或禁用磁盘缓存功能
- **AND** 禁用时行为与当前完全一致（纯内存模式）

#### Scenario: 配置缓存路径
- **WHEN** 用户设置磁盘缓存路径
- **THEN** 系统验证路径可写且有足够空间
- **AND** 默认路径为系统临时目录（Windows: `%TEMP%/PXView_cache/`, Linux: `/tmp/PXView_cache/`）

#### Scenario: 配置总缓存深度
- **WHEN** 用户修改 `SR_CONF_STREAM_BUFF` 值
- **THEN** 该值表示"内存 + 磁盘"的总缓存深度
- **AND** 内存部分由系统根据物理内存自动分配（默认为物理内存的 25%，上限 4GB）
- **AND** 磁盘部分 = 总缓存深度 - 内存部分

### Requirement: 数据读取路径透明化
系统 SHALL 使磁盘缓存对数据读取路径透明。

#### Scenario: get_samples 读取
- **WHEN** `LogicSnapshot::get_samples()` 请求的叶块处于 COLD 状态
- **THEN** 系统从磁盘加载该叶块到读缓存后返回数据指针
- **AND** 加载期间不阻塞其他通道的读取

#### Scenario: get_display_edges 读取
- **WHEN** `LogicSnapshot::get_display_edges()` 需要访问多个叶块
- **THEN** 系统批量预加载所需叶块，减少磁盘 I/O 次数

#### Scenario: 协议解码器读取
- **WHEN** 协议解码器通过 `get_samples()` 顺序读取数据
- **THEN** 系统预读下一个叶块到缓存，隐藏磁盘延迟

### Requirement: 采集性能保证
系统 SHALL 保证磁盘缓存不影响实时采集性能。

#### Scenario: 250MB/s 持续采集
- **WHEN** 在 NVMe SSD 上以 250MB/s 持续采集
- **THEN** USB 数据接收回调中不执行任何磁盘 I/O 操作
- **AND** 数据写入内存的延迟与纯内存模式一致（< 1μs/8字节）
- **AND** Mip-map 计算延迟与纯内存模式一致

#### Scenario: SATA SSD 采集
- **WHEN** 在 SATA SSD 上以 250MB/s 持续采集
- **THEN** 系统正常工作，但写入队列可能偶尔积压
- **AND** 内存缓冲区吸收突发延迟（默认 1-4GB 缓冲）

#### Scenario: HDD 或写入速度不足
- **WHEN** 检测到磁盘写入速度低于 200MB/s
- **THEN** 系统在启动采集时警告用户磁盘性能不足
- **AND** 建议用户切换到 NVMe SSD 或降低采样率

## MODIFIED Requirements

### Requirement: Stream 模式缓冲区大小配置
原要求：`SR_CONF_STREAM_BUFF` 配置纯内存缓冲区大小（1-128GB）。

修改为：`SR_CONF_STREAM_BUFF` 配置总缓存深度（内存+磁盘），范围 1-1024GB。内存部分由系统自动管理，磁盘部分为总深度减去内存部分。当磁盘缓存禁用时，行为与原来一致。

### Requirement: LogicSnapshot 环形缓冲区
原要求：环形缓冲区通过 `move_first_node_to_last()` 在内存中移动根节点实现循环。

修改为：环形缓冲区在内存中保持不变，磁盘上的旧数据区域通过索引标记为可覆盖。`move_first_node_to_last()` 逻辑保持不变（仅操作内存中的热数据窗口），但不再释放已写入磁盘的叶块内存（因为内存中可能已经没有这些叶块）。

### Requirement: LogicSnapshot 叶块分配
原要求：叶块通过 `malloc(LeafBlockSpace)` 分配，失败时设置 `_memory_failed`。

修改为：叶块分配优先从 WARM/COLD 状态的叶块中复用内存；如果无可用叶块则 `malloc`；`malloc` 失败时尝试释放读缓存中的叶块后重试；全部失败才设置 `_memory_failed`。

## REMOVED Requirements

### Requirement: 无
本次变更不移除任何现有功能。磁盘缓存为可选功能，禁用时所有行为与原来完全一致。
