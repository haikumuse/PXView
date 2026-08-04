"""Quick diagnostic script to test failing MCP APIs."""
import sys
sys.path.insert(0, '.')
from mcp_client import McpClient, McpError
import json

c = McpClient()
c.wait_for_server(timeout=10)
c.connect()

# Test 1: tool count
print(f'Tool count: {len(c.tools)}')
print(f'Tool names: {c.tool_names}')
print()

# Test 2: device operations
devices = c.get_devices()
demo = [d for d in devices if d.get('is_demo')]
if demo:
    dev_id = demo[0]['id']
    print(f'Demo device id: {dev_id}')
    print(f'Demo device: {json.dumps(demo[0], indent=2)}')
    print()

    # Connect
    try:
        r = c.connect_device(dev_id)
        print(f'connect_device result: {r}')
    except Exception as e:
        print(f'connect_device error: {e}')

    # Disconnect
    try:
        r = c.disconnect_device(dev_id)
        print(f'disconnect_device result: {r}')
    except Exception as e:
        print(f'disconnect_device error: {e}')

    # Refresh
    try:
        r = c.refresh_device_list()
        print(f'refresh_device_list result type: {type(r).__name__}')
        if isinstance(r, list):
            print(f'refresh_device_list returned list with {len(r)} items')
            if r:
                print(f'First item: {json.dumps(r[0], indent=2)}')
        else:
            print(f'refresh_device_list value: {r}')
    except Exception as e:
        print(f'refresh_device_list error: {e}')

    # Start capture minimal
    try:
        r = c.start_capture(dev_id)
        print(f'start_capture minimal result: {r}')
        try:
            c.stop_capture()
        except:
            pass
    except Exception as e:
        print(f'start_capture minimal error: {e}')

    # Start capture with logic config
    try:
        r = c.start_capture(dev_id, {'digitalChannels': [0, 1], 'digitalSampleRate': 1000000})
        print(f'start_capture logic_config result: {r}')
        c.stop_capture()
    except Exception as e:
        print(f'start_capture logic_config error: {e}')

    # Manual capture
    try:
        r = c.start_capture(dev_id, {
            'digitalChannels': [0, 1],
            'digitalSampleRate': 1000000,
        }, {
            'manualCaptureMode': {'sampleCount': 10000}
        })
        print(f'start_capture manual result: {r}')
        r2 = c.wait_capture(timeout_seconds=30, timeout=35)
        print(f'wait_capture result: {r2}')
        r3 = c.get_capture_status()
        print(f'capture_status: {r3}')
    except Exception as e:
        print(f'manual capture error: {e}')
