#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analyze Firefox Profiler format profile for PXView.exe.
Investigates post-capture lag during protocol decoding.

Usage:
    python analyze_profile.py [profile.json_path]

If no path is given, uses the default path.
"""

import json
import sys
import os
from collections import defaultdict, Counter

# Fix Windows console encoding
if sys.stdout.encoding != "utf-8":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if sys.stderr.encoding != "utf-8":
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

# Default profile path
DEFAULT_PROFILE = r"C:\Users\admin\Downloads\PXView.exe 2026-05-20 13.28 profile.json"


def load_profile(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def resolve_stack(stack_idx, stackTable, frameTable, funcTable, strings, resourceTable, libs):
    """Walk the stack table to resolve a stack index into a list of (func_name, lib_name) tuples."""
    frames = []
    visited = set()
    while stack_idx is not None and stack_idx not in visited:
        visited.add(stack_idx)
        frame_idx = stackTable["frame"][stack_idx]
        func_idx = frameTable["func"][frame_idx]
        name_idx = funcTable["name"][func_idx]
        name = strings[name_idx] if name_idx < len(strings) else "<idx {}>".format(name_idx)
        res_idx = funcTable["resource"][func_idx]
        if res_idx >= 0 and res_idx < len(resourceTable["lib"]):
            lib_idx = resourceTable["lib"][res_idx]
            lib_name = libs[lib_idx]["name"] if lib_idx < len(libs) else "?"
        else:
            lib_name = "?"
        frames.append((name, lib_name))
        stack_idx = stackTable["prefix"][stack_idx]
    return frames


def analyze_profile(profile_path):
    print("=" * 80)
    print("PXView Performance Profile Analysis")
    print("=" * 80)
    print("Profile: {}".format(profile_path))
    print()

    profile = load_profile(profile_path)
    meta = profile["meta"]
    libs = profile["libs"]
    shared = profile["shared"]

    strings = shared["stringArray"]
    stackTable = shared["stackTable"]
    frameTable = shared["frameTable"]
    funcTable = shared["funcTable"]
    resourceTable = shared["resourceTable"]

    # Build a function name cache for speed
    func_name_cache = {}
    func_lib_cache = {}
    for func_idx in range(len(funcTable["name"])):
        name_idx = funcTable["name"][func_idx]
        name = strings[name_idx] if name_idx < len(strings) else "<idx {}>".format(name_idx)
        res_idx = funcTable["resource"][func_idx]
        if res_idx >= 0 and res_idx < len(resourceTable["lib"]):
            lib_idx = resourceTable["lib"][res_idx]
            lib_name = libs[lib_idx]["name"] if lib_idx < len(libs) else "?"
        else:
            lib_name = "?"
        func_name_cache[func_idx] = name
        func_lib_cache[func_idx] = lib_name

    # Build a frame -> (func_name, lib_name) cache
    frame_cache = {}
    for frame_idx in range(len(frameTable["func"])):
        func_idx = frameTable["func"][frame_idx]
        frame_cache[frame_idx] = (func_name_cache[func_idx], func_lib_cache[func_idx])

    # =========================================================================
    # 1. Profile Overview
    # =========================================================================
    print("=" * 80)
    print("1. PROFILE OVERVIEW")
    print("=" * 80)
    print("Product: {}".format(meta["product"]))
    print("OS: {}".format(meta.get("oscpu", "N/A")))
    print("Profile version: {}".format(meta.get("version", "N/A")))
    print("Interval: {} ms".format(meta.get("interval", "N/A")))
    sample_units = str(meta.get("sampleUnits", "N/A"))
    print("Sample units: {}".format(sample_units))
    print("Start time: {:.3f} (epoch ms)".format(meta["startTime"]))
    print("Libraries loaded: {}".format(len(libs)))
    print()

    # =========================================================================
    # 2. Thread Summary
    # =========================================================================
    print("=" * 80)
    print("2. THREAD SUMMARY")
    print("=" * 80)

    threads_info = []
    for i, t in enumerate(profile["threads"]):
        samples = t["samples"]
        num_samples = len(samples.get("time", []))
        name = t.get("name", "N/A")
        is_main = t.get("isMainThread", False)
        times = samples.get("time", [])
        cpu_deltas = samples.get("threadCPUDelta", [])

        total_cpu_us = sum(d for d in cpu_deltas if d and d > 0)
        time_range = (times[0], times[-1]) if times else (0, 0)
        duration = time_range[1] - time_range[0] if times else 0

        threads_info.append({
            "index": i,
            "name": name,
            "is_main": is_main,
            "num_samples": num_samples,
            "time_start": time_range[0],
            "time_end": time_range[1],
            "duration_ms": duration,
            "total_cpu_us": total_cpu_us,
        })

    # Sort by total CPU time descending
    threads_info.sort(key=lambda x: x["total_cpu_us"], reverse=True)

    print("{:<5} {:<40} {:<6} {:<8} {:<12} {:<12} {:<12}".format(
        "Idx", "Name", "Main?", "Samples", "Start(ms)", "End(ms)", "CPU(us)"))
    print("-" * 100)
    for ti in threads_info:
        print("{:<5} {:<40} {:<6} {:<8} {:<12.1f} {:<12.1f} {:<12.0f}".format(
            ti["index"], ti["name"][:40], "Y" if ti["is_main"] else "",
            ti["num_samples"], ti["time_start"], ti["time_end"], ti["total_cpu_us"]))
    print()

    # =========================================================================
    # 3. Main Thread Analysis
    # =========================================================================
    main_thread = profile["threads"][0]
    samples = main_thread["samples"]
    times = samples["time"]
    cpu_deltas = samples["threadCPUDelta"]
    stacks = samples["stack"]
    num_samples = len(times)

    profile_start = times[0]
    profile_end = times[-1]
    profile_duration = profile_end - profile_start

    print("=" * 80)
    print("3. MAIN THREAD ANALYSIS")
    print("=" * 80)
    print("Time range: {:.1f} - {:.1f} ms (relative to profile start)".format(profile_start, profile_end))
    print("Duration: {:.1f} ms ({:.2f} s)".format(profile_duration, profile_duration / 1000.0))
    print("Total samples: {}".format(num_samples))

    total_cpu = sum(d for d in cpu_deltas if d and d > 0)
    print("Total CPU time: {:.1f} ms".format(total_cpu / 1000.0))
    print("Average CPU per sample: {:.1f} us".format(total_cpu / max(1, num_samples)))
    print()

    # =========================================================================
    # 4. CPU Usage Timeline (binned by 100ms)
    # =========================================================================
    print("=" * 80)
    print("4. CPU USAGE TIMELINE (100ms bins)")
    print("=" * 80)

    BIN_SIZE = 100  # ms
    bins = defaultdict(lambda: {"cpu_us": 0, "count": 0})

    for i in range(num_samples):
        t = times[i]
        cpu = cpu_deltas[i] if cpu_deltas[i] else 0
        bin_idx = int(t / BIN_SIZE)
        bins[bin_idx]["cpu_us"] += cpu
        bins[bin_idx]["count"] += 1

    max_cpu_ms = 0
    max_bin_idx = 0

    print("{:<12} {:<12} {:<12} {:<40}".format("Time(ms)", "CPU(ms)", "Samples", "Bar"))
    print("-" * 80)

    sorted_bins = sorted(bins.keys())
    for bin_idx in sorted_bins:
        bin_start = bin_idx * BIN_SIZE
        cpu_ms = bins[bin_idx]["cpu_us"] / 1000.0
        count = bins[bin_idx]["count"]
        bar_len = int(cpu_ms / 2)  # 1 char per 2ms
        bar = "#" * min(bar_len, 40)
        if cpu_ms > max_cpu_ms:
            max_cpu_ms = cpu_ms
            max_bin_idx = bin_idx
        print("{:<12.0f} {:<12.1f} {:<12} {:<40}".format(bin_start, cpu_ms, count, bar))

    print()
    print("Peak CPU bin: {}-{} ms ({:.1f} ms CPU)".format(
        max_bin_idx * BIN_SIZE, (max_bin_idx + 1) * BIN_SIZE, max_cpu_ms))
    print()

    # =========================================================================
    # 5. Identify Lag Period
    # =========================================================================
    print("=" * 80)
    print("5. LAG PERIOD IDENTIFICATION")
    print("=" * 80)

    # Find contiguous high-CPU periods (CPU > 50% of bin)
    # A 100ms bin with >50ms CPU is considered high load
    HIGH_CPU_THRESHOLD = 50  # ms per 100ms bin = 50% CPU

    lag_bins = []
    for bin_idx in sorted_bins:
        cpu_ms = bins[bin_idx]["cpu_us"] / 1000.0
        if cpu_ms >= HIGH_CPU_THRESHOLD:
            lag_bins.append(bin_idx)

    # Merge contiguous bins into periods
    lag_periods = []
    if lag_bins:
        start = lag_bins[0]
        end = lag_bins[0]
        for b in lag_bins[1:]:
            if b <= end + 1:  # contiguous or adjacent
                end = b
            else:
                lag_periods.append((start * BIN_SIZE, (end + 1) * BIN_SIZE))
                start = b
                end = b
        lag_periods.append((start * BIN_SIZE, (end + 1) * BIN_SIZE))

    print("High-CPU periods (>{}ms CPU per 100ms bin):".format(HIGH_CPU_THRESHOLD))
    for start_ms, end_ms in lag_periods:
        # Calculate total CPU in this period
        period_cpu = 0
        period_samples = 0
        for bin_idx in range(int(start_ms / BIN_SIZE), int(end_ms / BIN_SIZE)):
            if bin_idx in bins:
                period_cpu += bins[bin_idx]["cpu_us"]
                period_samples += bins[bin_idx]["count"]
        print("  {:.0f}-{:.0f} ms: {:.1f} ms CPU, {} samples".format(
            start_ms, end_ms, period_cpu / 1000.0, period_samples))

    # Use the longest lag period, or the one around 6.7s if identifiable
    if lag_periods:
        # Find the period with highest total CPU
        best_period = max(lag_periods, key=lambda p: sum(
            bins[bi]["cpu_us"] for bi in range(int(p[0] / BIN_SIZE), int(p[1] / BIN_SIZE)) if bi in bins))
        lag_start, lag_end = best_period
    else:
        # Fallback: use the peak bin +/- 2 seconds
        lag_start = max(profile_start, max_bin_idx * BIN_SIZE - 2000)
        lag_end = min(profile_end, (max_bin_idx + 1) * BIN_SIZE + 2000)

    print()
    print("Selected lag period for analysis: {:.0f}-{:.0f} ms ({:.1f} s duration)".format(
        lag_start, lag_end, (lag_end - lag_start) / 1000.0))
    print()

    # =========================================================================
    # 6. Hot Functions During Lag Period
    # =========================================================================
    print("=" * 80)
    print("6. HOT FUNCTIONS DURING LAG PERIOD ({:.0f}-{:.0f} ms)".format(lag_start, lag_end))
    print("=" * 80)

    # Collect samples in the lag period
    lag_samples = []
    for i in range(num_samples):
        if lag_start <= times[i] <= lag_end:
            cpu = cpu_deltas[i] if cpu_deltas[i] else 0
            lag_samples.append((i, times[i], cpu, stacks[i]))

    print("Samples in lag period: {}".format(len(lag_samples)))
    total_lag_cpu = sum(s[2] for s in lag_samples)
    print("Total CPU in lag period: {:.1f} ms".format(total_lag_cpu / 1000.0))
    print()

    # Self-time: time where function is at top of stack (leaf)
    self_time = defaultdict(float)
    # Total-time: time where function appears anywhere in stack
    total_time = defaultdict(float)
    # Also track by library
    self_time_by_lib = defaultdict(float)
    total_time_by_lib = defaultdict(float)

    for i, t, cpu, stack_idx in lag_samples:
        if cpu <= 0:
            continue
        # Resolve stack
        visited = set()
        current = stack_idx
        depth = 0
        while current is not None and current not in visited:
            visited.add(current)
            frame_idx = stackTable["frame"][current]
            func_name, lib_name = frame_cache[frame_idx]

            # Create a display name with library context
            display_name = "{} [{}]".format(func_name, lib_name)

            if depth == 0:
                self_time[display_name] += cpu
                self_time_by_lib[lib_name] += cpu

            total_time[display_name] += cpu
            total_time_by_lib[lib_name] += cpu

            current = stackTable["prefix"][current]
            depth += 1

    # Top 30 by self-time
    print("--- Top 30 Functions by SELF-TIME (time at top of stack) ---")
    print("{:<60} {:<12} {:<8}".format("Function [Library]", "CPU(ms)", "%"))
    print("-" * 85)
    sorted_self = sorted(self_time.items(), key=lambda x: x[1], reverse=True)
    for name, cpu_us in sorted_self[:30]:
        cpu_ms = cpu_us / 1000.0
        pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
        print("{:<60} {:<12.1f} {:<8.1f}".format(name[:60], cpu_ms, pct))
    print()

    # Top 30 by total-time
    print("--- Top 30 Functions by TOTAL-TIME (time anywhere in stack) ---")
    print("{:<60} {:<12} {:<8}".format("Function [Library]", "CPU(ms)", "%"))
    print("-" * 85)
    sorted_total = sorted(total_time.items(), key=lambda x: x[1], reverse=True)
    for name, cpu_us in sorted_total[:30]:
        cpu_ms = cpu_us / 1000.0
        pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
        print("{:<60} {:<12.1f} {:<8.1f}".format(name[:60], cpu_ms, pct))
    print()

    # CPU by library
    print("--- CPU Time by Library (self-time) ---")
    print("{:<40} {:<12} {:<8}".format("Library", "CPU(ms)", "%"))
    print("-" * 65)
    sorted_lib_self = sorted(self_time_by_lib.items(), key=lambda x: x[1], reverse=True)
    for name, cpu_us in sorted_lib_self:
        cpu_ms = cpu_us / 1000.0
        pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
        print("{:<40} {:<12.1f} {:<8.1f}".format(name[:40], cpu_ms, pct))
    print()

    # =========================================================================
    # 7. Pattern Analysis
    # =========================================================================
    print("=" * 80)
    print("7. PATTERN ANALYSIS")
    print("=" * 80)

    # Categorize samples by pattern
    patterns = defaultdict(float)  # pattern -> CPU us
    pattern_details = defaultdict(list)

    for i, t, cpu, stack_idx in lag_samples:
        if cpu <= 0:
            continue
        # Resolve stack to get all function names
        frames = resolve_stack(stack_idx, stackTable, frameTable, funcTable, strings, resourceTable, libs)
        frame_names = [f[0] for f in frames]
        frame_libs = [f[1] for f in frames]

        # Classify the pattern
        pattern = "other"
        top_name = frame_names[0] if frame_names else "?"
        top_lib = frame_libs[0] if frame_libs else "?"

        # Check for specific patterns
        all_names_lower = " ".join(frame_names).lower()
        all_libs = set(frame_libs)

        if "RtlAllocateHeap" in top_name or "RtlFreeHeap" in top_name or "RtlReAllocateHeap" in top_name:
            pattern = "heap_alloc"
        elif "RtlEnterCriticalSection" in top_name or "RtlLeaveCriticalSection" in top_name:
            pattern = "critical_section"
        elif "NtWaitFor" in top_name or "SleepConditionVariable" in top_name or "NtDelayExecution" in top_name:
            pattern = "wait_sleep"
        elif "NtUserPeekMessage" in top_name or "NtUserGetMessage" in top_name or "NtUserDispatchMessage" in top_name:
            pattern = "message_pump"
        elif "DWrite" in top_lib or "dwrite" in all_names_lower:
            pattern = "text_rendering_dwrite"
        elif "libharfbuzz" in top_lib:
            pattern = "text_shaping_harfbuzz"
        elif "Qt6Gui" in top_lib and any("0x3c" in n or "0x3d" in n or "0x3f" in n for n in frame_names[:5]):
            pattern = "qt_gui_rendering"
        elif "Qt6Widgets" in top_lib:
            pattern = "qt_widgets_paint"
        elif "Qt6Gui" in top_lib:
            pattern = "qt_gui_other"
        elif "PXView" in top_lib:
            pattern = "pxview_app_code"
        elif "libpython" in top_lib:
            pattern = "python_decoder"
        elif "ntdll" in top_lib:
            pattern = "ntdll_syscall"
        elif "ucrtbase" in top_lib or "msvcrt" in top_lib:
            pattern = "c_runtime"
        elif "user32" in top_lib or "win32u" in top_lib:
            pattern = "win32_ui"
        elif "gdi32" in top_lib or "GdiPlus" in top_lib:
            pattern = "gdi_rendering"

        patterns[pattern] += cpu
        pattern_details[pattern].append((t, cpu, top_name, top_lib))

    print("Pattern breakdown (by self-time CPU):")
    print("{:<30} {:<12} {:<8} {:<8}".format("Pattern", "CPU(ms)", "%", "Samples"))
    print("-" * 65)
    sorted_patterns = sorted(patterns.items(), key=lambda x: x[1], reverse=True)
    for pattern, cpu_us in sorted_patterns:
        cpu_ms = cpu_us / 1000.0
        pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
        count = len(pattern_details[pattern])
        print("{:<30} {:<12.1f} {:<8.1f} {:<8}".format(pattern, cpu_ms, pct, count))
    print()

    # =========================================================================
    # 8. Detailed Stack Analysis for Top Hotspots
    # =========================================================================
    print("=" * 80)
    print("8. DETAILED STACK ANALYSIS - TOP HOTSPOTS")
    print("=" * 80)

    # Find the most common stack prefixes (call paths) during lag
    # Group by the top 5 frames of each stack
    stack_prefix_counter = defaultdict(float)
    for i, t, cpu, stack_idx in lag_samples:
        if cpu <= 0:
            continue
        frames = resolve_stack(stack_idx, stackTable, frameTable, funcTable, strings, resourceTable, libs)
        # Take top 5 frames (leaf side)
        prefix = " <- ".join("{} [{}]".format(f[0], f[1]) for f in frames[:5])
        stack_prefix_counter[prefix] += cpu

    print("Most common call paths (top 5 frames, by CPU):")
    print("-" * 100)
    sorted_prefixes = sorted(stack_prefix_counter.items(), key=lambda x: x[1], reverse=True)
    for prefix, cpu_us in sorted_prefixes[:20]:
        cpu_ms = cpu_us / 1000.0
        pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
        print("{:.1f}ms ({:.1f}%): {}".format(cpu_ms, pct, prefix[:120]))
    print()

    # =========================================================================
    # 9. PXView.exe-specific Analysis
    # =========================================================================
    print("=" * 80)
    print("9. PXView.EXE FUNCTION ANALYSIS (by address)")
    print("=" * 80)

    # Collect all PXView.exe functions seen in stacks
    pxview_self_time = defaultdict(float)
    pxview_total_time = defaultdict(float)

    for i, t, cpu, stack_idx in lag_samples:
        if cpu <= 0:
            continue
        visited = set()
        current = stack_idx
        depth = 0
        while current is not None and current not in visited:
            visited.add(current)
            frame_idx = stackTable["frame"][current]
            func_name, lib_name = frame_cache[frame_idx]
            if lib_name == "PXView.exe":
                if depth == 0:
                    pxview_self_time[func_name] += cpu
                pxview_total_time[func_name] += cpu
            current = stackTable["prefix"][current]
            depth += 1

    print("PXView.exe functions by self-time:")
    print("{:<20} {:<12} {:<8}".format("Address", "CPU(ms)", "%"))
    print("-" * 45)
    sorted_px_self = sorted(pxview_self_time.items(), key=lambda x: x[1], reverse=True)
    for addr, cpu_us in sorted_px_self[:20]:
        cpu_ms = cpu_us / 1000.0
        pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
        print("{:<20} {:<12.1f} {:<8.1f}".format(addr, cpu_ms, pct))
    print()

    print("PXView.exe functions by total-time:")
    print("{:<20} {:<12} {:<8}".format("Address", "CPU(ms)", "%"))
    print("-" * 45)
    sorted_px_total = sorted(pxview_total_time.items(), key=lambda x: x[1], reverse=True)
    for addr, cpu_us in sorted_px_total[:20]:
        cpu_ms = cpu_us / 1000.0
        pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
        print("{:<20} {:<12.1f} {:<8.1f}".format(addr, cpu_ms, pct))
    print()

    # =========================================================================
    # 10. Worker/Decoder Thread Analysis
    # =========================================================================
    print("=" * 80)
    print("10. WORKER/DECODER THREAD ANALYSIS")
    print("=" * 80)

    # Analyze pooled threads and other worker threads
    for ti in threads_info[:15]:  # Top 15 by CPU
        t = profile["threads"][ti["index"]]
        t_samples = t["samples"]
        t_times = t_samples.get("time", [])
        t_cpu = t_samples.get("threadCPUDelta", [])
        t_stacks = t_samples.get("stack", [])
        t_name = t["name"]

        if len(t_times) == 0:
            continue

        # Count what these threads are doing
        thread_patterns = defaultdict(float)
        for i in range(len(t_times)):
            cpu = t_cpu[i] if t_cpu[i] else 0
            if cpu <= 0:
                continue
            frames = resolve_stack(t_stacks[i], stackTable, frameTable, funcTable, strings, resourceTable, libs)
            top_name = frames[0][0] if frames else "?"
            top_lib = frames[0][1] if frames else "?"

            if "NtWaitFor" in top_name or "SleepConditionVariable" in top_name or "NtDelayExecution" in top_name:
                thread_patterns["waiting"] += cpu
            elif "PXView" in top_lib:
                thread_patterns["pxview_work"] += cpu
            elif "libpython" in top_lib:
                thread_patterns["python_decoder"] += cpu
            elif "libsigrokdecode" in top_lib:
                thread_patterns["c_decoder"] += cpu
            else:
                thread_patterns["other"] += cpu

        total_thread_cpu = sum(thread_patterns.values())
        if total_thread_cpu > 0:
            print("Thread {} ({}): {:.1f}ms CPU total".format(
                ti["index"], t_name, total_thread_cpu / 1000.0))
            for pat, cpu_us in sorted(thread_patterns.items(), key=lambda x: x[1], reverse=True):
                pct = cpu_us / total_thread_cpu * 100
                print("  {:<25} {:.1f}ms ({:.1f}%)".format(pat, cpu_us / 1000.0, pct))
            print()

    # =========================================================================
    # 11. Markers Analysis
    # =========================================================================
    print("=" * 80)
    print("11. MARKERS / EVENTS")
    print("=" * 80)

    for i, t in enumerate(profile["threads"]):
        markers = t.get("markers", {})
        marker_names = markers.get("name", [])
        if marker_names and len(marker_names) > 0:
            # Count marker types
            name_strings = [strings[n] if n < len(strings) else "<err>" for n in marker_names]
            name_counts = Counter(name_strings)
            if sum(name_counts.values()) > 0:
                tname = t.get("name", "Thread {}".format(i))
                print("Thread {} ({}): {} markers".format(i, tname, len(marker_names)))
                for name, count in name_counts.most_common(10):
                    print("  {}: {}".format(name, count))
                print()

    # =========================================================================
    # 12. Summary and Conclusions
    # =========================================================================
    print("=" * 80)
    print("12. SUMMARY AND CONCLUSIONS")
    print("=" * 80)

    print()
    print("KEY FINDINGS:")
    print()

    # Identify the dominant pattern
    if sorted_patterns:
        dominant = sorted_patterns[0]
        print("1. Dominant CPU consumer: {} ({:.1f}ms, {:.1f}% of lag period CPU)".format(
            dominant[0], dominant[1] / 1000.0, dominant[1] / total_lag_cpu * 100 if total_lag_cpu > 0 else 0))

    # Check for text rendering
    text_cpu = patterns.get("text_rendering_dwrite", 0) + patterns.get("text_shaping_harfbuzz", 0)
    if text_cpu > 0:
        print("2. Text rendering (DWrite + HarfBuzz): {:.1f}ms ({:.1f}% of lag CPU)".format(
            text_cpu / 1000.0, text_cpu / total_lag_cpu * 100 if total_lag_cpu > 0 else 0))
        print("   This is a MAJOR contributor to the lag. Qt is spending significant time")
        print("   in font shaping and text layout during widget painting.")

    # Check for heap allocation
    heap_cpu = patterns.get("heap_alloc", 0)
    if heap_cpu > 0:
        print("3. Heap allocation/deallocation: {:.1f}ms ({:.1f}% of lag CPU)".format(
            heap_cpu / 1000.0, heap_cpu / total_lag_cpu * 100 if total_lag_cpu > 0 else 0))

    # Check for critical section
    cs_cpu = patterns.get("critical_section", 0)
    if cs_cpu > 0:
        print("4. Critical section contention: {:.1f}ms ({:.1f}% of lag CPU)".format(
            cs_cpu / 1000.0, cs_cpu / total_lag_cpu * 100 if total_lag_cpu > 0 else 0))

    # Check for PXView app code
    pxview_cpu = patterns.get("pxview_app_code", 0)
    if pxview_cpu > 0:
        print("5. PXView application code: {:.1f}ms ({:.1f}% of lag CPU)".format(
            pxview_cpu / 1000.0, pxview_cpu / total_lag_cpu * 100 if total_lag_cpu > 0 else 0))

    # Check for Python decoder
    python_cpu = patterns.get("python_decoder", 0)
    if python_cpu > 0:
        print("6. Python decoder execution: {:.1f}ms ({:.1f}% of lag CPU)".format(
            python_cpu / 1000.0, python_cpu / total_lag_cpu * 100 if total_lag_cpu > 0 else 0))

    # Qt rendering
    qt_render_cpu = patterns.get("qt_gui_rendering", 0) + patterns.get("qt_widgets_paint", 0)
    if qt_render_cpu > 0:
        print("7. Qt rendering/painting: {:.1f}ms ({:.1f}% of lag CPU)".format(
            qt_render_cpu / 1000.0, qt_render_cpu / total_lag_cpu * 100 if total_lag_cpu > 0 else 0))

    print()
    print("RECOMMENDATIONS:")
    print()

    if text_cpu > total_lag_cpu * 0.2:
        print("- Text rendering is consuming >20% of CPU during the lag period.")
        print("  Consider: caching text layouts, reducing repaint regions, using")
        print("  QStaticText for frequently drawn text, or reducing the number of")
        print("  visible decode annotation labels that need text shaping.")

    if heap_cpu > total_lag_cpu * 0.1:
        print("- Heap allocation is significant. Consider pre-allocating buffers,")
        print("  using object pools, or reducing temporary object creation in")
        print("  hot rendering paths.")

    if qt_render_cpu > total_lag_cpu * 0.3:
        print("- Qt rendering is the dominant cost. Consider optimizing viewport")
        print("  repaint: reduce invalidated regions, use backing store, avoid")
        print("  full repaints when only decode annotations change.")

    if pxview_cpu > total_lag_cpu * 0.1:
        print("- PXView application code is consuming significant CPU. The hex")
        print("  addresses need to be resolved with debug symbols to identify")
        print("  the specific functions. Build with -g and use addr2sym.")


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PROFILE
    # Write output to file
    output_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "profile_analysis_output.txt")
    import io

    class TeeWriter:
        def __init__(self, *files):
            self.files = files
        def write(self, data):
            for f in self.files:
                f.write(data)
        def flush(self):
            for f in self.files:
                f.flush()

    with open(output_file, "w", encoding="utf-8") as f:
        old_stdout = sys.stdout
        sys.stdout = TeeWriter(old_stdout, f)
        try:
            analyze_profile(path)
        finally:
            sys.stdout = old_stdout
