# Strokes++ 0.10.0

Strokes++ is a Windows 11 x64 mouse-gesture application that runs in the notification area.

## Highlights

- Assign keyboard, mouse, window, media, volume, virtual desktop, process, URL, and Lua actions to
  gestures. Application profiles can override global actions.
- Create Lua actions in the action editor, validate or test scripts, and share code through
  `%LOCALAPPDATA%\StrokesPlusPlus\scripts\init.lua` and restricted local modules.
- Use separate **Add Action** and **Edit Action** controls. Add lists gestures and mouse triggers
  without an existing global or application-profile action.
- Install with the NSIS executable or use the portable ZIP. Both include the Visual C++ runtime,
  project license, and Lua license notice.

## Before installing

- The installer is unsigned. Verify the SHA-256 value against the checksum file published beside
  the download.
- Strokes++ runs without administrator privileges. Windows can block automation of elevated apps.
- Virtual desktop commands use Windows shortcuts and may report success even when the system does
  not perform the requested operation.

Configuration stays in `%LOCALAPPDATA%\StrokesPlusPlus` when the installer is upgraded or removed.
For build instructions and the scripting API, see [README.md](README.md).
