import re

log_path = r"C:\Users\admin\AppData\Roaming\PXlogicV20\PXView\DSView.log"

try:
    with open(log_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    encoding = 'utf-8'
except UnicodeDecodeError:
    with open(log_path, 'r', encoding='utf-16') as f:
        lines = f.readlines()
    encoding = 'utf-16'

slow_lines = []
pattern_diag = re.compile(r"took\s+(\d+)\s+ms")
pattern_profiler = re.compile(r"took\s+(\d+)\s+ms")

for idx, line in enumerate(lines):
    if "[DIAG]" in line or "[PROFILER]" in line:
        match = pattern_diag.search(line)
        if match:
            ms = int(match.group(1))
            if ms > 20: # threshold of 20ms
                slow_lines.append((idx, ms, line.strip()))

slow_lines.sort(key=lambda x: x[1], reverse=True)
print(f"Top 50 slowest DIAG/PROFILER entries in log:")
for idx, ms, content in slow_lines[:50]:
    print(f"Line {idx} (took {ms} ms): {content}")
