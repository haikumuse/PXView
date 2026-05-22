#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Analyze DSView.log for performance bottlenecks - comprehensive version"""
import re, sys, os

log_path = r"C:\Users\admin\AppData\Roaming\PXlogicV20\PXView\DSView.log"

# Containers
paint_signals = []       # (line_no, took_ms, rebuilt, rebuild_time_ms)
dopaint = []             # (line_no, total_ms, phases_dict)
profiler = []            # (line_no, receiver, event_type, took_ms)
signals_changed = []     # (line_no, took_ms)
rebuild_signals = []     # (line_no, text)
check_update = []        # (line_no, took_ms)
generic_slow = []        # (line_no, text, took_ms) - any line with "took XXX ms" where XXX > 10

# Regex patterns
re_paint_signals = re.compile(r'Viewport::paintSignals took (\d+) ms.*rebuilt: (\d).*rebuild_time: (\d+) ms')
re_dopaint = re.compile(r'Viewport::doPaint took (\d+) ms')
re_dopaint_phases = re.compile(r'init: (\d+).*check: (\d+).*traces: (\d+).*cards: (\d+).*div: (\d+).*back: (\d+).*sig: (\d+).*fore: (\d+)')
re_profiler = re.compile(r'PROFILER Receiver: (.+?), EventType: (\d+), took (\d+) ms')
re_signals_changed = re.compile(r'signals_changed.*took (\d+) ms')
re_check_update = re.compile(r'check_update.*took (\d+) ms')
re_generic_took = re.compile(r'took (\d+) ms')

print("=" * 70)
print("DSView.log 性能分析")
print("=" * 70)

with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
    for line_no, line in enumerate(f, 1):
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
        
        # signals_changed
        m = re_signals_changed.search(line)
        if m:
            signals_changed.append((line_no, int(m.group(1))))
            continue
        
        # check_update
        m = re_check_update.search(line)
        if m:
            check_update.append((line_no, int(m.group(1))))
            continue
        
        # Generic slow operations (> 10ms)
        m = re_generic_took.search(line)
        if m:
            took = int(m.group(1))
            if took > 10:
                generic_slow.append((line_no, line.strip()[:200], took))

# ============ paintSignals Analysis ============
print(f"\n{'='*70}")
print(f"[paintSignals] 总样本: {len(paint_signals)}")
print(f"{'='*70}")
if paint_signals:
    times = [x[1] for x in paint_signals]
    times.sort()
    total = sum(times)
    print(f"  总耗时: {total}ms, 平均: {total/len(times):.1f}ms, 最大: {max(times)}ms")
    print(f"  P95: {times[int(len(times)*0.95)]}ms, P99: {times[int(len(times)*0.99)]}ms")
    print(f"  rebuild 次数: {sum(1 for x in paint_signals if x[2])}/{len(paint_signals)}")
    
    # Show top 20 slowest
    slow = sorted(paint_signals, key=lambda x: x[1], reverse=True)[:20]
    print(f"\n  🔴 最慢的20次:")
    for ln, took, rb, rbt in slow:
        print(f"     Line {ln}: {took}ms (rebuilt:{rb}, rebuild_time:{rbt}ms)")

# ============ doPaint Analysis ============
print(f"\n{'='*70}")
print(f"[doPaint] 总样本: {len(dopaint)}")
print(f"{'='*70}")
if dopaint:
    times = [x[1] for x in dopaint]
    times.sort()
    total = sum(times)
    print(f"  总耗时: {total}ms, 平均: {total/len(times):.1f}ms, 最大: {max(times)}ms")
    
    # Phase breakdown for slow ones (>10ms)
    slow = sorted(dopaint, key=lambda x: x[1], reverse=True)[:20]
    print(f"\n  🔴 最慢的20次 doPaint 阶段分解:")
    for ln, took, phases in slow:
        if phases:
            print(f"     Line {ln}: {took}ms | check:{phases['check']} sig:{phases['sig']} back:{phases['back']} init:{phases['init']}")
        else:
            print(f"     Line {ln}: {took}ms | (no phase breakdown)")

# ============ PROFILER Analysis ============
print(f"\n{'='*70}")
print(f"[PROFILER] 总样本: {len(profiler)}, 仅显示 >5ms 的事件")
print(f"{'='*70}")
slow_profiler = sorted([x for x in profiler if x[3] > 5], key=lambda x: x[3], reverse=True)
for ln, recv, etype, took in slow_profiler[:30]:
    event_names = {2: 'MousePress', 3: 'MouseRelease', 5: 'MouseMove', 12: 'Paint', 
                   6: 'KeyPress', 7: 'KeyRelease', 77: 'UpdateRequest'}
    ename = event_names.get(etype, f'Event{etype}')
    print(f"  Line {ln}: {took}ms | {ename} | {recv[:80]}")

# ============ check_update Analysis ============
if check_update:
    print(f"\n{'='*70}")
    print(f"[check_update] 总样本: {len(check_update)}")
    print(f"{'='*70}")
    times = [x[1] for x in check_update]
    print(f"  总耗时: {sum(times)}ms, 平均: {sum(times)/len(times):.1f}ms, 最大: {max(times)}ms")
    slow = sorted(check_update, key=lambda x: x[1], reverse=True)[:10]
    for ln, took in slow:
        print(f"  Line {ln}: {took}ms")

# ============ Generic Slow Operations ============
print(f"\n{'='*70}")
print(f"[通用慢操作] >10ms 的所有操作, 总计: {len(generic_slow)}")
print(f"{'='*70}")
slow_gen = sorted(generic_slow, key=lambda x: x[2], reverse=True)[:40]
for ln, text, took in slow_gen:
    print(f"  Line {ln}: {took}ms | {text[:140]}")

# ============ Timeline: find 500ms stall clusters ============
print(f"\n{'='*70}")
print("[时间线分析] 查找连续密集慢操作（可能造成500ms卡顿的区间）")
print(f"{'='*70}")

# Merge all timed events into a single timeline by line number
all_events = []
for ln, took, rb, rbt in paint_signals:
    all_events.append((ln, took, 'paintSignals', f'rebuilt:{rb}'))
for ln, total, phases in dopaint:
    if total > 5:
        sig = phases.get('sig', '?') if phases else '?'
        chk = phases.get('check', '?') if phases else '?'
        all_events.append((ln, total, 'doPaint', f'sig:{sig} check:{chk}'))
for ln, recv, etype, took in profiler:
    if took > 5:
        all_events.append((ln, took, 'PROFILER', f'{recv[:50]} evt:{etype}'))

all_events.sort(key=lambda x: x[0])

# Find clusters of slow events within 200 lines of each other
if all_events:
    clusters = []
    current_cluster = [all_events[0]]
    for ev in all_events[1:]:
        if ev[0] - current_cluster[-1][0] < 200:
            current_cluster.append(ev)
        else:
            if sum(e[1] for e in current_cluster) > 50:
                clusters.append(current_cluster)
            current_cluster = [ev]
    if sum(e[1] for e in current_cluster) > 50:
        clusters.append(current_cluster)
    
    print(f"  找到 {len(clusters)} 个慢操作聚集区域 (总耗时>50ms):")
    for i, cluster in enumerate(clusters[:15]):
        total = sum(e[1] for e in cluster)
        line_range = f"L{cluster[0][0]}-L{cluster[-1][0]}"
        print(f"\n  📍 聚集区域 #{i+1}: {line_range}, 总耗时: {total}ms, 事件数: {len(cluster)}")
        # Show top 5 events in this cluster
        top = sorted(cluster, key=lambda x: x[1], reverse=True)[:8]
        for ln, took, typ, detail in top:
            print(f"       Line {ln}: {took}ms [{typ}] {detail}")

# ============ Look for non-DIAG/PROFILER 500ms stalls ============
print(f"\n{'='*70}")
print("[搜索] 查找日志中可能导致主线程阻塞的关键词")
print(f"{'='*70}")

block_keywords = ['mutex', 'lock', 'wait', 'block', 'sleep', 'sync', 'freeze', 'stall', 
                   'signals_changed', 'rebuild_signals', 'data_updated', 'on_new_decode_data']
with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
    found = {}
    for line_no, line in enumerate(f, 1):
        for kw in block_keywords:
            if kw.lower() in line.lower():
                if kw not in found:
                    found[kw] = []
                if len(found[kw]) < 5:
                    found[kw].append((line_no, line.strip()[:150]))

for kw, entries in found.items():
    print(f"\n  关键词 '{kw}' 出现位置 (最多5条):")
    for ln, text in entries:
        print(f"    Line {ln}: {text}")

print(f"\n{'='*70}")
print("分析完成")
print(f"{'='*70}")
