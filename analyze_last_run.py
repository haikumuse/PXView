#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import re

log_path = r"C:\Users\admin\AppData\Roaming\PXlogicV20\PXView\DSView.log"

with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
    lines = f.readlines()

# Find the start of the last run
last_start_idx = 0
for idx, line in enumerate(lines):
    if "MainWindow::MainWindow() START" in line:
        last_start_idx = idx

print(f"Total lines: {len(lines)}")
print(f"Last run started at line: {last_start_idx + 1}")
print("Analyzing the last run...")

last_run_lines = lines[last_start_idx:]

paint_signals = []       # (line_no, took_ms, rebuilt, rebuild_time_ms)
dopaint = []             # (line_no, total_ms, phases_dict)
profiler = []            # (line_no, receiver, event_type, took_ms)
generic_slow = []        # (line_no, text, took_ms) - any line with "took XXX ms" where XXX > 10

re_paint_signals = re.compile(r'Viewport::paintSignals took (\d+) ms.*rebuilt: (\d).*rebuild_time: (\d+) ms')
re_dopaint = re.compile(r'Viewport::doPaint took (\d+) ms')
re_dopaint_phases = re.compile(r'init: (\d+).*check: (\d+).*traces: (\d+).*cards: (\d+).*div: (\d+).*back: (\d+).*sig: (\d+).*fore: (\d+)')
re_profiler = re.compile(r'PROFILER Receiver: (.+?), EventType: (\d+), took (\d+) ms')
re_generic_took = re.compile(r'took (\d+) ms')

for idx, line in enumerate(last_run_lines):
    line_no = last_start_idx + idx + 1
    
    # paintSignals
    m = re_paint_signals.search(line)
    if m:
        paint_signals.append((line_no, int(m.group(1)), int(m.group(2)), int(m.group(3))))
        continue
    
    # doPaint
    m = re_dopaint.search(line)
    if m:
        total = int(m.group(1))
        phases = {}
        m2 = re_dopaint_phases.search(line)
        if m2:
            phases = {
                'init': int(m2.group(1)), 'check': int(m2.group(2)),
                'traces': int(m2.group(3)), 'cards': int(m2.group(4)),
                'div': int(m2.group(5)), 'back': int(m2.group(6)),
                'sig': int(m2.group(7)), 'fore': int(m2.group(8))
            }
        dopaint.append((line_no, total, phases))
        continue
    
    # PROFILER
    m = re_profiler.search(line)
    if m:
        profiler.append((line_no, m.group(1), int(m.group(2)), int(m.group(3))))
        continue
        
    # Generic slow operations (> 10ms)
    m = re_generic_took.search(line)
    if m:
        took = int(m.group(1))
        if took > 10:
            generic_slow.append((line_no, line.strip()[:200], took))

# ============ paintSignals Analysis ============
print(f"\n==========================================")
print(f"[paintSignals] 总样本: {len(paint_signals)}")
print(f"==========================================")
if paint_signals:
    times = [x[1] for x in paint_signals]
    times.sort()
    total = sum(times)
    print(f"  总耗时: {total}ms, 平均: {total/len(times):.1f}ms, 最大: {max(times)}ms")
    print(f"  rebuild 次数: {sum(1 for x in paint_signals if x[2])}/{len(paint_signals)}")
    
    slow = sorted(paint_signals, key=lambda x: x[1], reverse=True)[:20]
    print(f"\n  🔴 最慢的20次:")
    for ln, took, rb, rbt in slow:
        print(f"     Line {ln}: {took}ms (rebuilt:{rb}, rebuild_time:{rbt}ms)")

# ============ doPaint Analysis ============
print(f"\n==========================================")
print(f"[doPaint] 总样本: {len(dopaint)}")
print(f"==========================================")
if dopaint:
    times = [x[1] for x in dopaint]
    times.sort()
    total = sum(times)
    print(f"  总耗时: {total}ms, 平均: {total/len(times):.1f}ms, 最大: {max(times)}ms")
    
    slow = sorted(dopaint, key=lambda x: x[1], reverse=True)[:20]
    print(f"\n  🔴 最慢的20次 doPaint 阶段分解:")
    for ln, took, phases in slow:
        if phases:
            print(f"     Line {ln}: {took}ms | check:{phases['check']} sig:{phases['sig']} back:{phases['back']} init:{phases['init']}")
        else:
            print(f"     Line {ln}: {took}ms | (no phase breakdown)")

# ============ PROFILER Analysis ============
print(f"\n==========================================")
print(f"[PROFILER] 总样本: {len(profiler)}, 仅显示 >5ms 的事件")
print(f"==========================================")
slow_profiler = sorted([x for x in profiler if x[3] > 5], key=lambda x: x[3], reverse=True)
for ln, recv, etype, took in slow_profiler[:30]:
    event_names = {2: 'MousePress', 3: 'MouseRelease', 5: 'MouseMove', 12: 'Paint', 
                   6: 'KeyPress', 7: 'KeyRelease', 77: 'UpdateRequest'}
    ename = event_names.get(etype, f'Event{etype}')
    print(f"  Line {ln}: {took}ms | {ename} | {recv[:80]}")

# ============ Generic Slow Operations ============
print(f"\n==========================================")
print(f"[通用慢操作] >10ms 的所有操作")
print(f"==========================================")
slow_gen = sorted(generic_slow, key=lambda x: x[2], reverse=True)[:30]
for ln, text, took in slow_gen:
    print(f"  Line {ln}: {took}ms | {text[:140]}")
