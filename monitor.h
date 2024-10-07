#pragma once

#include "framework.h"

#include <ShlObj_core.h>
#include <KnownFolders.h>

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <sstream>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

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

struct MonitorInfo
{
	// Raw device name like \\.\DISPLAY7\Monitor0
	// Not stable across restarts, so it is not stored, but is unique
	std::wstring deviceName;

	// Human-readable name like "Wide viewing angle & High density FlexView Display 1920x1200" or "Generic PnP Monitor"
	// Stable, but not unique if two of the same monitor are used
	std::wstring monitorName;

	// Screen size, swaps when rotated 90 or 270 degrees
	DWORD width;
	DWORD height;

	// Position on the global canvas
	// 0,0 for the main monitor
	POINTL pos;

	// One of DMDO_DEFAULT (Landscape), DMDO_90 (Portrait), DMDO_180 (Flipped Landscape), or DMDO_270 (Flipped Portrait)
	DWORD rotation;

	bool operator==(const MonitorInfo& rhs) const noexcept
	{
		return this->monitorName == rhs.monitorName 
			&& this->width == this->width
			&& this->height == this->height
			&& this->pos.x == this->pos.x
			&& this->pos.y == this->pos.y
			&& this->rotation == this->rotation;
	}
};

std::ostream& operator<<(std::ostream& os, const MonitorInfo& i)
{
	std::string name = utf16ToUtf8(i.monitorName);

	// This is fine since it would be odd to have two different monitors whose names differed only
	// by whether ` or , was used.
	std::replace(name.begin(), name.end(), ',', '`');

	// Output looks like:
	// Wide viewing angle & High density FlexView Display 1920x1200,1200,1920,-1200,0,1
	os << name << ','
		<< i.width << ',' << i.height << ','
		<< i.pos.x << ',' << i.pos.y << ','
		<< i.rotation;

	return os;
}

// Update a monitor using its device name (like \\.\DISPLAY7\Monitor0) to match ref
// Returns true if something actually changed
bool applyPropsToMonitor(std::wstring deviceName, const MonitorInfo& ref)
{
	DEVMODE mode = {};
	mode.dmSize = sizeof(mode);

	// Copy current settings to mode
	nassert(EnumDisplaySettings(deviceName.c_str(), ENUM_CURRENT_SETTINGS, &mode));

	// Keep a copy of mode for sensing duplication
	DEVMODE modeBackup;
	memcpy((void*)&modeBackup, (void*)&mode, sizeof(mode));

	mode.dmPelsWidth = ref.width;
	mode.dmPelsHeight = ref.height;
	mode.dmPosition.x = ref.pos.x;
	mode.dmPosition.y = ref.pos.y;
	mode.dmDisplayOrientation = ref.rotation;

	if (memcmp((void*)&modeBackup, (void*)&mode, sizeof(mode)) == 0)
	{
		std::wcout << L"Nothing changed on monitor " << deviceName << L"\n";
		return false;
	}

	// Write with CDS_UPDATEREGISTRY and CDS_NORESET so that all monitors are changed in a single pass, than persisted at the end.
	// This prevents positions that do not yet connect to a contiguous monitor from crashing the program.
	long ret = ChangeDisplaySettingsEx(deviceName.c_str(), &mode, nullptr, (CDS_UPDATEREGISTRY | CDS_NORESET), nullptr);
	nassert(ret == DISP_CHANGE_SUCCESSFUL);

	return true;
}

// Configuration for a set of monitors
struct MonitorSetup
{
	std::vector<MonitorInfo> monitors;

	static MonitorSetup getCurrent(DWORD settingsMode = ENUM_CURRENT_SETTINGS)
	{
		MonitorSetup setup;

		DISPLAY_DEVICE device = {};
		device.cb = sizeof(device);

		for (int deviceIndex = 0; EnumDisplayDevices(0, deviceIndex, &device, 0); deviceIndex++)
		{
			// Save the adapter name, since it will be replaced with the MonitorInfo name in the device struct
			std::wstring deviceName = device.DeviceName;

			for (int monitorIndex = 0; EnumDisplayDevices(deviceName.c_str(), monitorIndex, &device, EDD_GET_DEVICE_INTERFACE_NAME); monitorIndex++)
			{
				DEVMODE mode = {};
				mode.dmSize = sizeof(mode);

				nassert(EnumDisplaySettingsEx(deviceName.c_str(), settingsMode, &mode, 0) != 0);

				// Make sure the required fields are populated
				bool populated = (mode.dmFields & DM_DISPLAYORIENTATION) && (mode.dmFields & DM_PELSHEIGHT) && (mode.dmFields & DM_PELSWIDTH) && (mode.dmFields & DM_POSITION);

				if (!populated)
				{
					std::wcout << L"Ignoring monitor " << deviceName << L"\n";
					continue;
				}

				MonitorInfo next = {};
				next.deviceName = deviceName;
				next.monitorName = device.DeviceString;
				next.width = mode.dmPelsWidth;
				next.height = mode.dmPelsHeight;
				next.pos.x = mode.dmPosition.x;
				next.pos.y = mode.dmPosition.y;
				next.rotation = mode.dmDisplayOrientation;

				setup.monitors.push_back(next);
			}
		}

		return setup;
	}

	// Using the monitor names and rotations from the reference, set the correct rotations and positions
	// This struct holds the proper endpoints for this instance
	bool setPropsFrom(const MonitorSetup& reference)
	{
		bool needToUpdate = false;

		// Iterate over monitors that have yet to be set
		std::vector<bool> consumed(reference.monitors.size());
		for (auto& monitorItr : this->monitors)
		{
			bool didSet = false;

			// Iterate over potential matches
			for (int i = 0; !didSet && i < reference.monitors.size(); i++)
			{
				auto& monitorRefItr = reference.monitors[i];

				if (!consumed[i] && monitorItr.monitorName == monitorRefItr.monitorName)
				{
					consumed[i] = true;
					bool changed = applyPropsToMonitor(monitorItr.deviceName, monitorRefItr);

					if (changed)
					{
						needToUpdate = true;
					}

					didSet = true;
				}
			}

			// Did not find the right setup earlier!
			nassert(didSet);
		}

		if (needToUpdate)
		{
			// Apply all changes from the registry
			ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
		}

		return true;
	}
};

std::ostream& operator<<(std::ostream& os, const MonitorSetup& setup)
{
	for (auto& i : setup.monitors)
	{
		os << i << '\n';
	}
	return os;
}

std::filesystem::path getConfigFileFolder()
{
	wchar_t* pathPtr;
	HRESULT hret = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &pathPtr);
	nassert(hret == S_OK);

	std::filesystem::path path = pathPtr;
	CoTaskMemFree(pathPtr);

	path.append("MonitorRotationMemory");
	
	return path;
}

std::filesystem::path getConfigFilePath()
{
	auto path = getConfigFileFolder();

	path.append("config.txt");

	return path;
}

// Parse MonitorSetup's from the config file
std::vector<MonitorSetup> readConfigFile()
{
	std::ifstream file(getConfigFilePath(), 0);

	MonitorSetup setup;
	std::vector<MonitorSetup> setups;
	std::string lineStr;
	std::stringstream lineStream;
	while (std::getline(file, lineStr))
	{
		if (lineStr == "")
		{
			setups.push_back(setup);
			setup = {};
		}

		lineStream.clear();
		lineStream << lineStr;

		std::vector<std::string> entries;
		std::string entry;
		while (std::getline(lineStream, entry, ','))
		{
			entries.push_back(entry);
		}

		if (entries.size() == 0)
		{
			continue;
		}

		nassert(entries.size() == 6);
		MonitorInfo next = { L"", utf8ToUtf16(entries[0]), std::stoul(entries[1]), std::stoul(entries[2]), std::stol(entries[3]), std::stol(entries[4]), std::stoul(entries[5]) };

		// Un-escape commas
		std::replace(next.monitorName.begin(), next.monitorName.end(), '`', ',');

		setup.monitors.push_back(next);
	}
	setups.push_back(setup);

	// Reverse so that higher priority setups (last added) are seen first
	std::reverse(setups.begin(), setups.end());

	return setups;
}

// Write a new monitor config to the file
void appendConfigToFile(const MonitorSetup& setup)
{
	auto folder = getConfigFileFolder();

	if (!std::filesystem::is_directory(folder) || !std::filesystem::exists(folder))
	{
		std::filesystem::create_directory(folder);
	}

	std::ofstream configOut(getConfigFilePath(), std::ios::app);

	configOut << setup << "\n";
}

// Find setup in setups that has the exact same names and numbers as current
const MonitorSetup* findMatchingSetup(const MonitorSetup& current, const std::vector<MonitorSetup>& setups)
{
	for (auto& setupItr : setups)
	{
		if (setupItr.monitors.size() != current.monitors.size())
		{
			// The main factor differentiating setups is number of monitors
			continue;
		}

		// "Use up" matches from i while iterating through current
		std::vector<bool> matches(current.monitors.size());
		for (auto& monitorItr : setupItr.monitors)
		{
			for (int i = 0; i < current.monitors.size(); i++)
			{
				if (!matches[i] && current.monitors[i].monitorName == monitorItr.monitorName)
				{
					matches[i] = true;
				}
			}
		}

		if (std::all_of(matches.cbegin(), matches.cend(), [](bool bit) { return bit == true; }))
		{
			// Matching setup
			return &setupItr;
		}
	}

	return nullptr;
}

// Fixes monitor rotations to match the config file
void fixMonitorRotations()
{
	bool fixed;

	while (!fixed) {
		try {
			auto current = MonitorSetup::getCurrent();
			auto allSetups = readConfigFile();

			const MonitorSetup* nextSetup = findMatchingSetup(current, allSetups);

			if (nextSetup != nullptr)
			{
				std::cout << "Applying new config\n";
				nassert(current.setPropsFrom(*nextSetup));
			}

			fixed = true;
		}
		catch (...) {
			// Sleeping in the message thread is not great
			std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		}
	}
}