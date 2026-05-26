#!/usr/bin/env python3
"""Diagnose WARN decoders by checking config and running C decoder."""
import json, os, subprocess, sys

DECODER_TEST = os.path.join('..', '..', 'build.dir', 'decoder_test.exe')
if not os.path.exists(DECODER_TEST):
    DECODER_TEST = os.path.join('..', '..', 'build', 'bin', 'decoder_test.exe')

warn_decoders = ['ir_nec_c', 'dali_c', 'i2s_c', 'nrzi_c', 'am230x_c', 'morse_c', 'gpib_c', 'z80_c',
                 'rgb_led_ws281x_c', 'spdif_c', 'dmx512_c', 'mipi_rffe_c', 'c2_c', 'avr_pdi_c']

for d in warn_decoders:
    config_path = os.path.join('testdata', d, 'default', 'config.json')
    if not os.path.exists(config_path):
        print(f'{d}: NO CONFIG')
        continue
    with open(config_path, 'r') as f:
        cfg = json.load(f)
    nc = cfg.get('num_channels', '?')
    sr = cfg.get('samplerate', '?')
    sc = cfg.get('sample_count', '?')
    ch = cfg.get('channels', {})
    stk = cfg.get('stack', [])
    print(f'{d}: ch={nc} sr={sr} sc={sc} stack={len(stk)} channels={list(ch.keys())[:5]}')
    
    # Try running C decoder to see what it outputs
    actual_json = os.path.join('testdata', d, 'default', 'actual_c_check.json')
    cmd = [DECODER_TEST, '-d', d, '-t', os.path.join('testdata', d, 'default'), 
           '-f', actual_json, '--tolerance', '2', '--generate-only']
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=10, encoding='utf-8', errors='replace')
        if proc.returncode == 0 and os.path.exists(actual_json):
            with open(actual_json, 'r') as f:
                data = json.load(f)
            anns = data.get('annotations', [])
            print(f'  C decoder: {len(anns)} annotations')
            if anns:
                for a in anns[:3]:
                    print(f'    sample {a.get("start_sample")}-{a.get("end_sample")}: {a.get("texts", [])}')
        else:
            err = proc.stderr.strip()[:200] if proc.stderr else 'no stderr'
            print(f'  C decoder error: {err}')
    except Exception as e:
        print(f'  C decoder exception: {e}')
    
    # Clean up
    if os.path.exists(actual_json):
        os.remove(actual_json)
    print()
