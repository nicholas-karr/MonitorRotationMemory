#pragma once

#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <sstream>
#include <codecvt>

struct Monitor
{
	// Raw device name like \\.\DISPLAY7\Monitor0
	std::wstring deviceName;

	// Human-readable name like Wide viewing angle & High density FlexView Display 1920x1200 or Generic PnP Monitor
	std::wstring monitorName;

	// Since the dimensions swap when the monitor rotates, store them sorted
	DWORD dimensionLargest;
	DWORD dimensionSmallest;

	DWORD rotation;

	// One of DMDO_DEFAULT (Landscape), DMDO_90 (Portrait)
	// Don't use DMDO_180, DMDO_270
	bool setRotation(DWORD rotation)
	{
		DEVMODE mode = {};
		mode.dmSize = sizeof(mode);

		assert(EnumDisplaySettingsEx(deviceName.c_str(), ENUM_CURRENT_SETTINGS, &mode, 0));

		if (mode.dmDisplayOrientation == rotation) {
			return false;
		}
		else {
			std::swap(mode.dmPelsHeight, mode.dmPelsWidth);
		}

		long ret = ChangeDisplaySettings(&mode, 0);

		if (ret == DISP_CHANGE_SUCCESSFUL)
		{
			this->rotation = rotation;
			return true;
		}
		else
		{
			return false;
		}
	}
};

std::vector<Monitor> listMonitors()
{
	DISPLAY_DEVICE device = {};
	device.cb = sizeof(device);

	std::vector<Monitor> monitors;

	for (int deviceIndex = 0; EnumDisplayDevices(0, deviceIndex, &device, 0); deviceIndex++)
	{
		// Save the adapter name, since it will be replaced with the monitor name in the device struct
		std::wstring deviceName = device.DeviceName;

		for (int monitorIndex = 0; EnumDisplayDevices(deviceName.c_str(), monitorIndex, &device, EDD_GET_DEVICE_INTERFACE_NAME); monitorIndex++)
		{
			DEVMODE mode = {};
			mode.dmSize = sizeof(mode);

			assert(EnumDisplaySettingsEx(deviceName.c_str(), ENUM_CURRENT_SETTINGS, &mode, 0));
			assert((mode.dmFields & DM_DISPLAYORIENTATION) && (mode.dmFields & DM_PELSHEIGHT) && (mode.dmFields & DM_PELSWIDTH));
			std::cout << "VAL " << mode.dmDisplayOrientation << ' ' << (mode.dmFields & DM_DISPLAYORIENTATION) << '\n';

			std::wcout << device.DeviceName << ", " << device.DeviceString << ", " << device.StateFlags << ", " << device.DeviceID << '\n';

			Monitor next = {};
			next.deviceName = deviceName;
			next.monitorName = device.DeviceString;
			next.dimensionLargest = max(mode.dmPelsWidth, mode.dmPelsHeight);
			next.dimensionSmallest = min(mode.dmPelsWidth, mode.dmPelsHeight);
			next.rotation = mode.dmDisplayOrientation;

			monitors.push_back(next);
		}
	}

	return monitors;
}

std::wstring utf8ToUtf16(const std::string& str)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> myconv;
	return myconv.from_bytes(str);
}

std::string utf16ToUtf8(const std::wstring& str)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

std::string serializeMonitors(const std::vector<Monitor>& monitors)
{
	std::ostringstream buf;

	for (auto& i : monitors)
	{
		// ID
		// "Generic PNP Monitor",1920,1080,0
		buf << "\"" << utf16ToUtf8(i.monitorName) << "\"," << std::to_string(i.dimensionLargest) << ',' << std::to_string(i.dimensionSmallest);
		buf << ',' << i.rotation << '\n';
	}

	std::cout << buf.str();
	return buf.str();
}