#!/usr/bin/env python3
"""Test PWM decode with high maxCount."""

import json
import requests
import time

MCP_URL = "http://127.0.0.1:10110/mcp"

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
    # Add PWM decoder
    print("Adding PWM decoder...")
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
        print("  Failed!")
        return

    # Wait for decode
    print("Waiting 10s for decode...")
    time.sleep(10)

    # Get results with high maxCount
    print("Getting results (maxCount=50000)...")
    ann_result = call_tool("get_analyzer_results", {
        "analyzerId": instance_id,
        "maxCount": 50000
    }, timeout=30)
    
    if ann_result and isinstance(ann_result, list):
        print(f"  Got {len(ann_result)} annotations")
        if ann_result:
            first = ann_result[0]
            last = ann_result[-1]
            print(f"  First: [{first.get('start_sample')}-{first.get('end_sample')}]")
            print(f"  Last:  [{last.get('start_sample')}-{last.get('end_sample')}]")
            
            # Coverage
            total_samples = last.get('end_sample', 0) - first.get('start_sample', 0)
            print(f"  Total annotation span: {total_samples} samples ({total_samples/1000000:.1f}M)")
            
            # Class distribution
            classes = {}
            for ann in ann_result:
                cls = ann.get('ann_class', -1)
                classes[cls] = classes.get(cls, 0) + 1
            print(f"  Class distribution: {classes}")
    else:
        print(f"  Result: {ann_result}")

if __name__ == "__main__":
    main()
