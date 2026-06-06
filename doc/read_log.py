import os

log_path = r"C:\Users\admin\AppData\Roaming\PXlogicV20\PXView\PXView.log"

# Let's detect encoding or try both utf-8 and utf-16
try:
    with open(log_path, 'r', encoding='utf-8') as f:
        content = f.read(1000)
    print("Read with UTF-8 successful")
    encoding = 'utf-8'
except UnicodeDecodeError:
    try:
        with open(log_path, 'r', encoding='utf-16') as f:
            content = f.read(1000)
        print("Read with UTF-16 successful")
        encoding = 'utf-16'
    except Exception as e:
        print("Failed to read:", str(e))
        encoding = None

if encoding:
    with open(log_path, 'r', encoding=encoding) as f:
        lines = f.readlines()
    
    print(f"Total lines: {len(lines)}")
    profiler_lines = [line.strip() for line in lines if "PROFILER" in line or "DIAG" in line]
    print(f"Found {len(profiler_lines)} matching lines.")
    for line in profiler_lines[-100:]:
        print(line)
