# Strokes++

![Strokes++ logo](resources/strokes_plus_plus_logo.svg)


By GPT-5.6 Sol, Claude Opus 5, Kyle Rose

## Description
Prototype for a native Windows 11 mouse-gesture application, inspired by Strokes Plus and StrokesPlus.net. Runs in the notification area, captures configurable global mouse gestures, recognizes trained single-stroke shapes, and executes global or application-specific keyboard shortcuts. Other features present in the other two Strokes programs and general quality-of-life improvements are planned. 

Note: This program is not affiliated with the other Strokes projects, nor uses any of their source code.


## Build
Requirements:

- CMake 3.25 or newer
- A C++20 compiler (Visual Studio with the **Desktop development with C++** workload)

From a Visual Studio Developer PowerShell:

```powershell
.\run.ps1
```

That command configures the build when necessary, builds the Debug application, and starts it in the notification area. To build, run all tests, and then start it:

```powershell
.\run.ps1 -Test
```

To build and test without launching the application:

```powershell
.\run.ps1 -Test -NoRun
```

The equivalent manual commands are:

```powershell
cmake -S . -B build-vs2026 -A x64
cmake --build build-vs2026 --config Debug
ctest --test-dir build-vs2026 -C Debug --output-on-failure
```

Run `build-vs2026/Debug/GestureEngine.exe` (or the equivalent configured build directory). The application has no taskbar window. Use its notification-area icon to enable or disable gestures, open Settings, or exit. Settings run in-process for the MVP, while recognition and action execution run on the engine worker thread.

## Windows security boundary

Strokes++ is designed to run without administrator privileges. Windows User Interface Privilege Isolation (UIPI) can prevent its `SendInput` keyboard shortcuts from reaching an application running at a higher integrity level, such as an administrator-elevated window. This is an expected Windows security restriction. The action is reported as an injection failure when Windows exposes the failure. Elevated-process automation is not part of the MVP, and running Strokes++ as administrator has not been tested nor is not recommended for normal use. Windows may also deny process-image queries for elevated windows. in that case process-name profile matching is unavailable, while title and window-class criteria can still be used.

The application performs no network communication, analytics, cloud synchronization, or update checks. Configuration remains under `%LOCALAPPDATA%\StrokesPlusPlus`. Log messages for the current run in are written to `log.txt` beside `GestureEngine.exe` in structured JSON line format.

## License

This project is licensed under the MIT License.

Copyright (c) 2026 Strokes++ contributors

