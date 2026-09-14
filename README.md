# Strokes++

![Strokes++ logo](resources/strokes_plus_plus_logo.svg)


By GPT-5.6 Sol, Claude Opus 5, Kyle Rose

## Description
Prototype for a native Windows 11 mouse-gesture application, inspired by Strokes Plus and StrokesPlus.net. Runs in the notification area, captures configurable global mouse gestures, recognizes trained single-stroke shapes, and executes global or application-specific actions.

The action engine supports keyboard shortcut sequences, executable launches, registered URLs and
URIs, mouse input, window manipulation, media commands, output volume, and Windows virtual desktop
commands. Application-profile actions retain first-match precedence over global actions.

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

Run `build-vs2026/Debug/StrokesPlusPlus.exe` (or the equivalent configured build directory). The application has no taskbar window. Use its notification-area icon to enable or disable gestures, open Settings, or exit. Settings run in-process for the MVP. Recognition runs on the engine worker thread and actions execute on a separate action worker; the engine currently waits for each action to finish, which is being changed so that a slow action cannot delay the next gesture.

## Action configuration schema

Actions are stored under `global_actions` or a profile's `actions` object. Each mapping has a
`type`, action schema `version`, and type-specific parameters. Examples:

```json
{
  "close-tab": {"type":"keyboard","version":1,"shortcut":"CTRL+W"},
  "terminal": {
    "type":"process","version":1,"operation":"launch","path":"wt.exe",
    "arguments":"-d C:\\Projects","working_directory":"C:\\Projects"
  },
  "website": {"type":"url","version":1,"url":"https://example.com"},
  "click-origin": {
    "type":"mouse","version":1,"operation":"click","button":"left",
    "position":{"type":"gesture_start"}
  },
  "maximize": {
    "type":"window","version":1,"operation":"maximize","target":"gesture_window"
  },
  "pause": {"type":"media","version":1,"operation":"play_pause"},
  "quieter": {"type":"volume","version":1,"operation":"decrease","amount":5},
  "next-desktop": {"type":"virtual_desktop","version":1,"operation":"next"}
}
```

Mouse positions may be `current_cursor`, `gesture_start`, `gesture_end`, or `absolute`; absolute
positions include numeric `x` and `y`. Window targets may be `gesture_window`,
`foreground_window`, or `window_at_gesture_start`. Move actions add the required `x` and `y` fields,
resize actions add `width` and `height`, and move-resize actions add all four. Fields belonging to
another operation are currently applied if present, so a hand-edited move action that still carries
`width` and `height` also resizes the window.

Phase 1 keyboard records without an action-level `version` remain supported and are written in the
versioned representation on the next save. Invalid mappings are skipped independently, so valid
sibling mappings remain usable; the log identifies rejected global or profile mapping IDs.

## Windows security boundary

Strokes++ is designed to run without administrator privileges. Windows User Interface Privilege Isolation (UIPI) can prevent its `SendInput` keyboard shortcuts from reaching an application running at a higher integrity level, such as an administrator-elevated window. This is an expected Windows security restriction. The action is reported as an injection failure when Windows exposes the failure. Elevated-process automation is not part of the MVP, and running Strokes++ as administrator has not been tested nor is not recommended for normal use. Windows may also deny process-image queries for elevated windows. in that case process-name profile matching is unavailable, while title and window-class criteria can still be used.

The same integrity boundary applies to synthetic mouse, media, and virtual-desktop input. Windows
may deny foreground activation even for a valid window; this is reported as an action failure.
URLs and URIs require a registered shell handler. Volume actions require an available default
output endpoint. Virtual desktop actions use Windows 11 system shortcuts; switching at the first or
last desktop is a harmless no-op, and behavior can vary on unsupported Windows versions. Strokes++
does not yet detect whether those shortcuts are available, so a virtual desktop action reports
success whenever Windows accepts the keystrokes, even on a system where they do nothing. Runtime
capability detection is planned.

The application performs no network communication, analytics, cloud synchronization, or update checks. Configuration remains under `%LOCALAPPDATA%\StrokesPlusPlus`. Log messages for the current run in are written to `log.txt` beside `StrokesPlusPlus.exe` in structured JSON line format.

## License

This project is licensed under the MIT License.

Copyright (c) 2026 Strokes++ contributors
