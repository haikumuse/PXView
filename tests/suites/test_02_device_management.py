"""
test_02_device_management.py - Device management tests (Layer 2).

Validates get_devices, get_channels, connect_device, disconnect_device,
refresh_device_list.
"""

import pytest

from mcp_client import McpClient, McpError
from helpers.assertions import assert_device_valid, assert_channel_valid

pytestmark = pytest.mark.p0


class TestDeviceManagement:

    def test_get_devices_returns_list(self, mcp: McpClient):
        """get_devices returns a non-empty list."""
        devices = mcp.get_devices()
        assert isinstance(devices, list), "Devices should be a list"
        assert len(devices) > 0, "Device list should not be empty"

    def test_get_devices_contains_demo(self, mcp: McpClient):
        """get_devices includes a demo device."""
        devices = mcp.get_devices()
        demo = [d for d in devices if d.get("is_demo")]
        assert len(demo) > 0, "No demo device found"

    def test_get_devices_schema(self, mcp: McpClient):
        """Each device has required fields."""
        devices = mcp.get_devices()
        for d in devices:
            assert_device_valid(d)

    def test_get_devices_include_simulation(self, mcp: McpClient):
        """get_devices with includeSimulationDevices=true includes demo."""
        devices = mcp.get_devices(include_simulation_devices=True)
        demo = [d for d in devices if d.get("is_demo")]
        assert len(demo) > 0

    def test_get_devices_has_active_flag(self, mcp: McpClient):
        """Each device has an is_active field."""
        devices = mcp.get_devices()
        for d in devices:
            assert "is_active" in d, f"Device missing 'is_active': {d}"

    def test_get_channels(self, mcp: McpClient, device_id: str):
        """get_channels returns channel list after connecting device."""
        mcp.connect_device(device_id)
        try:
            channels = mcp.get_channels()
            assert isinstance(channels, list), "Channels should be a list"
            assert len(channels) > 0, "Channel list should not be empty"
            for ch in channels:
                assert_channel_valid(ch)
        finally:
            pass  # Keep device connected for subsequent tests

    def test_get_channels_has_logic_channels(self, mcp: McpClient):
        """Channel list includes logic (digital) channels."""
        channels = mcp.get_channels()
        logic_channels = [ch for ch in channels if ch.get("type") == 0]
        assert len(logic_channels) > 0, "No logic channels found"

    def test_connect_device(self, mcp: McpClient, device_id: str):
        """connect_device succeeds for demo device."""
        result = mcp.connect_device(device_id)
        assert result is not None

    def test_connect_invalid_device(self, mcp: McpClient):
        """connect_device with invalid ID returns error."""
        with pytest.raises(McpError):
            mcp.connect_device("invalid_device_id_99999")

    def test_disconnect_device(self, mcp: McpClient, device_id: str):
        """disconnect_device succeeds."""
        mcp.connect_device(device_id)
        result = mcp.disconnect_device(device_id)
        assert result is not None
        # Reconnect for subsequent tests
        mcp.connect_device(device_id)

    def test_refresh_device_list(self, mcp: McpClient):
        """refresh_device_list returns updated device list."""
        devices = mcp.refresh_device_list()
        assert isinstance(devices, list), "Refresh should return a list"
        assert len(devices) > 0, "Refreshed device list should not be empty"
