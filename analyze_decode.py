import json
from collections import defaultdict
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
    samples_dict = t.get('samples', {})
    if not samples_dict or 'stack' not in samples_dict:
        continue
    
    stack_list = samples_dict['stack']
    time_list = samples_dict.get('time', [])
    weight_list = samples_dict.get('weight', [1] * len(stack_list))
    
    if not stack_list or not time_list:
        continue
        
    min_time = min(time_list)
    start_window = min_time + 6700.0
    
    for stack_idx, time_val, weight in zip(stack_list, time_list, weight_list):
        if stack_idx is None:
            continue
        if time_val >= start_window:
            try:
                call_stack = resolve_stack(stack_idx)
            except:
                continue
            if not call_stack:
                continue
            
            # check if any frame contains 'python' or 'srd' or 'decode'
            for frame in call_stack:
                if 'python' in frame.lower() or 'srd' in frame.lower() or 'decode' in frame.lower():
                    print(f"FOUND THREAD {t.get('tid')} DOING DECODING! Frame: {frame}")
                    sys.exit(0)
