import json, sys

name = sys.argv[1]

with open(f'testdata/{name}_c/default/actual_c.json') as f:
    c = json.load(f)
with open(f'testdata/{name}_c/default/expected_py.json') as f:
    p = json.load(f)

print(f'C: {c["num_annotations"]} anns, Py: {p["num_annotations"]} anns')
ca = c['annotations']
pa = p['annotations']

print("\n=== ALL C annotations ===")
for i, a in enumerate(ca):
    print(f'  [{i}] ss={a["start_sample"]}, es={a["end_sample"]}, cls={a["ann_class"]}, texts={a["texts"]}')

print("\n=== ALL Py annotations ===")
for i, a in enumerate(pa):
    print(f'  [{i}] ss={a["start_sample"]}, es={a["end_sample"]}, cls={a["ann_class"]}, texts={a["texts"]}')
