# Strokes++

A native Windows 11 mouse-gesture application. The MVP runs in the notification area,
captures configurable global mouse gestures, recognizes trained single-stroke shapes,
and executes global or application-specific keyboard shortcuts.

## Build

Requirements:

- CMake 3.25 or newer
- A C++20 compiler (Visual Studio with the **Desktop development with C++** workload)

From a Visual Studio Developer PowerShell:

```powershell
.\run.ps1
```

That command configures the build when necessary, builds the Debug application, and
starts it in the notification area. To build, run all tests, and then start it:

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

Run `build-vs2026/Debug/GestureEngine.exe` (or the equivalent configured build directory). The
application has no taskbar window; use its notification-area icon to enable or disable
gestures, open Settings, or exit. Settings run in-process for the MVP, while recognition
and action execution run on the engine worker thread.

## Windows security boundary

Strokes++ is designed to run without administrator privileges. Windows User Interface
Privilege Isolation (UIPI) can prevent its `SendInput` keyboard shortcuts from reaching
an application running at a higher integrity level, such as an administrator-elevated
window. This is an expected Windows security restriction; the action is reported as an
injection failure when Windows exposes the failure. Elevated-process automation is not
part of the MVP, and running Strokes++ as administrator is not recommended for normal use.

The application performs no network communication, analytics, cloud synchronization, or
update checks. Configuration and JSON Lines logs remain under
`%LOCALAPPDATA%\StrokesPlusPlus`.
