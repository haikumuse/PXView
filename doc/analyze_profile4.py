import json
from collections import defaultdict
import sys

profile_path = "C:/Users/admin/Downloads/PXView.exe 2026-05-20 13.28 profile.json"

try:
    with open(profile_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
except Exception as e:
    print(f"Error loading profile: {e}")
    sys.exit(1)

shared = data.get('shared', {})
string_array = shared.get('stringArray', [])
stack_table = shared.get('stackTable', {})
frame_table = shared.get('frameTable', {})
func_table = shared.get('funcTable', {})
resource_table = shared.get('resourceTable', {})
libs = data.get('libs', [])

resource_to_lib = {}
if 'lib' in resource_table:
    for i in range(resource_table.get('length', 0)):
        lib_idx = resource_table['lib'][i]
        if lib_idx is not None and lib_idx < len(libs):
            resource_to_lib[i] = libs[lib_idx].get('name', 'UnknownLib')

stack_cache = {}

def resolve_stack(stack_idx):
    if stack_idx is None or stack_idx < 0:
        return []
    if stack_idx in stack_cache:
        return stack_cache[stack_idx]
    
    prefix = stack_table['prefix'][stack_idx]
    frame_idx = stack_table['frame'][stack_idx]
    
    parent_stack = []
    if prefix is not None and prefix != stack_idx:
        parent_stack = resolve_stack(prefix)
    
    func_name = "Unknown"
    lib_name = ""
    if frame_idx is not None and frame_idx < len(frame_table['func']):
        func_idx = frame_table['func'][frame_idx]
        if func_idx is not None and func_idx < len(func_table['name']):
            name_idx = func_table['name'][func_idx]
            if name_idx is not None and name_idx < len(string_array):
                func_name = string_array[name_idx]
            
            if 'resource' in func_table:
                res_idx = func_table['resource'][func_idx]
                if res_idx is not None and res_idx in resource_to_lib:
                    lib_name = resource_to_lib[res_idx]
    
    full_symbol = f"{lib_name}!{func_name}" if lib_name else func_name
    full_stack = parent_stack + [full_symbol]
    stack_cache[stack_idx] = full_stack
    return full_stack

threads = data.get('threads', [])
for t in threads:
    is_main = t.get('isMainThread', False)
    if not is_main:
        continue
        
    name = t.get('name')
    samples_dict = t.get('samples', {})
    if not samples_dict or 'stack' not in samples_dict:
        continue
    
    stack_list = samples_dict['stack']
    time_list = samples_dict.get('time', [])
    weight_list = samples_dict.get('weight', [1] * len(stack_list))
    
    if not stack_list or not time_list:
        continue
        
    min_time = min(time_list)
    start_window = min_time + 6700.0  # 6.7s onwards
    
    in_window_weight = 0
    in_window_exclusive = defaultdict(int)
    in_window_paths = defaultdict(int)
    
    for stack_idx, time_val, weight in zip(stack_list, time_list, weight_list):
        if stack_idx is None:
            continue
        if time_val >= start_window:
            try:
                call_stack = resolve_stack(stack_idx)
            except Exception:
                continue
            if not call_stack:
                continue
            leaf = call_stack[-1]
            in_window_exclusive[leaf] += weight
            
            # Optionally skip idle state for stacks
            if "NtUserMsgWaitForMultipleObjectsEx" not in leaf:
                in_window_paths[" -> ".join(call_stack)] += weight
                
            in_window_weight += weight
            
    print(f"\n=======================================================")
    print(f"MAIN THREAD: {name} (TID: {t.get('tid')}) - Samples >= 6.7s: {in_window_weight}")
    print(f"=======================================================")
    
    print("TOP 15 EXCLUSIVE FUNCTIONS:")
    sorted_in = sorted(in_window_exclusive.items(), key=lambda x: x[1], reverse=True)
    for func, cnt in sorted_in[:15]:
        print(f"  {cnt:5d} ({cnt/in_window_weight*100:5.2f}%) : {func}")
        
    print("\nTOP 5 ACTIVE CALL STACKS:")
    sorted_paths = sorted(in_window_paths.items(), key=lambda x: x[1], reverse=True)
    total_active = sum(in_window_paths.values())
    if total_active > 0:
        for rank, (path, cnt) in enumerate(sorted_paths[:10]):
            pct = (cnt / total_active) * 100
            print(f"\n  Rank {rank+1} (Weight: {cnt}, Percentage of Active CPU: {pct:.2f}%)")
            parts = path.split(" -> ")
            for level, part in enumerate(parts):
                if level > 2:
                    print("  " * (level-3) + f"└─ {part}")
