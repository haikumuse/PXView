import json, sys

name = sys.argv[1]
max_show = int(sys.argv[2]) if len(sys.argv) > 2 else 5

with open(f'testdata/{name}_c/default/actual_c.json') as f:
    c = json.load(f)
with open(f'testdata/{name}_c/default/expected_py.json') as f:
    p = json.load(f)

print(f'C: {c["num_annotations"]} anns, Py: {p["num_annotations"]} anns')
ca = c['annotations']
pa = p['annotations']
diffs = 0
for i in range(max(len(ca), len(pa))):
    c_item = ca[i] if i < len(ca) else None
    p_item = pa[i] if i < len(pa) else None
    if c_item != p_item:
        diffs += 1
        if diffs <= max_show:
            print(f'DIFF at [{i}]:')
            if c_item: print(f'  C: ss={c_item["start_sample"]}, es={c_item["end_sample"]}, cls={c_item["ann_class"]}, texts={c_item["texts"]}')
            else: print(f'  C: MISSING')
            if p_item: print(f'  P: ss={p_item["start_sample"]}, es={p_item["end_sample"]}, cls={p_item["ann_class"]}, texts={p_item["texts"]}')
            else: print(f'  P: MISSING')
print(f'Total diffs: {diffs}')
