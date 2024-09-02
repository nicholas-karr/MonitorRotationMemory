#pragma once

#include <Windows.h>
#include <shlobj.h>

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <sstream>
#include <codecvt>
#include <filesystem>
#include <fstream>

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
	// Not stable across restarts
	std::wstring deviceName;

	// Human-readable name like Wide viewing angle & High density FlexView Display 1920x1200 or Generic PnP MonitorInfo
	std::wstring monitorName;

	DWORD width;
	DWORD height;

	POINTL pos;

	DWORD rotation;

	// One of DMDO_DEFAULT (Landscape), DMDO_90 (Portrait)
	// Don't use DMDO_180, DMDO_270
	bool setProps(const MonitorInfo& ref)
	{
		DEVMODE mode = {};
		mode.dmSize = sizeof(mode);

		assert(EnumDisplaySettings(deviceName.c_str(), ENUM_CURRENT_SETTINGS, &mode));

		DEVMODE modeBackup;
		memcpy((void*)&modeBackup, (void*)&mode, sizeof(mode));

		mode.dmPelsWidth = ref.width;
		mode.dmPelsHeight = ref.height;
		mode.dmPosition.x = ref.pos.x;
		mode.dmPosition.y = ref.pos.y;
		mode.dmDisplayOrientation = ref.rotation;

		if (memcmp((void*)&modeBackup, (void*)&mode, sizeof(mode)) == 0)
		{
			std::cout << "Nothing changed\n";
			return true;
		}

		// Never use CDS_UPDATEREGISTRY, so the effects of the app do not persist across reboots
		long ret = ChangeDisplaySettingsEx(deviceName.c_str(), &mode, nullptr, 0, nullptr);

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

	bool eqProps(const MonitorInfo& rhs) const noexcept
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

	// Who would use ` in a monitor name? Hopefully nobody.
	std::replace(name.begin(), name.end(), ',', '`');

	os << name << ','
		<< i.width << ',' << i.height << ','
		<< i.pos.x << ',' << i.pos.y << ','
		<< i.rotation;
	return os;
}

// Configuration for a set of monitors
struct MonitorSetup
{
	std::vector<MonitorInfo> monitors;

	static MonitorSetup getFromCurrent(DWORD settingsMode = ENUM_CURRENT_SETTINGS)
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

				assert(EnumDisplaySettingsEx(deviceName.c_str(), settingsMode, &mode, 0));
				assert((mode.dmFields & DM_DISPLAYORIENTATION) && (mode.dmFields & DM_PELSHEIGHT) && (mode.dmFields & DM_PELSWIDTH));

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
	// This struct knows the proper endpoints for this instance
	bool setPropsFrom(const MonitorSetup& reference)
	{
		// The current main monitor is the one at 0,0
		
		// Monitors with the same (name, resolution) will all need to have the same rotation, there's no way to tell them apart
		// Iterate over monitors that have yet to be set
		std::vector<bool> consumed(reference.monitors.size());
		for (auto& monitorItr : this->monitors)
		{
			bool didSet = false;

			// Iterate over potential matches
			for (int i = 0; i < reference.monitors.size(); i++)
			{
				auto& monitorRefItr = reference.monitors[i];

				if (!consumed[i] && monitorItr.monitorName == monitorRefItr.monitorName)
				{
					consumed[i] = true;
					bool success = monitorItr.setProps(monitorRefItr);

					if (success)
					{
						didSet = true;
						break;
					}
					else
					{
						return false;
					}
				}
			}

			// Did not find the right setup earlier!
			assert(didSet);
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
	assert(hret == S_OK);

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

std::vector<MonitorSetup> readConfigFile()
{
	std::ifstream file(getConfigFilePath(), 0);

	MonitorSetup setup;
	std::vector<MonitorSetup> setups;
	std::string lineStr;
	while (std::getline(file, lineStr))
	{
		if (lineStr == "\n")
		{
			setups.push_back(setup);
			setup = {};
		}

		std::stringstream lineStream(lineStr);

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

		assert(entries.size() == 6);
		MonitorInfo next = { L"", utf8ToUtf16(entries[0]), std::stoul(entries[1]), std::stoul(entries[2]), std::stol(entries[3]), std::stol(entries[4]), std::stoul(entries[5]) };

		// Un-escape commas
		std::replace(next.monitorName.begin(), next.monitorName.end(), '`', ',');

		setup.monitors.push_back(next);
	}
	setups.push_back(setup);

	return setups;
}

// Write a new monitor config to the file
void appendConfigFile(const MonitorSetup& setup)
{
	auto folder = getConfigFileFolder();

	if (!std::filesystem::is_directory(folder) || !std::filesystem::exists(folder))
	{
		std::filesystem::create_directory(folder);
	}

	std::ofstream configOut(getConfigFilePath(), std::ios::app);

	configOut << setup << "\n";
}

// Find the MonitorSetup from the config file that has the same names and resolutions of monitors as the current setup
// Ignores rotation, as that is what is going to be imposed
const MonitorSetup* findMatching(const MonitorSetup& current, const std::vector<MonitorSetup>& setups)
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
	auto current = MonitorSetup::getFromCurrent();
	auto allSetups = readConfigFile();

	const MonitorSetup* nextSetup = findMatching(current, allSetups);

	if (nextSetup == nullptr)
	{
		// We haven't seen this setup before, write it to the file
		appendConfigFile(current);
	}
	else
	{
		std::cout << "apply?";
		assert(current.setPropsFrom(*nextSetup));
	}
}