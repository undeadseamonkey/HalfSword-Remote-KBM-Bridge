# Half Sword Remote KBM Bridge

Half Sword Remote KBM Bridge gives the Steam Remote Play Together joiner independent keyboard and mouse control of Player 2. The host keeps normal keyboard and mouse control of Player 1. It does not emulate a controller.

## Nexus Mods release

[Half Sword Remote KBM Bridge on Nexus Mods](https://www.nexusmods.com/halfsword/mods/557)

## How it works

The launcher locates the standard `HalfSwordUE5-Win64-Shipping.exe` process and loads `HalfSwordBridge20.dll` through a temporary Windows message hook. The bridge enables Steamworks Remote Play Together direct input, reads the joiner's keyboard and mouse events with `ISteamRemotePlay::GetInput`, and dispatches the corresponding Half Sword Player 2 input functions.

The loader validates the process name and expected Half Sword installation path before loading the DLL. It does not create a remote thread or write a DLL path into the game process. The bridge operates only while Half Sword is running and can be stopped from the launcher.

## Antivirus detections

The bridge must run inside Half Sword to call the game's input functions. Version 1.1 uses `SetWindowsHookExW` for that bootstrap and requests only enough process access to validate the target path. Some antivirus products may still report unfamiliar unsigned executables or DLL loading behavior. The complete launcher and bridge source is published here for review.

Version 1.1 release ZIP SHA-256:

```text
43025AAE6F28410D272144016506AD935351510E14F8A8B6CBFE48AC86084A85
```

[VirusTotal analysis for the Version 1.1 ZIP](https://www.virustotal.com/gui/file/43025aae6f28410d272144016506ad935351510e14f8a8b6cbfe48ac86084a85)

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

Version 1.1 targets Half Sword Early Access 0.6.15. It relies on game addresses and Unreal Engine function names from that build, so a Half Sword update may require corresponding source changes.

## Source layout

- `src/bridge_v20.cpp`: Steam Remote Play input capture and Player 2 game input bridge
- `src/bridge_loader_v20.cpp`: restricted Windows message-hook bootstrap
- `src/bridge_controls_v20.cpp`: launcher, controls, settings, recording, and keybind interface
- `src/keybindings_v20.h`: shared configurable keybind definitions

## Version 1.1 changes

- Replaced remote memory allocation and `CreateRemoteThread` with a Windows message-hook bootstrap.
- Retains customizable keybinds, sensitivity, Classic Alt Thrust support, input reset, and troubleshooting recordings.
- Reduces the antivirus detections associated with the Version 1.0 loader.

## Privacy and networking

The application does not include telemetry, advertising, automatic updates, or an independent network service. Network transport is provided by Steam Remote Play Together. Troubleshooting recordings are local text files created only when the user enables recording.

## License

The source code is available under the MIT License. Half Sword and Steamworks are owned by their respective rights holders.
