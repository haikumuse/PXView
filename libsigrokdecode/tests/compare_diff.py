import json
import sys

for name in sys.argv[1:]:
    py = json.load(open(f'testdata/{name}/default/expected_py.json'))
    c = json.load(open(f'testdata/{name}/default/actual_c.json'))
    print(f'\n=== {name} ===')
    print(f'Python: {py["num_annotations"]} annotations')
    print(f'C: {c["num_annotations"]} annotations')

    py_by_start = {}
    for a in py['annotations']:
        key = (a['start_sample'], a['ann_class'])
        py_by_start[key] = a
    c_by_start = {}
    for a in c['annotations']:
        key = (a['start_sample'], a['ann_class'])
        c_by_start[key] = a

    for key in sorted(set(list(py_by_start.keys()) + list(c_by_start.keys()))):
        pa = py_by_start.get(key)
        ca = c_by_start.get(key)
        if pa and ca:
            if pa['texts'] != ca['texts'] or pa['end_sample'] != ca['end_sample']:
                print(f'  DIFF at {key}: Py texts={pa["texts"]}, end={pa["end_sample"]} vs C texts={ca["texts"]}, end={ca["end_sample"]}')
        elif pa:
            print(f'  ONLY Py at {key}: texts={pa["texts"]}, end={pa["end_sample"]}, class={pa["ann_class"]}')
        else:
            print(f'  ONLY C at {key}: texts={ca["texts"]}, end={ca["end_sample"]}, class={ca["ann_class"]}')
