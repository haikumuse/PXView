"""Simple test: configure_and_start directly with PX device."""
import json, requests, time, sys

URL = "http://127.0.0.1:10530/mcp"

def call_tool(name, args=None, timeout=10):
    body = {"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": name, "arguments": args or {}}}
    try:
        r = requests.post(URL, json=body, timeout=timeout)
        data = r.json()
        if "error" in data:
            print(f"  ERROR: {data['error']}")
            return None
        if "result" in data and "content" in data["result"]:
            text = data["result"]["content"][0]["text"]
            try:
                return json.loads(text)
            except:
                return text
        return data.get("result")
    except requests.exceptions.ConnectionError:
        print("  FATAL: PXView crashed (connection refused)")
        sys.exit(1)
    except requests.exceptions.ReadTimeout:
        print("  TIMEOUT")
        return None
    except Exception as e:
        print(f"  EXCEPTION: {e}")
        sys.exit(1)

# Step 1: Get devices
print("=== Step 1: Get devices ===")
devs = call_tool("get_devices")
px_id = None
for d in devs:
    print(f"  Device: {d['display_name']} (id={d['id']})")
    if "PX" in d['display_name']:
        px_id = d['id']
if not px_id:
    print("  No PX device found!")
    sys.exit(1)

# Step 2: Directly start capture with logicDeviceConfiguration
print("\n=== Step 2: start_capture with logicDeviceConfiguration ===")
r = call_tool("start_capture", {
    "deviceId": px_id,
    "logicDeviceConfiguration": {
        "digitalChannels": [14],
        "digitalSampleRate": 1000000
    }
}, timeout=30)
print(f"  Result: {r}")

# Step 3: Check if alive
print("\n=== Step 3: Check if PXView alive ===")
r = call_tool("get_capture_status", timeout=5)
if r:
    print(f"  Status: state={r.get('state')}, triggered={r.get('triggered')}")
else:
    print("  PXView may have crashed!")
    sys.exit(1)

print("\n=== Test complete! ===")
