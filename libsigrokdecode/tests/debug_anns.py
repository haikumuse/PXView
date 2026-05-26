#!/usr/bin/env python3
import json

for dec in ['ps2_c', 'jtag_c', 'lfast_c', 'maple_bus_c', 'qi_c', 'rvswd_c', 'sdio_c', 'usb_power_delivery_c']:
    for fname in ['expected_py.json', 'actual_c.json']:
        path = f'testdata/{dec}/default/{fname}'
        try:
            with open(path) as f:
                data = json.load(f)
            anns = data.get('annotations', [])
            print(f'{dec}/{fname}: {len(anns)} annotations')
            for a in anns[:8]:
                ss = a.get('start_sample')
                es = a.get('end_sample')
                cls = a.get('ann_class')
                texts = a.get('texts')
                print(f'  s={ss}, e={es}, cls={cls}, texts={texts}')
        except Exception as e:
            print(f'{dec}/{fname}: ERROR {e}')
    print()
