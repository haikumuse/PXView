#!/usr/bin/env python3
"""Test PWM capture - start_capture first, then add analyzer."""

import json
import requests
import time

MCP_URL = "http://127.0.0.1:10530/mcp"

def call_tool(name, args=None, timeout=60):
    payload = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "tools/call",
        "params": {
            "name": name,
            "arguments": args or {}
        }
    }
    try:
        resp = requests.post(MCP_URL, json=payload, timeout=timeout)
        data = resp.json()
        if "result" in data and "content" in data["result"]:
            text = data["result"]["content"][0]["text"]
            is_error = data["result"].get("isError", False)
            if is_error:
                print(f"  ERROR: {text}")
                return None
            try:
                return json.loads(text)
            except:
                return text
        return data
    except Exception as e:
        print(f"  EXCEPTION: {e}")
        return None

def main():
    print("=" * 60)
    print("PWM Decode Test (capture FIRST, then add analyzer)")
    print("=" * 60)

    # Step 1: Get devices
    print("\n[1] Getting device list...")
    devs = call_tool("get_devices")
    if not devs:
        print("Failed to get devices")
        return

    px_dev = None
    for d in devs:
        if not d.get("is_demo"):
            px_dev = d
            break

    if not px_dev:
        print("No PX device found!")
        return

    device_id = px_dev["id"]
    print(f"  Using device: {px_dev['display_name']} (id={device_id})")

    # Step 2: Start capture FIRST (this creates the session)
    print("\n[2] Starting capture on channel 14 at 100MHz...")
    result = call_tool("start_capture", {
        "deviceId": device_id,
        "logicDeviceConfiguration": {
            "digitalChannels": [14],
            "digitalSampleRate": 100000000
        },
        "captureConfiguration": {
            "manualCaptureMode": {}
        }
    }, timeout=120)
    if result is None:
        print("Failed to start capture!")
        return
    print(f"  Capture started, id={result}")

    # Step 3: Wait for capture to complete
    print("\n[3] Waiting for capture to complete...")
    time.sleep(10)

    # Step 4: Add PWM decoder AFTER capture
    print("\n[4] Adding PWM decoder on channel 14 (after capture)...")
    decoder_result = call_tool("add_analyzer", {
        "analyzerName": "pwm_c",
        "settings": {
            "channelMap": {
                "data": 14
            }
        }
    }, timeout=120)
    instance_id = None
    if decoder_result:
        print(f"  Decoder added: {decoder_result}")
        if isinstance(decoder_result, (int, float)):
            instance_id = str(int(decoder_result))
        elif isinstance(decoder_result, dict):
            instance_id = decoder_result.get("instance_id", decoder_result.get("id", ""))
        elif isinstance(decoder_result, str):
            instance_id = decoder_result
    else:
        print("  Failed to add PWM decoder!")
        return

    # Step 5: Wait for decode to complete
    print("\n[5] Waiting for decode to complete...")
    time.sleep(5)

    # Step 6: Get results
    print("\n[6] Getting analyzer results...")
    if instance_id:
        ann_result = call_tool("get_analyzer_results", {
            "analyzerId": instance_id,
            "maxCount": 10000
        }, timeout=30)
        if ann_result and isinstance(ann_result, list):
            print(f"  Got {len(ann_result)} annotations")
            if ann_result:
                first = ann_result[0]
                last = ann_result[-1]
                print(f"  First: [{first.get('start_sample')}-{first.get('end_sample')}] {first.get('texts', [])}")
                print(f"  Last:  [{last.get('start_sample')}-{last.get('end_sample')}] {last.get('texts', [])}")
        else:
            print(f"  Result: {ann_result}")

    print("\n" + "=" * 60)
    print("Test complete! Check GUI for decode track with data.")
    print("=" * 60)

if __name__ == "__main__":
    main()
