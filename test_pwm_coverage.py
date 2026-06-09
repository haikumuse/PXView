#!/usr/bin/env python3
"""Test PWM capture and decode - check decode coverage."""

import json
import requests
import time
import sys

MCP_URL = "http://127.0.0.1:10530/mcp"

def call_tool(name, args=None, timeout=60):
    """Call an MCP tool and return the result."""
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
    print("PWM Decode Coverage Test")
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

    # Step 2: Start capture
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
    }, timeout=60)
    if result is None:
        print("Failed to start capture!")
        return
    print(f"  Capture started, id={result}")

    # Step 3: Wait for capture to complete
    print("\n[3] Waiting for capture to complete...")
    time.sleep(3)

    status = call_tool("get_capture_status")
    if status:
        print(f"  Capture state: {status.get('state')}, have_data: {status.get('have_view_data')}")

    # Step 4: Add PWM decoder
    print("\n[4] Adding PWM decoder on channel 14...")
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

    # Step 5: Wait for decode to complete (longer wait)
    print("\n[5] Waiting for decode to complete (30s)...")
    for i in range(30):
        time.sleep(1)
        # Check progress
        if instance_id:
            ann = call_tool("get_analyzer_results", {
                "analyzerId": instance_id,
                "maxCount": 1
            })
            if ann and isinstance(ann, list) and len(ann) > 0:
                last_end = ann[-1].get('end_sample', 0)
                print(f"  [{i+1}s] Last annotation end_sample: {last_end}")
            else:
                print(f"  [{i+1}s] No annotations yet")

    # Step 6: Get all results with high maxCount
    print("\n[6] Getting all analyzer results...")
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
                print(f"  First: start_sample={first.get('start_sample')}, end_sample={first.get('end_sample')}")
                print(f"  Last:  start_sample={last.get('start_sample')}, end_sample={last.get('end_sample')}")
                
                # Calculate coverage
                total_range = last.get('end_sample', 0) - first.get('start_sample', 0)
                print(f"  Total annotation range: {total_range} samples")
                
                # Show first 5 and last 5
                print("\n  First 5 annotations:")
                for ann in ann_result[:5]:
                    texts = ann.get('texts', [])
                    text_str = " | ".join(str(t) for t in texts) if texts else "(no text)"
                    print(f"    [{ann.get('start_sample')}-{ann.get('end_sample')}] class={ann.get('ann_class')}: {text_str}")
                
                print("\n  Last 5 annotations:")
                for ann in ann_result[-5:]:
                    texts = ann.get('texts', [])
                    text_str = " | ".join(str(t) for t in texts) if texts else "(no text)"
                    print(f"    [{ann.get('start_sample')}-{ann.get('end_sample')}] class={ann.get('ann_class')}: {text_str}")
        else:
            print(f"  Result: {ann_result}")

    # Step 7: Get capture status for total sample count
    print("\n[7] Getting capture status...")
    status = call_tool("get_capture_status")
    if status:
        print(f"  Status: {json.dumps(status, indent=2)}")

    print("\n" + "=" * 60)
    print("Test complete!")
    print("=" * 60)

if __name__ == "__main__":
    main()
