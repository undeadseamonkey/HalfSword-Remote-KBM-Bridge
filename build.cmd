@echo off
setlocal

if "%STEAMWORKS_SDK%"=="" (
  echo Set STEAMWORKS_SDK to the folder containing the Steamworks SDK's sdk directory.
  echo Example: set STEAMWORKS_SDK=C:\path\to\steamworks_sdk_165
  exit /b 1
)

if not exist build mkdir build

cl /nologo /EHsc /std:c++17 /W4 /MT /LD /I"%STEAMWORKS_SDK%\sdk\public" /Fe:build\HalfSwordBridge20.dll src\bridge_v20.cpp user32.lib
if errorlevel 1 exit /b 1

cl /nologo /EHsc /std:c++17 /W4 /MT /DUNICODE /D_UNICODE /DBRIDGE_DLL_NAME=L\"HalfSwordBridge20.dll\" /Fe:build\HalfSwordJoinerControls20.exe src\bridge_controls_v20.cpp src\bridge_loader_v20.cpp user32.lib comctl32.lib
if errorlevel 1 exit /b 1

echo Build completed in the build folder.
