import json
import sys

profile_path = "C:/Users/admin/Downloads/PXView.exe 2026-05-20 13.28 profile.json"

try:
    with open(profile_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
except:
    sys.exit(1)

shared = data.get('shared', {})
string_array = shared.get('stringArray', [])
stack_table = shared.get('stackTable', {})
frame_table = shared.get('frameTable', {})
func_table = shared.get('funcTable', {})

stack_cache = {}

def get_leaf(stack_idx):
    if stack_idx is None or stack_idx < 0:
        return "Unknown"
    if stack_idx in stack_cache:
        return stack_cache[stack_idx]
    
    prefix = stack_table['prefix'][stack_idx]
    frame_idx = stack_table['frame'][stack_idx]
    
    if frame_idx is not None and frame_idx < len(frame_table['func']):
        func_idx = frame_table['func'][frame_idx]
        if func_idx is not None and func_idx < len(func_table['name']):
            name_idx = func_table['name'][func_idx]
            if name_idx is not None and name_idx < len(string_array):
                name = string_array[name_idx]
                stack_cache[stack_idx] = name
                return name
                
    if prefix is not None and prefix != stack_idx:
        return get_leaf(prefix)
    return "Unknown"

for t in data.get('threads', []):
    is_main = t.get('isMainThread', False)
    if not is_main:
        continue
        
    samples = t.get('samples', {})
    if not samples: continue
    
    stack_list = samples.get('stack', [])
    time_list = samples.get('time', [])
    
    start_window = min(time_list) + 6700.0 if time_list else 0
    
    max_continuous_active = 0
    current_active_duration = 0
    active_start_time = 0
    
    active_intervals = []
    
    is_active = False
    
    for s_idx, t_val in zip(stack_list, time_list):
        if t_val < start_window:
            continue
            
        leaf = get_leaf(s_idx)
        if "NtUserMsgWait" in leaf:
            if is_active:
                duration = t_val - active_start_time
                active_intervals.append(duration)
                is_active = False
        else:
            if not is_active:
                is_active = True
                active_start_time = t_val
                
    if is_active:
        duration = time_list[-1] - active_start_time
        active_intervals.append(duration)
        
    active_intervals.sort(reverse=True)
    print(f"Top 10 continuous ACTIVE (non-idle) intervals in Main Thread after 6.7s (in ms):")
    for d in active_intervals[:10]:
        print(f" - {d:.2f} ms")
