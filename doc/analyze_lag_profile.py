#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analyze Firefox Profiler (SimpleFirefox) format profile for PXView.exe.
Focuses on identifying root causes of GUI lag during capture and decoding.

Usage:
    python analyze_lag_profile.py [profile.json_path]
"""

import json
import sys
import os
from collections import defaultdict, Counter

if sys.stdout.encoding != "utf-8":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if sys.stderr.encoding != "utf-8":
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

DEFAULT_PROFILE = r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\doc\PXView.exe 2026-05-21 09.53 profile.json"
NM_FILE = r"c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\doc\nm_sorted.txt"
IMAGE_BASE = 0x140000000


def demangle(name):
    """Simple C++ name demangler for the most common patterns."""
    import re
    if not name.startswith("_Z"):
        return name
    # Handle _ZN<namespace>...E<args>
    # Just do a basic demangle for readability
    text = name
    # Remove _Z prefix
    text = text[2:]
    # Handle N...E (nested name)
    if text.startswith("N"):
        text = text[1:]
        if text.endswith("E"):
            text = text[:-1]
        # Split by length-prefixed segments
        parts = []
        i = 0
        while i < len(text):
            if text[i].isdigit():
                length = 0
                while i < len(text) and text[i].isdigit():
                    length = length * 10 + int(text[i])
                    i += 1
                if i + length <= len(text):
                    parts.append(text[i:i+length])
                    i += length
                else:
                    break
            else:
                # Template or other
                parts.append(text[i:])
                break
        result = "::".join(parts)
        return result
    # Handle simple names
    i = 0
    length = 0
    while i < len(text) and text[i].isdigit():
        length = length * 10 + int(text[i])
        i += 1
    if i + length <= len(text):
        return text[i:i+length]
    return name


def load_nm_symbols(nm_path):
    """Load nm output and build a sorted list of (addr, name) for symbol resolution."""
    symbols = []
    if not os.path.exists(nm_path):
        return symbols
    with open(nm_path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(None, 1)
            if len(parts) < 2:
                continue
            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue
            name = parts[1]
            symbols.append((addr, name))
    symbols.sort(key=lambda x: x[0])
    return symbols


def resolve_address(addr_hex, symbols):
    """Given a hex address string from profile (RVA), find the nearest symbol."""
    try:
        rva = int(addr_hex, 16)
    except ValueError:
        return addr_hex
    vma = IMAGE_BASE + rva
    # Binary search for nearest symbol <= vma
    lo, hi = 0, len(symbols) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        if symbols[mid][0] <= vma:
            best = mid
            lo = mid + 1
        else:
            hi = mid - 1
    if best is not None:
        sym_addr, sym_name = symbols[best]
        offset = vma - sym_addr
        demangled = demangle(sym_name)
        if offset > 0:
            return f"{demangled}+{offset}"
        return demangled
    return addr_hex


def load_profile(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


class ProfileAnalyzer:
    def __init__(self, profile, symbols=None):
        self.profile = profile
        self.meta = profile["meta"]
        self.libs = profile["libs"]
        self.shared = profile["shared"]
        self.strings = self.shared["stringArray"]
        self.stackTable = self.shared["stackTable"]
        self.frameTable = self.shared["frameTable"]
        self.funcTable = self.shared["funcTable"]
        self.resourceTable = self.shared["resourceTable"]
        self.symbols = symbols or []
        self._build_caches()

    def _build_caches(self):
        """Pre-build lookup caches for fast stack resolution."""
        self.func_name_cache = {}
        self.func_lib_cache = {}
        for func_idx in range(len(self.funcTable["name"])):
            name_idx = self.funcTable["name"][func_idx]
            name = self.strings[name_idx] if name_idx < len(self.strings) else f"<idx {name_idx}>"
            res_idx = self.funcTable["resource"][func_idx]
            if res_idx >= 0 and res_idx < len(self.resourceTable.get("lib", [])):
                lib_idx = self.resourceTable["lib"][res_idx]
                lib_name = self.libs[lib_idx]["name"] if lib_idx < len(self.libs) else "?"
            else:
                lib_name = "?"

            # Try to resolve PXView.exe hex addresses to symbols
            if lib_name == "PXView.exe" and name.startswith("0x") and self.symbols:
                resolved = resolve_address(name[2:], self.symbols)
                if resolved != name:
                    name = resolved

            self.func_name_cache[func_idx] = name
            self.func_lib_cache[func_idx] = lib_name

        self.frame_cache = {}
        for frame_idx in range(len(self.frameTable["func"])):
            func_idx = self.frameTable["func"][frame_idx]
            self.frame_cache[frame_idx] = (self.func_name_cache[func_idx], self.func_lib_cache[func_idx])

    def resolve_stack_fast(self, stack_idx):
        """Resolve stack to list of (func_name, lib_name) from leaf to root."""
        frames = []
        visited = set()
        current = stack_idx
        while current is not None and current not in visited:
            visited.add(current)
            frame_idx = self.stackTable["frame"][current]
            frames.append(self.frame_cache[frame_idx])
            current = self.stackTable["prefix"][current]
        return frames

    def classify_frame(self, func_name, lib_name):
        """Classify a single frame into a category."""
        fn = func_name
        ln = lib_name

        if "RtlAllocateHeap" in fn or "RtlFreeHeap" in fn or "RtlReAllocateHeap" in fn:
            return "heap_alloc"
        if "RtlEnterCriticalSection" in fn or "RtlLeaveCriticalSection" in fn or "NtWaitForSingleObject" in fn:
            return "lock_contention"
        if "NtWaitFor" in fn or "SleepConditionVariable" in fn or "NtDelayExecution" in fn:
            return "wait_sleep"
        if "NtUserPeekMessage" in fn or "NtUserGetMessage" in fn or "NtUserDispatchMessage" in fn:
            return "win32_msg_pump"
        if "memcpy" in fn.lower() or "memmove" in fn.lower() or "memset" in fn.lower():
            return "memory_copy"
        if "DWrite" in ln or "dwrite" in fn.lower():
            return "text_rendering_dwrite"
        if "libharfbuzz" in ln or "harfbuzz" in fn.lower():
            return "text_shaping"
        if "Qt6Widgets" in ln:
            return "qt_widgets"
        if "Qt6Gui" in ln:
            if "paint" in fn.lower() or "draw" in fn.lower() or "render" in fn.lower() or "raster" in fn.lower():
                return "qt_gui_render"
            return "qt_gui_other"
        if "Qt6Core" in ln:
            return "qt_core"
        if "PXView" in ln:
            return "pxview_app"
        if "libpython" in ln or "python3" in ln.lower():
            return "python_decoder"
        if "libsigrokdecode" in ln:
            return "c_decoder_engine"
        if "libsigrok" in ln:
            return "libsigrok"
        if "ntdll" in ln:
            return "ntdll"
        if "ucrtbase" in ln or "msvcrt" in ln or "libstdc++" in ln or "libgcc" in ln:
            return "c_runtime"
        if "user32" in ln or "win32u" in ln:
            return "win32_ui"
        if "gdi32" in ln or "GdiPlus" in ln:
            return "gdi_render"
        if "libusb" in ln:
            return "usb_io"
        if "zlib" in ln:
            return "zlib"
        return "other"

    def classify_stack(self, frames):
        """Classify a stack trace by its leaf (topmost) frame."""
        if not frames:
            return "empty"
        top_name, top_lib = frames[0]
        return self.classify_frame(top_name, top_lib)

    def get_thread_info(self):
        """Gather summary info for all threads."""
        threads_info = []
        for i, t in enumerate(self.profile["threads"]):
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

        threads_info.sort(key=lambda x: x["total_cpu_us"], reverse=True)
        return threads_info

    def analyze_thread(self, thread_idx, focus_period=None):
        """Analyze a single thread. If focus_period=(start_ms, end_ms), only analyze that window."""
        t = self.profile["threads"][thread_idx]
        samples = t["samples"]
        times = samples.get("time", [])
        cpu_deltas = samples.get("threadCPUDelta", [])
        stacks = samples.get("stack", [])

        self_time = defaultdict(float)
        total_time = defaultdict(float)
        self_time_by_lib = defaultdict(float)
        total_time_by_lib = defaultdict(float)
        category_cpu = defaultdict(float)
        call_paths = defaultdict(float)

        total_cpu = 0

        for i in range(len(times)):
            t_ms = times[i]
            cpu = cpu_deltas[i] if i < len(cpu_deltas) and cpu_deltas[i] else 0
            if cpu <= 0:
                continue
            if focus_period and not (focus_period[0] <= t_ms <= focus_period[1]):
                continue

            total_cpu += cpu
            stack_idx = stacks[i] if i < len(stacks) else None
            if stack_idx is None:
                continue

            frames = self.resolve_stack_fast(stack_idx)
            cat = self.classify_stack(frames)
            category_cpu[cat] += cpu

            # Call path (top 6 frames)
            path = " <- ".join(f"{f[0]} [{f[1]}]" for f in frames[:6])
            call_paths[path] += cpu

            visited_funcs = set()
            for depth, (func_name, lib_name) in enumerate(frames):
                display = f"{func_name} [{lib_name}]"
                if depth == 0:
                    self_time[display] += cpu
                    self_time_by_lib[lib_name] += cpu
                total_time[display] += cpu
                total_time_by_lib[lib_name] += cpu

        return {
            "self_time": self_time,
            "total_time": total_time,
            "self_time_by_lib": self_time_by_lib,
            "total_time_by_lib": total_time_by_lib,
            "category_cpu": category_cpu,
            "call_paths": call_paths,
            "total_cpu": total_cpu,
        }

    def find_lag_periods(self, thread_idx, bin_size_ms=100, threshold_pct=50):
        """Find periods of high CPU usage on a thread."""
        t = self.profile["threads"][thread_idx]
        samples = t["samples"]
        times = samples.get("time", [])
        cpu_deltas = samples.get("threadCPUDelta", [])

        bins = defaultdict(lambda: {"cpu_us": 0, "count": 0})
        for i in range(len(times)):
            cpu = cpu_deltas[i] if i < len(cpu_deltas) and cpu_deltas[i] else 0
            bin_idx = int(times[i] / bin_size_ms)
            bins[bin_idx]["cpu_us"] += cpu
            bins[bin_idx]["count"] += 1

        # Merge contiguous high-CPU bins into periods
        threshold_us = threshold_pct * bin_size_ms * 10  # e.g. 50% of 100ms = 50000us
        lag_bins = [bi for bi in sorted(bins.keys()) if bins[bi]["cpu_us"] >= threshold_us]

        lag_periods = []
        if lag_bins:
            start = lag_bins[0]
            end = lag_bins[0]
            for b in lag_bins[1:]:
                if b <= end + 1:
                    end = b
                else:
                    lag_periods.append((start * bin_size_ms, (end + 1) * bin_size_ms))
                    start = b
                    end = b
            lag_periods.append((start * bin_size_ms, (end + 1) * bin_size_ms))

        return lag_periods, bins

    def print_top(self, data, label, total_cpu, n=25):
        """Print top N items from a {name: cpu_us} dict."""
        sorted_items = sorted(data.items(), key=lambda x: x[1], reverse=True)
        print(f"\n--- {label} ---")
        print(f"{'Name':<70} {'CPU(ms)':<10} {'%':<8}")
        print("-" * 90)
        for name, cpu_us in sorted_items[:n]:
            cpu_ms = cpu_us / 1000.0
            pct = (cpu_us / total_cpu * 100) if total_cpu > 0 else 0
            print(f"{name[:70]:<70} {cpu_ms:<10.1f} {pct:<8.1f}")


def main():
    profile_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PROFILE

    print("=" * 90)
    print("PXView GUI Lag Analysis - Capture & Decode Profile")
    print("=" * 90)
    print(f"Profile: {profile_path}")
    print()

    profile = load_profile(profile_path)
    symbols = load_nm_symbols(NM_FILE)
    print(f"Loaded {len(symbols)} symbols from nm output")
    analyzer = ProfileAnalyzer(profile, symbols)

    # =========================================================================
    # 1. Overview
    # =========================================================================
    print("=" * 90)
    print("1. PROFILE OVERVIEW")
    print("=" * 90)
    print(f"Product: {analyzer.meta['product']}")
    print(f"OS: {analyzer.meta.get('oscpu', 'N/A')}")
    print(f"Interval: {analyzer.meta.get('interval', 'N/A')} ms")
    print(f"Libraries: {len(analyzer.libs)}")
    print()

    # =========================================================================
    # 2. Thread Summary
    # =========================================================================
    print("=" * 90)
    print("2. THREAD SUMMARY (sorted by CPU)")
    print("=" * 90)
    threads_info = analyzer.get_thread_info()
    print(f"{'Idx':<5} {'Name':<40} {'Main':<6} {'Samples':<8} {'CPU(ms)':<10} {'Start':<10} {'End':<10}")
    print("-" * 95)
    for ti in threads_info:
        if ti["total_cpu_us"] < 100:
            continue
        print(f"{ti['index']:<5} {ti['name'][:40]:<40} {'Y' if ti['is_main'] else '':<6} "
              f"{ti['num_samples']:<8} {ti['total_cpu_us']/1000:<10.1f} "
              f"{ti['time_start']:<10.0f} {ti['time_end']:<10.0f}")
    print()

    # =========================================================================
    # 3. Main Thread Deep Analysis
    # =========================================================================
    main_idx = 0
    for i, t in enumerate(profile["threads"]):
        if t.get("isMainThread", False):
            main_idx = i
            break

    print("=" * 90)
    print(f"3. MAIN THREAD (index {main_idx}) - FULL ANALYSIS")
    print("=" * 90)

    main_t = profile["threads"][main_idx]
    main_samples = main_t["samples"]
    main_times = main_samples.get("time", [])
    main_cpu = main_samples.get("threadCPUDelta", [])
    main_stacks = main_samples.get("stack", [])
    main_num = len(main_times)

    main_total_cpu = sum(d for d in main_cpu if d and d > 0)
    print(f"Samples: {main_num}")
    print(f"Time range: {main_times[0]:.0f} - {main_times[-1]:.0f} ms ({(main_times[-1]-main_times[0])/1000:.1f}s)")
    print(f"Total CPU: {main_total_cpu/1000:.1f} ms")
    print()

    # CPU timeline
    print("--- CPU Timeline (200ms bins) ---")
    BIN = 200
    bins = defaultdict(lambda: {"cpu_us": 0, "count": 0})
    for i in range(main_num):
        cpu = main_cpu[i] if main_cpu[i] else 0
        bi = int(main_times[i] / BIN)
        bins[bi]["cpu_us"] += cpu
        bins[bi]["count"] += 1

    print(f"{'Time(s)':<10} {'CPU(ms)':<10} {'Samples':<8} {'Bar'}")
    print("-" * 70)
    max_cpu_ms = 0
    max_bin = 0
    for bi in sorted(bins.keys()):
        cpu_ms = bins[bi]["cpu_us"] / 1000.0
        bar = "#" * min(int(cpu_ms), 50)
        if cpu_ms > max_cpu_ms:
            max_cpu_ms = cpu_ms
            max_bin = bi
        print(f"{bi*BIN/1000:<10.1f} {cpu_ms:<10.1f} {bins[bi]['count']:<8} {bar}")
    print()

    # =========================================================================
    # 4. Lag Period Detection
    # =========================================================================
    print("=" * 90)
    print("4. LAG PERIOD DETECTION (main thread)")
    print("=" * 90)

    lag_periods, _ = analyzer.find_lag_periods(main_idx, bin_size_ms=200, threshold_pct=40)

    if not lag_periods:
        print("No significant lag periods detected. Analyzing full profile.")
        lag_start, lag_end = main_times[0], main_times[-1]
    else:
        print("Detected high-CPU periods:")
        for s, e in lag_periods:
            print(f"  {s:.0f} - {e:.0f} ms ({(e-s)/1000:.1f}s)")

        # Pick the longest/highest-CPU period
        best = max(lag_periods, key=lambda p: e - s if False else (e - s))
        lag_start, lag_end = best
        print(f"\nSelected for deep analysis: {lag_start:.0f} - {lag_end:.0f} ms")

    print()

    # =========================================================================
    # 5. Main Thread Hot Functions (lag period)
    # =========================================================================
    print("=" * 90)
    print(f"5. MAIN THREAD HOT FUNCTIONS (lag period {lag_start:.0f}-{lag_end:.0f} ms)")
    print("=" * 90)

    result = analyzer.analyze_thread(main_idx, focus_period=(lag_start, lag_end))
    total_lag_cpu = result["total_cpu"]

    if total_lag_cpu > 0:
        analyzer.print_top(result["self_time"], "SELF-TIME (leaf function)", total_lag_cpu, 30)
        analyzer.print_top(result["total_time"], "TOTAL-TIME (anywhere in stack)", total_lag_cpu, 30)
        analyzer.print_top(result["self_time_by_lib"], "SELF-TIME by Library", total_lag_cpu, 15)
        analyzer.print_top(result["category_cpu"], "CPU by Category", total_lag_cpu, 20)
        analyzer.print_top(result["call_paths"], "Top Call Paths (top 6 frames)", total_lag_cpu, 20)

    # =========================================================================
    # 6. Full Main Thread Analysis (all time)
    # =========================================================================
    print("\n" + "=" * 90)
    print("6. MAIN THREAD - FULL PROFILE CATEGORY BREAKDOWN")
    print("=" * 90)

    full_result = analyzer.analyze_thread(main_idx)
    full_cpu = full_result["total_cpu"]
    if full_cpu > 0:
        analyzer.print_top(full_result["category_cpu"], "Category (full profile)", full_cpu, 20)
        analyzer.print_top(full_result["self_time"], "Self-time (full profile)", full_cpu, 25)

    # =========================================================================
    # 7. Worker/Decoder Thread Analysis
    # =========================================================================
    print("\n" + "=" * 90)
    print("7. WORKER & DECODER THREAD ANALYSIS")
    print("=" * 90)

    for ti in threads_info[:20]:
        if ti["total_cpu_us"] < 5000 or ti["is_main"]:
            continue

        t_idx = ti["index"]
        t_name = ti["name"]
        t_result = analyzer.analyze_thread(t_idx)

        if t_result["total_cpu"] < 5000:
            continue

        print(f"\n--- Thread {t_idx}: {t_name} ({t_result['total_cpu']/1000:.1f}ms CPU) ---")
        analyzer.print_top(t_result["category_cpu"], f"Categories", t_result["total_cpu"], 10)
        analyzer.print_top(t_result["self_time"], f"Self-time top 15", t_result["total_cpu"], 15)
        analyzer.print_top(t_result["call_paths"], f"Top call paths", t_result["total_cpu"], 10)

    # =========================================================================
    # 8. Cross-Thread Lock Contention Analysis
    # =========================================================================
    print("\n" + "=" * 90)
    print("8. LOCK CONTENTION & BLOCKING ANALYSIS (all threads)")
    print("=" * 90)

    for ti in threads_info[:20]:
        t_idx = ti["index"]
        t = profile["threads"][t_idx]
        samples = t["samples"]
        times = samples.get("time", [])
        cpu_deltas = samples.get("threadCPUDelta", [])
        stacks = samples.get("stack", [])

        blocking_samples = 0
        blocking_cpu = 0
        lock_names = defaultdict(float)

        for i in range(len(times)):
            cpu = cpu_deltas[i] if i < len(cpu_deltas) and cpu_deltas[i] else 0
            stack_idx = stacks[i] if i < len(stacks) else None
            if stack_idx is None:
                continue
            frames = analyzer.resolve_stack_fast(stack_idx)
            if not frames:
                continue
            top_name, top_lib = frames[0]
            if any(kw in top_name for kw in ["RtlEnterCriticalSection", "NtWaitFor",
                                               "SleepConditionVariable", "NtDelayExecution",
                                               "RtlAllocateHeap", "RtlFreeHeap"]):
                blocking_samples += 1
                blocking_cpu += cpu
                lock_names[top_name] += cpu

        if blocking_cpu > 1000:
            print(f"\nThread {t_idx} ({t.get('name', '?')}): {blocking_cpu/1000:.1f}ms in blocking/waiting")
            for name, cpu_us in sorted(lock_names.items(), key=lambda x: x[1], reverse=True)[:5]:
                print(f"  {name[:60]}: {cpu_us/1000:.1f}ms")

    # =========================================================================
    # 9. PXView.exe Symbol Analysis
    # =========================================================================
    print("\n" + "=" * 90)
    print("9. PXView.exe FUNCTION ANALYSIS (main thread, lag period)")
    print("=" * 90)

    pxview_self = defaultdict(float)
    pxview_total = defaultdict(float)
    pxview_callers = defaultdict(lambda: defaultdict(float))  # func -> {caller: cpu}

    for i in range(main_num):
        if not (lag_start <= main_times[i] <= lag_end):
            continue
        cpu = main_cpu[i] if main_cpu[i] else 0
        if cpu <= 0:
            continue
        stack_idx = main_stacks[i] if i < len(main_stacks) else None
        if stack_idx is None:
            continue

        frames = analyzer.resolve_stack_fast(stack_idx)
        for depth, (func_name, lib_name) in enumerate(frames):
            if lib_name == "PXView.exe":
                if depth == 0:
                    pxview_self[func_name] += cpu
                pxview_total[func_name] += cpu
                # Record caller (next frame up in the stack = parent)
                if depth + 1 < len(frames):
                    caller_name, caller_lib = frames[depth + 1]
                    pxview_callers[func_name][f"{caller_name} [{caller_lib}]"] += cpu

    if pxview_self:
        print("\nPXView.exe functions by SELF-TIME:")
        print(f"{'Function':<30} {'CPU(ms)':<10} {'%':<8}")
        print("-" * 50)
        for name, cpu_us in sorted(pxview_self.items(), key=lambda x: x[1], reverse=True)[:20]:
            pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
            print(f"{name[:30]:<30} {cpu_us/1000:<10.1f} {pct:<8.1f}")

    if pxview_total:
        print("\nPXView.exe functions by TOTAL-TIME:")
        print(f"{'Function':<30} {'CPU(ms)':<10} {'%':<8}")
        print("-" * 50)
        for name, cpu_us in sorted(pxview_total.items(), key=lambda x: x[1], reverse=True)[:20]:
            pct = (cpu_us / total_lag_cpu * 100) if total_lag_cpu > 0 else 0
            print(f"{name[:30]:<30} {cpu_us/1000:<10.1f} {pct:<8.1f}")

    # Show callers for top PXView functions
    if pxview_self:
        top_func = sorted(pxview_self.items(), key=lambda x: x[1], reverse=True)[0][0]
        if top_func in pxview_callers:
            print(f"\nCallers of top self-time function '{top_func}':")
            for caller, cpu_us in sorted(pxview_callers[top_func].items(), key=lambda x: x[1], reverse=True)[:10]:
                print(f"  {caller[:60]}: {cpu_us/1000:.1f}ms")

    # =========================================================================
    # 10. Decode-Specific Analysis
    # =========================================================================
    print("\n" + "=" * 90)
    print("10. DECODE PIPELINE ANALYSIS")
    print("=" * 90)

    # Look for decoder-related activity across all threads
    decode_cpu_by_thread = {}
    for ti in threads_info:
        t_idx = ti["index"]
        t = profile["threads"][t_idx]
        samples = t["samples"]
        times = samples.get("time", [])
        cpu_deltas = samples.get("threadCPUDelta", [])
        stacks = samples.get("stack", [])

        decode_cpu = 0
        decode_funcs = defaultdict(float)
        for i in range(len(times)):
            cpu = cpu_deltas[i] if i < len(cpu_deltas) and cpu_deltas[i] else 0
            if cpu <= 0:
                continue
            stack_idx = stacks[i] if i < len(stacks) else None
            if stack_idx is None:
                continue
            frames = analyzer.resolve_stack_fast(stack_idx)
            for func_name, lib_name in frames:
                if any(kw in lib_name.lower() for kw in ["libsigrokdecode", "libpython", "python"]):
                    decode_cpu += cpu
                    decode_funcs[f"{func_name} [{lib_name}]"] += cpu
                    break  # count sample once

        if decode_cpu > 0:
            decode_cpu_by_thread[t_idx] = {
                "name": t.get("name", "?"),
                "cpu": decode_cpu,
                "funcs": decode_funcs,
            }

    if decode_cpu_by_thread:
        print("Threads with decode activity:")
        for t_idx, info in sorted(decode_cpu_by_thread.items(), key=lambda x: x[1]["cpu"], reverse=True):
            print(f"\n  Thread {t_idx} ({info['name']}): {info['cpu']/1000:.1f}ms decode CPU")
            for func, cpu_us in sorted(info["funcs"].items(), key=lambda x: x[1], reverse=True)[:10]:
                print(f"    {func[:65]}: {cpu_us/1000:.1f}ms")
    else:
        print("No significant decode activity detected in this profile.")

    # =========================================================================
    # 11. Data Copy / memcpy Analysis
    # =========================================================================
    print("\n" + "=" * 90)
    print("11. DATA COPY / memcpy ANALYSIS (main thread)")
    print("=" * 90)

    memcpy_cpu = 0
    memcpy_callers = defaultdict(float)
    for i in range(main_num):
        cpu = main_cpu[i] if main_cpu[i] else 0
        if cpu <= 0:
            continue
        stack_idx = main_stacks[i] if i < len(main_stacks) else None
        if stack_idx is None:
            continue
        frames = analyzer.resolve_stack_fast(stack_idx)
        for depth, (func_name, lib_name) in enumerate(frames):
            if "memcpy" in func_name.lower() or "memmove" in func_name.lower():
                memcpy_cpu += cpu
                # Find the PXView caller
                for d2 in range(depth + 1, len(frames)):
                    fn2, ln2 = frames[d2]
                    if ln2 == "PXView.exe" or ln2 == "libsigrok.dll":
                        memcpy_callers[f"{fn2} [{ln2}]"] += cpu
                        break
                break

    if memcpy_cpu > 0:
        print(f"Total memcpy/memmove CPU on main thread: {memcpy_cpu/1000:.1f}ms")
        print("Called from:")
        for caller, cpu_us in sorted(memcpy_callers.items(), key=lambda x: x[1], reverse=True)[:10]:
            print(f"  {caller[:60]}: {cpu_us/1000:.1f}ms")
    else:
        print("No significant memcpy activity on main thread.")

    # =========================================================================
    # 12. Summary
    # =========================================================================
    print("\n" + "=" * 90)
    print("12. ROOT CAUSE SUMMARY")
    print("=" * 90)

    cats = result["category_cpu"]
    if total_lag_cpu > 0:
        print(f"\nLag period total CPU: {total_lag_cpu/1000:.1f}ms")
        print("\nCategory breakdown:")
        for cat, cpu_us in sorted(cats.items(), key=lambda x: x[1], reverse=True):
            pct = cpu_us / total_lag_cpu * 100
            bar = "#" * int(pct / 2)
            print(f"  {cat:<25} {cpu_us/1000:>8.1f}ms  {pct:>5.1f}%  {bar}")

    print("\n--- Key Findings ---")

    # Check main bottleneck categories
    findings = []

    render_cpu = cats.get("qt_gui_render", 0) + cats.get("qt_widgets", 0) + cats.get("gdi_render", 0)
    text_cpu = cats.get("text_rendering_dwrite", 0) + cats.get("text_shaping", 0)
    heap_cpu = cats.get("heap_alloc", 0)
    lock_cpu = cats.get("lock_contention", 0)
    pxview_cpu = cats.get("pxview_app", 0)
    memcpy_cat_cpu = cats.get("memory_copy", 0)
    python_cpu = cats.get("python_decoder", 0)
    c_decoder_cpu = cats.get("c_decoder_engine", 0)

    if total_lag_cpu > 0:
        if render_cpu / total_lag_cpu > 0.2:
            findings.append(f"Qt/GDI rendering: {render_cpu/1000:.1f}ms ({render_cpu/total_lag_cpu*100:.1f}%) - MAJOR bottleneck")
        if text_cpu / total_lag_cpu > 0.1:
            findings.append(f"Text rendering (DWrite+HarfBuzz): {text_cpu/1000:.1f}ms ({text_cpu/total_lag_cpu*100:.1f}%) - font shaping is expensive")
        if heap_cpu / total_lag_cpu > 0.1:
            findings.append(f"Heap allocation: {heap_cpu/1000:.1f}ms ({heap_cpu/total_lag_cpu*100:.1f}%) - malloc/free overhead")
        if lock_cpu / total_lag_cpu > 0.05:
            findings.append(f"Lock contention: {lock_cpu/1000:.1f}ms ({lock_cpu/total_lag_cpu*100:.1f}%) - threads blocking on locks")
        if pxview_cpu / total_lag_cpu > 0.1:
            findings.append(f"PXView app code: {pxview_cpu/1000:.1f}ms ({pxview_cpu/total_lag_cpu*100:.1f}%) - check symbol addresses")
        if memcpy_cat_cpu / total_lag_cpu > 0.05:
            findings.append(f"Memory copy: {memcpy_cat_cpu/1000:.1f}ms ({memcpy_cat_cpu/total_lag_cpu*100:.1f}%) - data copying overhead")
        if python_cpu / total_lag_cpu > 0.05:
            findings.append(f"Python decoder on main thread: {python_cpu/1000:.1f}ms - decoder running on GUI thread!")
        if c_decoder_cpu / total_lag_cpu > 0.05:
            findings.append(f"C decoder engine on main thread: {c_decoder_cpu/1000:.1f}ms - decoder work blocking GUI!")

    if not findings:
        findings.append("No single dominant bottleneck found. Lag may be from cumulative small costs.")

    for i, f in enumerate(findings, 1):
        print(f"  {i}. {f}")

    print("\n--- Recommendations ---")
    recs = []
    if total_lag_cpu > 0:
        if render_cpu / total_lag_cpu > 0.2:
            recs.append("Reduce viewport repaint area; use backing store; avoid full repaint on decode updates")
        if text_cpu / total_lag_cpu > 0.1:
            recs.append("Cache text layouts (QStaticText); reduce visible annotation labels; batch text rendering")
        if heap_cpu / total_lag_cpu > 0.1:
            recs.append("Pre-allocate memory pools; use LeafBlockPool for snapshot data; reduce temp objects in paint paths")
        if memcpy_cat_cpu / total_lag_cpu > 0.05:
            recs.append("Move data copy off main thread; use shared_ptr/COW for snapshot data instead of deep copy")
        if lock_cpu / total_lag_cpu > 0.05:
            recs.append("Reduce lock hold times; use lock-free queues for decode results; avoid cross-thread sync in hot paths")
        if python_cpu / total_lag_cpu > 0.05 or c_decoder_cpu / total_lag_cpu > 0.05:
            recs.append("Ensure decoder work runs on worker threads only; never invoke decode on the GUI thread")

    for i, r in enumerate(recs, 1):
        print(f"  {i}. {r}")


if __name__ == "__main__":
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PROFILE
    output_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "lag_analysis_output.txt")

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
            main()
        finally:
            sys.stdout = old_stdout

    print(f"\nOutput saved to: {output_file}")
