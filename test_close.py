import json, subprocess, time

def mcp_call(method, params=None):
    body = {"jsonrpc":"2.0","id":1,"method":method,"params":params or {}}
    cmd = ['curl', '-s', '-X', 'POST', 'http://127.0.0.1:10530/mcp',
           '-H', 'Content-Type: application/json',
           '-d', json.dumps(body)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
    if r.returncode != 0:
        return None, f"curl error {r.returncode}"
    try:
        return json.loads(r.stdout), None
    except:
        return None, r.stdout

def call_tool(name, args=None):
    return mcp_call("tools/call", {"name": name, "arguments": args or {}})

# 1. Get devices
r, err = call_tool("get_devices")
if err: print(f"ERROR: {err}"); exit(1)
devs = json.loads(r['result']['content'][0]['text'])
demo = [d for d in devs if d['is_demo']][0]
print(f"Device: {demo['display_name']} (id={demo['id']})")

# 2. Start capture
r, err = call_tool("start_capture", {"deviceId": demo['id']})
if err: print(f"ERROR: {err}"); exit(1)
print(f"start_capture: {json.dumps(r['result'])}")

# 3. Wait for capture to complete
time.sleep(3)

# 4. Check capture status
r, err = call_tool("get_capture_status")
if err:
    print(f"get_capture_status ERROR: {err}")
    print("PXView likely crashed!")
else:
    print(f"capture_status: {json.dumps(r['result'])}")

# 5. Close capture
r, err = call_tool("close_capture")
if err:
    print(f"close_capture ERROR: {err}")
    print("PXView likely crashed!")
else:
    print(f"close_capture: {json.dumps(r['result'])}")

# 6. Verify PXView still alive
time.sleep(1)
r, err = call_tool("get_capture_status")
if err:
    print(f"VERIFY FAILED: {err}")
    print("PXView has crashed after close_capture!")
else:
    print(f"PXView still alive! status: {json.dumps(r['result'])}")
