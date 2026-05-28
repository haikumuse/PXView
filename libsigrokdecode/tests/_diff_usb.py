import json

with open('testdata/usb_signalling_c/default/actual_c.json') as f:
    c = json.load(f)
with open('testdata/usb_signalling_c/default/expected_py.json') as f:
    p = json.load(f)

ca = c['annotations']
pa = p['annotations']

print(f"C: {len(ca)} anns, Py: {len(pa)} anns")
print(f"\n=== Last 10 C annotations ===")
for i in range(max(0, len(ca)-10), len(ca)):
    a = ca[i]
    print(f'  [{i}] ss={a["start_sample"]}, es={a["end_sample"]}, cls={a["ann_class"]}, texts={a["texts"]}')

print(f"\n=== Py annotations around index {len(ca)} ===")
for i in range(max(0, len(ca)-3), min(len(pa), len(ca)+5)):
    a = pa[i]
    print(f'  [{i}] ss={a["start_sample"]}, es={a["end_sample"]}, cls={a["ann_class"]}, texts={a["texts"]}')

# Check if C and Py match for first 117
match_count = 0
for i in range(min(len(ca), len(pa))):
    if ca[i] == pa[i]:
        match_count += 1
print(f"\nMatching annotations in first {min(len(ca), len(pa))}: {match_count}")
