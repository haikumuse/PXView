import json
import sys

profile_path = "C:/Users/admin/Downloads/PXView.exe 2026-05-20 13.28 profile.json"
try:
    with open(profile_path, 'r', encoding='utf-8') as f:
        data = json.load(f)
except:
    sys.exit(1)

min_time = float('inf')
max_time = 0

for t in data.get('threads', []):
    samples = t.get('samples', {})
    times = samples.get('time', [])
    if times:
        if min(times) < min_time:
            min_time = min(times)
        if max(times) > max_time:
            max_time = max(times)

print(f"Profile Start: {min_time} ms")
print(f"Profile End: {max_time} ms")
print(f"Total Duration: {(max_time - min_time) / 1000} seconds")
