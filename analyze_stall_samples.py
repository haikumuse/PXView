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
    if stack_idx is None or stack_idx < 0: return []
    if stack_idx in stack_cache: return stack_cache[stack_idx]
    
    prefix = stack_table['prefix'][stack_idx]
    frame_idx = stack_table['frame'][stack_idx]
    parent = resolve_stack(prefix) if prefix is not None and prefix != stack_idx else []
    
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
    
    leaf = f"{lib_name}!{func_name}" if lib_name else func_name
    res = parent + [leaf]
    stack_cache[stack_idx] = res
    return res

for t in data.get('threads', []):
    if not t.get('isMainThread', False): continue
    samples = t.get('samples', {})
    if not samples: continue
    
    stack_list = samples.get('stack', [])
    time_list = samples.get('time', [])
    
    for i, (s_idx, t_val) in enumerate(zip(stack_list, time_list)):
        if 10170 <= t_val <= 12992:
            stack = resolve_stack(s_idx)
            leaf = stack[-1] if stack else ""
            print(f"Time: {t_val:.2f} ms - Leaf: {leaf}")
            if "win32u.dll" in leaf or "ntdll.dll" in leaf:
                print("   Stack:")
                for frame in stack[-5:]:
                    print(f"     {frame}")
