# 第二次采样 SIGSEGV 崩溃排查报告

## 1. 问题描述

### 现象

在 repeat 模式下，第二次采样完成后程序崩溃，GDB 报告：

```
Thread 48 "1:spi-1" received signal SIGSEGV, Segmentation fault.
0x00007ff7bbb537c7 in update_old_pins_array_initial_pins (di=0x98e94d0)
    at libsigrokdecode/instance.c:1158
1158    sample = *sample_pos & (1 << bit_offset) ? 1 : 0;
```

调用栈：

```
#0  update_old_pins_array_initial_pins (di=0x98e94d0)
#1  find_match (di=0x98e94d0)
#2  process_samples_until_condition_match (di=0x98e94d0, found_match=0xe66f604)
#3  Decoder_wait (self=0x4638050, args=0x41434c0)
#4  ..libpython3.14.dll (Python 解释器)
...
#14 di_thread (data=0x98e94d0)
#15 libglib-2.0-0.dll (GLib 线程)
```

崩溃发生在 Python 解码器线程 (`di_thread`) 中，`di->inbuf[i]` 指向的内存无效。

### 环境

- 提交 `dd2a58b` 引入的架构缺陷（多文档架构）
- 前一次提交 `b218f0e` 不存在此问题
- Windows + MSYS2 + MinGW64

---

## 2. 根因分析

### 2.1 `di->inbuf` 指向什么

`di->inbuf` 的类型是 `const uint8_t**`，每个元素对应一个解码器通道，**直接指向 snapshot 的内部数据缓冲区**，不是数据拷贝。

数据来源追踪：

```
DecoderStack::decode_data()
  → _snapshot->get_samples(i, chunk_end, sig_index, &lbp)
    → 返回 LogicSnapshot 内部 LeafBlock 的裸指针:
       (uint8_t*)_ch_data[order][index0].lbp[index1] + offset
  → chunk.push_back(data_ptr)
  → srd_session_send(session, ..., chunk.data(), ...)
    → srd_inst_decode()
      → di->inbuf = inbuf   // 直接指向 snapshot 内部缓冲区
```

### 2.2 `_owner_document` 路径引入的竞态条件

`dd2a58b` 提交在 `DecoderStack::do_decode_work()` 中新增了通过 `_owner_document` 获取 snapshot 的路径：

```cpp
// dd2a58b 新增代码
if (_owner_document && _owner_document->has_data()) {
    _snapshot = _owner_document->get_active_logic();  // 返回 SessionDocument::_logic
}
```

`SessionDocument::_logic` 是 `SessionData::_logic` 的**深拷贝**，由 `copy_data_to_document()` 创建：

```cpp
void SigSession::copy_data_to_document(SessionDocument *doc) {
    doc->copy_from_logic(_view_data->get_logic());  // 深拷贝，先 free_data() 再 malloc+memcpy
}
```

### 2.3 竞态条件的触发时序

在 `DSV_MSG_REV_END_PACKET` 处理器中：

```
主线程:
  1. add_decode_task(de)           ← 创建解码线程，立即返回
  2. _callback->frame_ended()      ← 同步调用 MainWindow::on_frame_ended()
     → copy_data_to_document()     ← 释放 SessionDocument::_logic 的旧数据！
```

```
解码线程 (di_thread):
  1. do_decode_work()
  2. _owner_document->get_active_logic()  ← 获取 SessionDocument::_logic
  3. _snapshot->get_samples()             ← 获取 _logic 内部 LeafBlock 指针
  4. di->inbuf[i] = chunk[i]              ← 指向 _logic 内部数据
  5. di_thread 访问 di->inbuf[i]          ← 💥 主线程已释放该内存！
```

**时序图：**

```
主线程:    add_decode_task(de) ──────────────────── _callback->frame_ended()
                │                                      │
                │ 创建解码线程                          │ 同步调用 on_frame_ended()
                ▼                                      ▼
解码线程:  do_decode_work()                      copy_data_to_document()
           → get_active_logic()                   → copy_from_logic()
           → 获取 _logic 旧数据指针                → _logic.free_data() ← 释放旧数据!
           → di->inbuf[i] 指向 _logic 内部数据
           → di_thread 访问 di->inbuf[i]
           → 💥 Use-After-Free → SIGSEGV!
```

### 2.4 为什么第一次采样不崩溃

第一次采样时，`SessionDocument::_logic` 是空的（`has_data()` 返回 false），解码器走的是 `LogicSignal::data()` 路径，获取的是 `SessionData` 的 `LogicSnapshot`，该 snapshot 在采样期间不会被释放。

第二次采样时，`SessionDocument::_logic` 已有数据（`has_data()` 返回 true），解码器走 `_owner_document->get_active_logic()` 路径，获取的是 `SessionDocument::_logic` 的副本，该副本会被 `copy_data_to_document()` 释放。

### 2.5 原始工程为什么没有这个问题

| 方面 | 原始工程 | 当前工程（修复前） |
|------|---------|------------------|
| 获取 snapshot | 通过 `LogicSignal::data()` 直接获取 `SessionData` 的 `LogicSnapshot` | 通过 `_owner_document->get_active_logic()` 获取 `SessionDocument::_logic` 副本 |
| 数据生命周期 | `SessionData` 的 `LogicSnapshot` 在采样间不被释放 | `SessionDocument::_logic` 被 `copy_data_to_document()` 释放 |
| 缓冲区交换 | 有 `attach_data_to_signal()` 更新 Signal 指针 | 缺失 `attach_data_to_signal()` |

---

## 3. 修复方案

### 3.1 修复 1：移除 `_owner_document` 路径

**文件**: `PXView/pv/data/decoderstack.cpp`

**修改前**:
```cpp
if (_owner_document && _owner_document->has_data()) {
    _snapshot = _owner_document->get_active_logic();
}

if (_snapshot == NULL) {
    for (auto dec : _stack) {
        if (dec->have_probes()) {
            for(auto s : _session->get_signals()) {
                if(s->get_index() == dec->first_probe_index() && s->signal_type() == SR_CHANNEL_LOGIC)
                {
                    _snapshot = ((pv::view::LogicSignal*)s)->data();
                    if (_snapshot != NULL)
                        break;
                }
            }
            if (_snapshot != NULL)
                break;
        }
    }
}
```

**修改后**:
```cpp
for (auto dec : _stack) {
    if (dec->have_probes()) {
        for(auto s : _session->get_signals()) {
            if(s->get_index() == dec->first_probe_index() && s->signal_type() == SR_CHANNEL_LOGIC)
            {
                _snapshot = ((pv::view::LogicSignal*)s)->data();
                if (_snapshot != NULL)
                    break;
            }
        }
        if (_snapshot != NULL)
            break;
    }
}
```

**原因**: 解码器不再使用 `SessionDocument::_logic`（可能被 `copy_data_to_document()` 释放的副本），改为始终通过 `LogicSignal::data()` 获取 `SessionData` 的 `LogicSnapshot`（与原始工程一致）。`SessionData` 的 `LogicSnapshot` 在第二次采样时不会被释放（只 clear 后重新填充数据）。

### 3.2 修复 2：恢复 `attach_data_to_signal()`

**文件**: `PXView/pv/sigsession.h` 和 `PXView/pv/sigsession.cpp`

原始工程在 `DSV_MSG_REV_END_PACKET` 中缓冲区交换后调用 `attach_data_to_signal(_view_data)`，将新的 `SessionData` 的 snapshot 绑定到 Signal 对象。当前工程删除了这个调用，导致 repeat 模式下 Signal 对象仍引用旧的 snapshot。

**新增函数**:
```cpp
void SigSession::attach_data_to_signal(SessionData *data) {
    if (!data)
        return;

    for (auto sig : _signals) {
        int type = sig->signal_type();
        switch (type) {
        case SR_CHANNEL_LOGIC: {
            view::LogicSignal *s = (view::LogicSignal *)sig;
            s->set_data(data->get_logic());
            break;
        }
        case SR_CHANNEL_ANALOG: {
            view::AnalogSignal *s = (view::AnalogSignal *)sig;
            s->set_data(data->get_analog());
            break;
        }
        case SR_CHANNEL_DSO: {
            view::DsoSignal *s = (view::DsoSignal *)sig;
            s->set_data(data->get_dso());
            break;
        }
        }
    }
}
```

### 3.3 修复 3：调整 `DSV_MSG_REV_END_PACKET` 处理顺序

**文件**: `PXView/pv/sigsession.cpp`

**修改前**:
```cpp
if (bSwapBuffer) {
    _view_data = _capture_data;
    set_session_time(_trig_time);
    // 缺少 attach_data_to_signal()
    _callback->receive_trigger(_view_data->_trig_pos);
    _callback->trigger_message(DSV_MSG_DATA_POOL_CHANGED);
}

for (auto de : decode_traces()) {
    de->decoder()->set_capture_end_flag(true);
    if (bAddDecoder) {
        de->frame_ended();
        add_decode_task(de);    // 解码线程启动
    }
}

_callback->frame_ended();       // 触发 copy_data_to_document()
```

**修改后**:
```cpp
if (bSwapBuffer) {
    _view_data = _capture_data;
    attach_data_to_signal(_view_data);  // 恢复：更新 Signal 数据指针
    set_session_time(_trig_time);
    _callback->receive_trigger(_view_data->_trig_pos);
    _callback->trigger_message(DSV_MSG_DATA_POOL_CHANGED);
}

if (bAddDecoder && _active_document) {
    copy_data_to_document(_active_document);  // 新增：在启动解码线程前拷贝数据
}

for (auto de : decode_traces()) {
    de->decoder()->set_capture_end_flag(true);
    if (bAddDecoder) {
        de->frame_ended();
        add_decode_task(de);
    }
}

_callback->frame_ended();
```

**关键改动**:
1. 缓冲区交换后调用 `attach_data_to_signal()`，确保 Signal 指向新 snapshot
2. 在 `add_decode_task()` 之前调用 `copy_data_to_document()`，确保 `SessionDocument` 已有新数据
3. `on_frame_ended()` 中的 `copy_data_to_document()` 调用变为安全的（数据已是最新，`copy_from_logic()` 内部会先 `free_data()` 再拷贝，但此时解码线程尚未启动或正在使用 `SessionData` 的 snapshot，不受影响）

---

## 4. 修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `PXView/pv/data/decoderstack.cpp` | 移除 `_owner_document->get_active_logic()` 路径，恢复原始的 Signal 方式获取 snapshot |
| `PXView/pv/sigsession.h` | 添加 `attach_data_to_signal()` 声明 |
| `PXView/pv/sigsession.cpp` | 添加 `attach_data_to_signal()` 实现；`DSV_MSG_REV_END_PACKET` 中添加 `attach_data_to_signal()` 和 `copy_data_to_document()` 调用 |

---

## 5. 其他排查过程中发现的问题

以下问题在之前的排查中已修复或记录，但与本次 SIGSEGV 不直接相关：

### 5.1 `srd_inst_find_by_obj` 的 NULL 安全检查和 GRWLock 保护（已修复）

`srd_inst_find_by_obj()` 遍历全局 `sessions` 列表时，在 session 销毁过程中 `sessions->data` 和 `sess->di_list->data` 可能为 NULL。已添加 NULL 检查和 `GRWLock` 读写锁保护。

### 5.2 `srd_session_destroy` 中先从全局列表移除再释放资源（已修复）

`srd_session_destroy()` 必须先从 `sessions` 全局列表移除 session，再释放资源，否则其他线程可能在释放过程中遍历到正在销毁的 session。

### 5.3 `srd_inst_free_all` 三步走（已修复）

先发终止信号 → join 所有线程 → 释放资源，避免在线程运行时释放 decoder instance。

### 5.4 `clear_all_documents_decoders` 恢复 `_delete_flag` 延迟销毁机制（已修复）

### 5.5 Python 解码器从同步模型恢复为 `di_thread` 多线程模型（已修复）

### 5.6 `Decoder_wait` 从单次处理恢复为 `while(1)` + `g_cond_wait` 循环（已修复）

### 5.7 `Decoder_put` 的 `SRD_OUTPUT_PYTHON` 分支中仍有 `srd_ChunkDone_exc` 引用（待清理）

### 5.8 `decode_end()` 在 `_running_tasks_mutex` 锁内调用（潜在死锁风险）

当前项目在 `decode_single_task()` 中持有 `_running_tasks_mutex` 时调用 `_view_data->get_logic()->decode_end()`，如果 `decode_end()` 内部尝试获取同一互斥锁，会导致死锁。原始项目在无锁状态下调用 `decode_end()`。

### 5.9 `_running_tasks.empty()` 无锁读取（数据竞争）

`feed_in_logic()` 中 `bool bNotFree = !_running_tasks.empty()` 读取 `_running_tasks` 没有加锁，而 `decode_single_task()` 中修改 `_running_tasks` 是在 `_running_tasks_mutex` 保护下进行的。原始项目使用 `volatile bool _is_decoding` 标志，在实践中更安全。

---

## 6. 架构建议

### 6.1 解码线程模型

原始工程使用**单线程串行队列**模型（`decode_task_proc`），所有解码任务在一个线程中串行执行。当前工程改为**多线程并行**模型（`decode_single_task`），每个解码器一个线程。

多线程模型增加了复杂度：
- 需要管理多个线程的生命周期
- `decode_end()` 的调用时机更复杂
- `_running_tasks` 的线程安全需要额外保护
- `_delete_flag` 的竞态风险

建议：如果解码器的并行执行不是刚需，考虑恢复单线程串行队列模型，降低并发复杂度。

### 6.2 `SessionDocument` 与解码器的数据路径

`SessionDocument::_logic` 是 `SessionData::_logic` 的深拷贝，用于历史文档的数据保存。解码器应始终使用 `SessionData` 的 `LogicSnapshot`（通过 Signal 获取），而不是 `SessionDocument` 的副本。这避免了 `copy_data_to_document()` 与解码线程之间的竞态条件。

如果未来需要让解码器使用 `SessionDocument` 的数据（例如解码历史文档），需要确保：
1. `copy_data_to_document()` 在解码线程启动前完成
2. 解码线程运行期间 `SessionDocument::_logic` 不被释放或替换
3. 或者使用引用计数/读写锁保护 `SessionDocument::_logic` 的生命周期
