# Mouse Gesture Engine — Phase 2 Specification

## 1. Overview

### 1.1 Purpose

Phase 2 expands the working MVP from a gesture-to-keyboard-shortcut application into a general-purpose Windows action engine.

The MVP established the following pipeline:

```text
Input
  ↓
Gesture Capture
  ↓
Gesture Recognition
  ↓
Application Context
  ↓
Action Resolution
  ↓
Keyboard Shortcut
```

Phase 2 extends the final stage into a generalized action system:

```text
Input
  ↓
Gesture Recognition
  ↓
Application Context
  ↓
Action Resolution
  ↓
Action Definition
  ↓
Action Execution
      ├── Keyboard
      ├── Process
      ├── URL
      ├── Mouse
      ├── Window
      ├── Media
      ├── Volume
      └── Virtual Desktop
```

The primary goal is to establish an extensible action architecture that can later serve as the foundation for Lua scripting, macros, plugins, and higher-level automation.

---

# 2. Goals

Phase 2 must allow a user to map gestures to actions beyond keyboard shortcuts.

The application must support:

1. Launching executables.
2. Opening URLs.
3. Clicking mouse buttons.
4. Moving the mouse pointer.
5. Closing windows.
6. Minimizing windows.
7. Maximizing windows.
8. Restoring windows.
9. Activating windows.
10. Moving windows.
11. Resizing windows.
12. Moving and resizing windows in one operation.
13. Controlling system media playback.
14. Controlling system volume.
15. Switching virtual desktops.
16. Creating new virtual desktops.
17. Closing virtual desktops where supported.
18. Configuring all supported action types through the application.
19. Persisting all action definitions.
20. Reporting action execution failures without destabilizing the gesture engine.

---

# 3. Non-Goals

The following remain outside Phase 2:

- Lua scripting
- Python scripting
- JavaScript scripting
- arbitrary shell scripts entered directly by users
- macro recording
- multi-action sequences
- conditional action execution
- branching actions
- loops
- delayed action sequences
- plugins
- remote actions
- cloud synchronization
- UI Automation element targeting
- OCR
- image recognition
- screen scraping
- touch input
- touchpad gestures
- multi-stroke gestures
- StrokesPlus Lua compatibility
- elevated process automation guarantees
- advanced window tiling layouts
- user-defined action types

These may be introduced in later phases.

---

# 4. Architectural Principle

Phase 2 must separate:

```text
Gesture recognition
```

from:

```text
Action execution
```

The gesture engine must not directly implement behavior such as:

```text
if gesture == "Left" then SendInput(...)
```

Instead:

```text
Gesture
  ↓
Action Resolver
  ↓
Action Definition
  ↓
Action Executor
```

The gesture engine should only know that an action was resolved.

---

# 5. Action Abstraction

Every supported action must implement a common interface.

Conceptually:

```cpp
class IAction {
public:
    virtual ~IAction() = default;

    virtual ActionResult execute(
        const ActionContext& context
    ) = 0;
};
```

The exact implementation may differ.

All action types must expose equivalent behavior through a shared abstraction.

---

# 6. Action Context

Every action execution must receive contextual information about the gesture that triggered it.

The `ActionContext` should contain at minimum:

```text
GestureContext
ApplicationContext
current cursor position
gesture start position
gesture end position
target window
foreground window
target process
modifier state
```

Conceptually:

```cpp
struct ActionContext {
    GestureContext gesture;
    ApplicationContext application;
    Point cursorPosition;
};
```

This context must allow actions to use symbolic targets such as:

```text
gesture start position
gesture end position
current cursor position
gesture target window
current foreground window
```

---

# 7. Action Result

Action execution must return a structured result.

At minimum:

```text
success
error code or category
human-readable message
```

Conceptually:

```cpp
struct ActionResult {
    bool success;
    ActionError error;
    std::string message;
};
```

An action failure must not crash the gesture engine.

---

# 8. Supported Action Types

Phase 2 must implement the following action categories:

```text
KeyboardAction
ProcessAction
UrlAction
MouseAction
WindowAction
MediaAction
VolumeAction
VirtualDesktopAction
```

---

# 9. Action Serialization

Every action must have a serializable definition.

The serialized representation should be independent of the internal C++ class layout.

JSON is the expected persistence format.

Example:

```json
{
  "type": "keyboard",
  "shortcut": "CTRL+W"
}
```

Each serialized action must include:

```text
type
action-specific parameters
```

Optional metadata may include:

```text
id
name
enabled
description
```

---

# 10. Action Factory

The system should provide one component responsible for converting serialized action definitions into executable action objects.

Conceptually:

```text
JSON
  ↓
ActionFactory
  ↓
IAction
```

For example:

```json
{
  "type": "window",
  "operation": "maximize"
}
```

becomes:

```text
WindowAction(Maximize)
```

Unknown action types must be rejected safely.

---

# 11. Existing Keyboard Action

The existing keyboard shortcut behavior from Phase 1 must be migrated into the generalized action architecture.

Keyboard shortcuts must remain supported.

Examples:

```text
CTRL+W
CTRL+T
ALT+LEFT
WIN+D
CTRL+SHIFT+TAB
```

The Phase 2 implementation must preserve existing MVP keyboard-action behavior.

---

# 12. Process Actions

## 12.1 Launch Executable

The application must support launching a local executable.

Example:

```json
{
  "type": "process",
  "operation": "launch",
  "path": "C:\\Windows\\System32\\notepad.exe"
}
```

---

## 12.2 Executable Arguments

Process actions must support optional command-line arguments.

Example:

```json
{
  "type": "process",
  "operation": "launch",
  "path": "wt.exe",
  "arguments": "-d C:\\Projects"
}
```

---

## 12.3 Working Directory

Process actions should support an optional working directory.

Example:

```json
{
  "type": "process",
  "operation": "launch",
  "path": "mytool.exe",
  "arguments": "--open config.json",
  "working_directory": "C:\\Tools"
}
```

---

## 12.4 Launch Failure

If a target executable:

- does not exist;
- cannot be executed;
- is blocked;
- returns an immediate launch error;

the action must fail gracefully.

The gesture engine must remain operational.

---

# 13. URL Actions

## 13.1 Open URL

The application must support opening a URL with the system's registered handler.

Example:

```json
{
  "type": "url",
  "url": "https://example.com"
}
```

The default browser should normally handle HTTP and HTTPS URLs.

---

## 13.2 Supported URI Schemes

The action system should permit URI schemes supported by Windows.

Examples may include:

```text
https:
http:
mailto:
ms-settings:
```

The implementation must not unnecessarily limit actions to HTTP URLs.

---

## 13.3 Invalid URL Handling

Malformed or unsupported URLs must fail safely.

---

# 14. Mouse Actions

The action system must support synthetic mouse actions.

---

## 14.1 Mouse Click

Supported mouse buttons must include:

```text
Left
Right
Middle
XButton1
XButton2
```

Example:

```json
{
  "type": "mouse",
  "operation": "click",
  "button": "left"
}
```

---

## 14.2 Double Click

The action system should support double-click operations.

Example:

```json
{
  "type": "mouse",
  "operation": "double_click",
  "button": "left"
}
```

---

## 14.3 Mouse Down

The system should support explicit button-down input.

Example:

```json
{
  "type": "mouse",
  "operation": "down",
  "button": "left"
}
```

---

## 14.4 Mouse Up

The system should support explicit button-up input.

Example:

```json
{
  "type": "mouse",
  "operation": "up",
  "button": "left"
}
```

---

# 15. Mouse Position Targets

Mouse actions must support symbolic position targets.

Supported targets should include:

```text
current_cursor
gesture_start
gesture_end
absolute
```

Example:

```json
{
  "type": "mouse",
  "operation": "click",
  "button": "left",
  "position": {
    "type": "gesture_start"
  }
}
```

---

## 15.1 Absolute Position

The action system must support absolute desktop coordinates.

Example:

```json
{
  "type": "mouse",
  "operation": "move",
  "position": {
    "type": "absolute",
    "x": 800,
    "y": 500
  }
}
```

Negative coordinates must be supported for multi-monitor desktops.

---

# 16. Mouse Movement

The system must support moving the pointer.

Example:

```json
{
  "type": "mouse",
  "operation": "move",
  "position": {
    "type": "gesture_start"
  }
}
```

The MVP of Phase 2 does not require animated or interpolated pointer movement.

Movement may occur immediately.

---

# 17. Window Service

Window manipulation should be implemented through a reusable `WindowService` or equivalent subsystem.

The action classes should delegate Windows-specific manipulation to this subsystem.

Conceptually:

```text
WindowAction
     ↓
WindowService
     ↓
Win32
```

---

# 18. Window Targets

Window actions must support symbolic target selection.

At minimum:

```text
gesture_window
foreground_window
window_at_gesture_start
```

`gesture_window` should refer to the application/window associated with the gesture session.

---

# 19. Close Window

The action system must support requesting that a window close.

Example:

```json
{
  "type": "window",
  "operation": "close",
  "target": "gesture_window"
}
```

The implementation should prefer normal window-close semantics rather than terminating the process.

---

# 20. Minimize Window

Example:

```json
{
  "type": "window",
  "operation": "minimize",
  "target": "gesture_window"
}
```

---

# 21. Maximize Window

Example:

```json
{
  "type": "window",
  "operation": "maximize",
  "target": "gesture_window"
}
```

---

# 22. Restore Window

Example:

```json
{
  "type": "window",
  "operation": "restore",
  "target": "gesture_window"
}
```

Restore must return a minimized or maximized window to its normal state where Windows permits.

---

# 23. Activate Window

The action system must support requesting foreground activation.

Example:

```json
{
  "type": "window",
  "operation": "activate",
  "target": "gesture_window"
}
```

The implementation must account for Windows foreground activation restrictions.

Failure to activate a window must be reported rather than treated as success.

---

# 24. Move Window

The system must support moving a window without resizing it.

Example:

```json
{
  "type": "window",
  "operation": "move",
  "target": "gesture_window",
  "x": 100,
  "y": 100
}
```

Coordinates must use the Windows virtual desktop coordinate system.

---

# 25. Resize Window

The system must support resizing a window without moving its origin.

Example:

```json
{
  "type": "window",
  "operation": "resize",
  "target": "gesture_window",
  "width": 1200,
  "height": 800
}
```

---

# 26. Move and Resize Window

The system must support changing position and dimensions in one operation.

Example:

```json
{
  "type": "window",
  "operation": "move_resize",
  "target": "gesture_window",
  "x": 0,
  "y": 0,
  "width": 1280,
  "height": 720
}
```

---

# 27. Window Bounds

The `WindowService` should expose the current window bounds to the action system.

Conceptually:

```text
left
top
right
bottom
width
height
```

This will support future contextual and scripted actions.

---

# 28. Monitor Information

The `WindowService` should support determining which monitor contains or primarily contains a target window.

At minimum it should expose:

```text
monitor work area
monitor bounds
monitor identifier
```

This is primarily foundational functionality for later snapping and scripting.

---

# 29. Media Actions

The system must support common system media commands.

At minimum:

```text
Play/Pause
Next Track
Previous Track
Stop
```

Example:

```json
{
  "type": "media",
  "operation": "play_pause"
}
```

---

# 30. Volume Actions

The system must support system output-volume control.

Required operations:

```text
Volume Up
Volume Down
Mute Toggle
```

Example:

```json
{
  "type": "volume",
  "operation": "mute_toggle"
}
```

---

## 30.1 Volume Step

Volume-up and volume-down actions should support a configurable amount where practical.

Example:

```json
{
  "type": "volume",
  "operation": "increase",
  "amount": 5
}
```

The precise interpretation of `amount` may depend on the audio implementation.

---

# 31. Virtual Desktop Actions

The application must support Windows virtual desktop actions where technically reliable.

Required operations:

```text
Next Desktop
Previous Desktop
```

Desired operations:

```text
Create Desktop
Close Desktop
```

Example:

```json
{
  "type": "virtual_desktop",
  "operation": "next"
}
```

---

# 32. Virtual Desktop Compatibility

Virtual desktop functionality must be isolated behind a dedicated service.

Conceptually:

```text
VirtualDesktopAction
        ↓
VirtualDesktopService
        ↓
Windows implementation
```

This is required because Windows virtual desktop APIs have historically changed between Windows versions.

Any unsupported operation must fail safely.

---

# 33. Action Services

Phase 2 should introduce reusable platform services.

Recommended services:

```text
InputService
ProcessService
ShellService
MouseService
WindowService
MediaService
AudioService
VirtualDesktopService
```

Actions should call these services rather than directly invoking Win32 APIs throughout the codebase.

---

# 34. Symbolic Values

Phase 2 must establish the concept of symbolic contextual values.

Rather than storing only literal values, action definitions may reference runtime context.

Supported symbolic position values should include:

```text
gesture_start
gesture_end
current_cursor
```

Supported symbolic window values should include:

```text
gesture_window
foreground_window
```

This system should be designed so future phases can add values such as:

```text
active_monitor
gesture_monitor
window_center
window_top_left
clipboard_text
```

without redesigning the action model.

---

# 35. Action Resolution

Application-specific and global mappings continue to follow Phase 1 precedence:

```text
1. Application-specific gesture action
2. Global gesture action
3. No action
```

The resolved value must now be a generic action definition rather than a keyboard shortcut.

---

# 36. Action Configuration UI

The settings interface must allow users to select an action type.

Conceptually:

```text
Action Type

[ Keyboard Shortcut ▼ ]

Keyboard Shortcut
Launch Program
Open URL
Mouse
Window
Media
Volume
Virtual Desktop
```

Changing the action type should display the parameters relevant to that type.

---

# 37. Keyboard Action UI

The user must be able to configure:

```text
keyboard shortcut
```

Existing Phase 1 functionality must remain available.

---

# 38. Process Action UI

The user must be able to configure:

```text
executable path
arguments
working directory
```

The UI should provide file browsing for executable selection.

---

# 39. URL Action UI

The user must be able to enter:

```text
URL or URI
```

---

# 40. Mouse Action UI

The user must be able to configure:

```text
operation
button
position target
```

Where relevant, absolute coordinates must be configurable.

---

# 41. Window Action UI

The user must be able to select:

```text
operation
target window
```

Additional position or size fields must appear for:

```text
move
resize
move_resize
```

---

# 42. Media Action UI

The user must be able to select one of:

```text
Play/Pause
Next
Previous
Stop
```

---

# 43. Volume Action UI

The user must be able to select:

```text
Increase
Decrease
Mute Toggle
```

If increase/decrease amount is configurable, the UI must expose it.

---

# 44. Virtual Desktop Action UI

The user must be able to select supported operations such as:

```text
Next Desktop
Previous Desktop
Create Desktop
Close Desktop
```

Unsupported operations should not be presented as available.

---

# 45. Action Validation

Action definitions must be validated before persistence.

Examples of invalid definitions include:

```text
process action with no executable
URL action with no URL
absolute mouse movement without coordinates
window resize without dimensions
unknown action type
unknown operation
```

Invalid actions must not be silently saved as valid.

---

# 46. Runtime Validation

Actions must also validate runtime-dependent requirements when executed.

Examples:

```text
target window no longer exists
process path was deleted
requested monitor no longer exists
desktop action unavailable
```

The system must return an action failure.

---

# 47. Action Failure Handling

If an action fails:

```text
gesture engine continues running
input hooks remain operational
gesture state returns to idle
subsequent gestures may execute
```

An action failure must not:

- crash the process;
- leave the engine in `Executing`;
- leave injected keys or mouse buttons pressed;
- disable gesture capture.

---

# 48. Action Concurrency

The action executor must define behavior when a new gesture action occurs while another action is executing.

For Phase 2, actions should normally be short-lived.

The preferred behavior is:

```text
independent short actions may execute sequentially
```

The implementation must prevent action execution from blocking the low-level input hook.

---

# 49. Input Injection Safety

Synthetic keyboard and mouse actions must guarantee cleanup.

If execution fails after sending:

```text
key down
mouse down
modifier down
```

the action system must make a reasonable attempt to send the corresponding release event.

The action engine must not intentionally leave synthetic input logically held.

---

# 50. Action Execution Thread

Actions must not execute inside the low-level input-hook callback.

Recommended flow:

```text
Input Hook
   ↓
Gesture Engine
   ↓
Action Queue
   ↓
Action Executor
```

Longer-running process launches or shell operations must not block gesture capture.

---

# 51. Persistence

Existing gesture/profile persistence must be expanded to store generic actions.

Example:

```json
{
  "profiles": [
    {
      "id": "chrome",
      "name": "Google Chrome",
      "actions": {
        "close-tab": {
          "type": "keyboard",
          "shortcut": "CTRL+W"
        },
        "terminal": {
          "type": "process",
          "operation": "launch",
          "path": "wt.exe"
        }
      }
    }
  ]
}
```

Existing Phase 1 configurations should be migrated where practical.

---

# 52. Backward Compatibility

If existing Phase 1 action configuration represents keyboard shortcuts directly, Phase 2 should either:

1. automatically migrate those definitions; or
2. continue supporting the legacy representation during configuration loading.

Users should not be required to manually recreate working Phase 1 mappings.

---

# 53. Action Type Versioning

Serialized actions should support future schema changes.

A version field is recommended at either:

```text
configuration level
```

or:

```text
individual action level
```

Example:

```json
{
  "type": "window",
  "version": 1,
  "operation": "maximize"
}
```

---

# 54. Diagnostics

The application should expose enough information to diagnose failed actions.

Useful diagnostic information includes:

```text
gesture name
profile
action type
action operation
target
result
failure reason
```

Example:

```text
Gesture: UpRight
Profile: Explorer
Action: Window.Maximize
Target: HWND 0x001203A4
Result: Failed
Reason: Window no longer exists
```

A user-facing diagnostics interface is desirable but not mandatory.

---

# 55. Testing Requirements

Each action type must be testable independently of gesture recognition.

The following should be mockable:

```text
ProcessService
ShellService
MouseService
WindowService
MediaService
AudioService
VirtualDesktopService
```

The recognizer must not be required to test action behavior.

---

# 56. Action Factory Tests

Tests must cover:

```text
valid keyboard action
valid process action
valid URL action
valid mouse action
valid window action
valid media action
valid volume action
valid virtual desktop action
unknown action type
missing required parameter
invalid operation
```

---

# 57. Process Action Tests

Tests must cover:

```text
launch executable
launch executable with arguments
launch executable with working directory
missing executable
invalid executable
launch failure
```

---

# 58. URL Action Tests

Tests must cover:

```text
valid HTTPS URL
valid HTTP URL
valid registered URI
empty URI
malformed URI
handler failure
```

---

# 59. Mouse Action Tests

Tests must cover:

```text
left click
right click
middle click
XButton1 click
XButton2 click
double click
mouse down
mouse up
absolute movement
gesture-start target
gesture-end target
negative monitor coordinates
```

---

# 60. Window Action Tests

Tests must cover:

```text
close
minimize
maximize
restore
activate
move
resize
move_resize
invalid HWND
window destroyed before execution
negative coordinates
multi-monitor coordinates
```

---

# 61. Media Action Tests

Tests must cover:

```text
play/pause
next
previous
stop
```

---

# 62. Volume Action Tests

Tests must cover:

```text
increase
decrease
mute toggle
unsupported audio state
```

---

# 63. Virtual Desktop Tests

Tests must cover:

```text
next desktop
previous desktop
create desktop if supported
close desktop if supported
operation unavailable
Windows implementation failure
```

---

# 64. Phase 2 Acceptance Scenarios

## Scenario 1: Launch an application

Given a gesture is mapped to a process launch action  
And the configured executable exists  
When the gesture is recognized  
Then the configured executable is launched.

---

## Scenario 2: Open a website

Given a gesture is mapped to a URL action  
When the gesture is recognized  
Then the configured URL is opened using the registered Windows handler.

---

## Scenario 3: Click at gesture start

Given a gesture is mapped to a left-click mouse action  
And the action position is `gesture_start`  
When the gesture executes  
Then the mouse pointer is moved to the gesture start position if required  
And a left click occurs at that location.

---

## Scenario 4: Maximize the gesture target window

Given a gesture is mapped to a maximize-window action  
When the gesture is performed over a valid target application  
Then the gesture target window is maximized.

---

## Scenario 5: Minimize the gesture target window

Given a gesture is mapped to a minimize-window action  
When the gesture executes  
Then the target window is minimized.

---

## Scenario 6: Move a window

Given a gesture maps to a move-window action  
And coordinates are configured  
When the gesture executes  
Then the target window moves to those coordinates.

---

## Scenario 7: Resize a window

Given a gesture maps to a resize-window action  
And width and height are configured  
When the gesture executes  
Then the target window is resized to the configured dimensions.

---

## Scenario 8: Control media playback

Given a gesture maps to Play/Pause  
When the gesture executes  
Then the system receives the Play/Pause media command.

---

## Scenario 9: Adjust system volume

Given a gesture maps to Volume Up  
When the gesture executes  
Then system output volume increases.

---

## Scenario 10: Change virtual desktop

Given a gesture maps to Next Desktop  
When the gesture executes  
Then Windows switches to the next virtual desktop where supported.

---

## Scenario 11: Application action overrides global action

Given a global gesture maps to a URL action  
And the same gesture maps to a process action in the active application profile  
When the gesture is performed in that application  
Then the process action executes  
And the global URL action does not execute.

---

## Scenario 12: Action failure does not break gestures

Given a gesture maps to an executable that no longer exists  
When the gesture is performed  
Then the process action reports failure  
And the gesture engine returns to idle  
And subsequent gestures continue to function.

---

# 65. Definition of Phase 2 Complete

Phase 2 is complete when all of the following are true:

- Existing keyboard actions use the generalized action architecture.
- Process-launch actions work.
- Process arguments are supported.
- URL actions work.
- Mouse clicks work.
- Mouse movement works.
- Contextual mouse positions work.
- Window close works.
- Window minimize works.
- Window maximize works.
- Window restore works.
- Window activation works where Windows permits.
- Window movement works.
- Window resizing works.
- Combined move/resize works.
- Media controls work.
- Volume controls work.
- Next/previous virtual desktop actions work.
- Additional virtual desktop operations are implemented where sufficiently reliable.
- Actions are serializable.
- Actions persist between application restarts.
- Existing Phase 1 mappings remain usable.
- Application-specific mappings continue to override global mappings.
- Invalid action definitions are rejected.
- Runtime action failures are handled safely.
- Action execution does not block low-level input processing.
- The configuration UI supports every Phase 2 action type.
- Each action subsystem has automated tests.
- Failure of any individual action does not destabilize the gesture engine.

---

# 66. Phase 2 Architectural Outcome

At the completion of Phase 2, the application architecture should resemble:

```text
Mouse / Keyboard
       ↓
Gesture Engine
       ↓
Gesture Recognizer
       ↓
Profile Resolver
       ↓
Action Resolver
       ↓
Action Definition
       ↓
Action Factory
       ↓
Action Executor
       │
       ├── KeyboardAction
       ├── ProcessAction
       ├── UrlAction
       ├── MouseAction
       ├── WindowAction
       ├── MediaAction
       ├── VolumeAction
       └── VirtualDesktopAction
                ↓
            Services
                │
                ├── InputService
                ├── ProcessService
                ├── ShellService
                ├── MouseService
                ├── WindowService
                ├── MediaService
                ├── AudioService
                └── VirtualDesktopService
```

This architecture is the primary deliverable of Phase 2.

The individual actions are important, but the larger objective is to establish a stable automation layer that later phases can call without duplicating Windows-specific behavior.

---

# 67. Preparation for Phase 3

Phase 2 must leave the action layer in a form that can later be called from Lua.

For example, Phase 3 should be able to expose:

```lua
keyboard.hotkey("CTRL", "W")

process.launch("wt.exe")

mouse.click("left")

window.maximize()

media.play_pause()

volume.increase(5)

desktop.next()
```

without implementing those Windows operations again.

Lua should become another caller of the same services built during Phase 2:

```text
Gesture Mapping ─────┐
                     │
                     ▼
                Action Services
                     ▲
                     │
Lua Runtime ─────────┘
```

No Phase 2 action API should be designed in a way that prevents this reuse.