# Half Sword Remote KBM Bridge

Half Sword Remote KBM Bridge gives the Steam Remote Play Together joiner independent keyboard and mouse control of Player 2. The host keeps normal keyboard and mouse control of Player 1. It does not emulate a controller.

## Nexus Mods release

[Half Sword Remote KBM Bridge on Nexus Mods](https://www.nexusmods.com/halfsword/mods/557)

## How it works

The launcher locates the standard `HalfSwordUE5-Win64-Shipping.exe` process and loads `HalfSwordBridge19.dll` into it. The bridge enables Steamworks Remote Play Together direct input, reads the joiner's keyboard and mouse events with `ISteamRemotePlay::GetInput`, and dispatches the corresponding Half Sword Player 2 input functions.

The loader validates the process name and expected Half Sword installation path before loading the DLL. The bridge operates only while Half Sword is running and can be stopped from the launcher.

## Antivirus detections

This project uses Windows process access, remote memory allocation, and `CreateRemoteThread` with `LoadLibraryW` to load the bridge DLL into Half Sword. Those techniques are also used by malicious programs, so some antivirus products may report a generic or heuristic detection. The complete loader and bridge source is published here for review.

Version 1.0 release ZIP SHA-256:

```text
F084936EBAF96FE19303716E096F1E57B1287A93765941392B503101CC7B81AD
```

[VirusTotal analysis for the Version 1.0 ZIP](https://www.virustotal.com/gui/file/f084936ebaf96fe19303716e096f1e57b1287a93765941392b503101cc7b81ad)

## Building

Requirements:

- Windows 10 or 11
- Visual Studio 2022 Build Tools with Desktop development with C++
- Steamworks SDK 1.65

Valve's Steamworks SDK is not included because its files are distributed under Valve's terms. Download it from the [official Steamworks SDK page](https://partner.steamgames.com/doc/sdk).

Open an x64 Native Tools Command Prompt for Visual Studio, then run:

```bat
set STEAMWORKS_SDK=C:\path\to\steamworks_sdk_165
build.cmd
```

The compiled files will be placed in `build`.

## Version compatibility

Version 1.0 targets Half Sword Early Access 0.6.1.5. It relies on game addresses and Unreal Engine function names from that build, so a Half Sword update may require corresponding source changes.

## Source layout

- `src/bridge_v19.cpp`: Steam Remote Play input capture and Player 2 game input bridge
- `src/bridge_loader_v19.cpp`: restricted Half Sword process loader
- `src/bridge_controls_v19.cpp`: launcher, controls, settings, recording, and keybind interface
- `src/keybindings_v19.h`: shared configurable keybind definitions

## Privacy and networking

The application does not include telemetry, advertising, automatic updates, or an independent network service. Network transport is provided by Steam Remote Play Together. Troubleshooting recordings are local text files created only when the user enables recording.

## License

The source code is available under the MIT License. Half Sword and Steamworks are owned by their respective rights holders.
