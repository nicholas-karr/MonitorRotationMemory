# MonitorRotationMemory
Fixes sideways laptop docking. 
Rotates sets of monitors depending on how they were saved from a persistent configuration.

## Motivation
Windows 11 is able to automatically handle moving windows and monitors on the canvas to where they last were based on their names. However, it does not do so with rotation.
If you dock your laptop by turning it on its side to save desk space, it will _stay_ sideways when you undock it. This app fixes that.

## Installation
Clone/download the repository.

Open `MonitorRotationMemory.sln` in Visual Studio Community and build the Release profile. If you build it in the Debug profile, a log terminal will always be visible.

To autostart, add a registry key to `Computer\HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run` of type REG_SZ named `MonitorRotationMemory` with a value of the path to your executable.
It should be under `x64/Release`. There is not an installer currently.

## Usage
1. Reboot or manually launch the executable. 
2. A system tray icon will be added to your task bar (Usually under the caret ^). Right click it and select "Capture Current Monitors."
3. Your current configuration has now been written to disk. Whenever the same pattern of monitor names is plugged in, they will be set to this resolution, spacing, and size.
4. To bind your second configuration, unplug/plug in a monitor and manually set the rotations and locations correctly. Then, capture it.
5. To change a binding, right click the icon and click "Pause". This will allow you to change rotations without it snapping back, and you can capture again to save it. Then, unpause.
6. If you ever messed up, right click the icon and click "Open Configuration". Clear it to reset the app.
7. Now, you can freely dock/undock your device sideways!

## Configuration Format
The config is stored at `%USERPROFILE%\AppData\Local\MonitorRotationMemory\config.txt`.  
Every line represents a monitor as a (Monitor Name, Width, Height, X Position, Y Position, Rotation) tuple separated by commas. Setups are separated by double newlines.
Setups later in the file have greater priority and will match first.

## Troubleshooting
The logs do not actually get written to disk. Upon encountering an error, the program will usually crash. 
Launch the debug version in Visual Studio to see a log console that will not disappear. Feel free to PR with bug fixes.

## Credits
Icon from https://icon-icons.com/users/JupI7ALOpv56pZQJjRpQL/icon-sets/  
MonitorRotationMemory.cpp is based on NotificationIcon.cpp from https://github.com/microsoft/Windows-classic-samples