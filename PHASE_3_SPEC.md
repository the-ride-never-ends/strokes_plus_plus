# Mouse Gesture Engine — Phase 3 Specification

## 1. Overview

### 1.1 Purpose

Phase 3 adds an embedded Lua scripting environment to the mouse gesture engine.

Phase 1 established:

```text
Gesture
  ↓
Recognition
  ↓
Application Profile
  ↓
Keyboard Action
```

Phase 2 generalized this into:

```text
Gesture
  ↓
Recognition
  ↓
Application Profile
  ↓
Action
  ↓
Automation Services
```

Phase 3 adds a programmable interface over the same automation layer:

```text
                   ┌── Built-in Action
Gesture ───────────┤
                   │
                   └── Lua Script
                           ↓
                       Lua Runtime
                           ↓
                    Automation API
                           ↓
                 Phase 2 Services
```

Lua must not duplicate the implementation of keyboard, mouse, window, process, media, volume, or virtual-desktop functionality.

Instead, Lua must call the existing Phase 2 services.

---

# 2. Goals

Phase 3 must allow users to:

1. Assign a Lua script to a gesture.
2. Write and edit Lua scripts within the application.
3. Execute Lua scripts when gestures are recognized.
4. Access information about the triggering gesture.
5. Access information about the target application.
6. Access mouse and keyboard state.
7. Send keyboard input.
8. Generate mouse input.
9. manipulate windows.
10. Launch processes.
11. Open URLs and URIs.
12. Control media.
13. Control system volume.
14. Control virtual desktops.
15. Display simple user-facing messages.
16. define reusable Lua functions.
17. load shared user scripts.
18. report syntax and runtime errors.
19. prevent a failed script from destabilizing the gesture engine.
20. preserve Lua actions between application restarts.

Phase 3 should make most ordinary automation possible without requiring changes to the C++ application.

---

# 3. Non-Goals

The following are outside Phase 3:

- Python scripting
- JavaScript scripting
- arbitrary native plugins
- external DLL loading through the supported API
- remote code execution
- network APIs
- HTTP client APIs
- filesystem automation beyond explicitly exposed functionality
- unrestricted operating-system command execution
- package managers
- LuaRocks integration
- debugging with breakpoints
- remote debugging
- macro recording
- visual workflow editor
- multi-action visual sequences
- asynchronous Lua coroutines for long-running background automation
- user-defined C/C++ plugins
- compatibility with every StrokesPlus Lua function
- complete StrokesPlus script compatibility
- unrestricted Lua native FFI

Some may be introduced later.

---

# 4. Lua Version

The application must embed a modern Lua runtime.

Recommended:

```text
Lua 5.4
```

The exact patch version should be pinned by the project.

Lua should be statically or otherwise predictably bundled with the application rather than depending on a separately installed system Lua runtime.

---

# 5. Lua Action Type

Phase 3 must introduce a Lua action.

Conceptually:

```text
IAction
├── KeyboardAction
├── ProcessAction
├── UrlAction
├── MouseAction
├── WindowAction
├── MediaAction
├── VolumeAction
├── VirtualDesktopAction
└── LuaAction
```

Example persisted definition:

```json
{
  "type": "lua",
  "script": "window.maximize()"
}
```

A Lua action participates in the same application-specific and global action resolution rules as every other action.

---

# 6. Lua Execution

When a Lua action executes:

```text
Recognized Gesture
       ↓
Action Resolver
       ↓
LuaAction
       ↓
Create Execution Context
       ↓
Execute Lua Script
       ↓
Return ActionResult
```

Successful script execution must result in a successful action result.

A syntax error or runtime error must result in a failed action result.

The gesture engine must remain operational afterward.

---

# 7. Lua Runtime Isolation

Scripts must execute through a managed Lua runtime owned by the application.

Lua code must not execute inside:

- the low-level mouse hook;
- the low-level keyboard hook;
- gesture recognition;
- overlay rendering.

Lua execution must occur through the action execution system established in Phase 2.

---

# 8. Execution Context

Every Lua invocation must receive contextual information describing the event that caused it.

At minimum the Lua environment must expose:

```text
gesture
window
application
mouse
keyboard
```

Where appropriate, context values should be snapshots captured when the gesture began or completed.

---

# 9. Gesture Context API

Lua scripts must be able to inspect the triggering gesture.

Example:

```lua
print(gesture.name)
print(gesture.score)

print(gesture.start.x)
print(gesture.start.y)

print(gesture.finish.x)
print(gesture.finish.y)
```

Required values:

```text
gesture.id
gesture.name
gesture.score

gesture.start.x
gesture.start.y

gesture.finish.x
gesture.finish.y
```

Optional useful values include:

```text
gesture.duration
gesture.point_count
gesture.distance
```

---

# 10. Application Context API

Lua must expose information about the application associated with the gesture.

Example:

```lua
print(application.process)
print(application.title)
print(application.class)
```

Required values:

```text
application.process
application.process_id
application.title
application.class
```

Where available:

```text
application.executable_path
```

These values should normally represent the application context captured when the gesture began.

---

# 11. Window API

Lua must expose the Phase 2 window functionality.

Recommended namespace:

```lua
window
```

Required functions:

```lua
window.close()
window.minimize()
window.maximize()
window.restore()
window.activate()

window.move(x, y)
window.resize(width, height)
window.move_resize(x, y, width, height)
```

By default these functions should operate on the gesture target window.

---

# 12. Explicit Window Targets

Window functions should optionally permit a target.

Conceptually:

```lua
window.maximize("gesture")
window.maximize("foreground")
```

At minimum supported symbolic targets should include:

```text
gesture
foreground
```

If no target is provided:

```text
gesture
```

should be used.

---

# 13. Window Information

Lua must be able to retrieve information about a target window.

Recommended functions:

```lua
window.title()
window.class()
window.process()
window.bounds()
window.exists()
```

Example:

```lua
local bounds = window.bounds()

print(bounds.x)
print(bounds.y)
print(bounds.width)
print(bounds.height)
```

---

# 14. Keyboard API

Lua must expose keyboard input functionality.

Recommended namespace:

```lua
keyboard
```

Required functionality:

```lua
keyboard.hotkey(...)
keyboard.press(...)
keyboard.down(...)
keyboard.up(...)
```

Examples:

```lua
keyboard.hotkey("CTRL", "W")
```

```lua
keyboard.hotkey("CTRL", "SHIFT", "T")
```

```lua
keyboard.press("F5")
```

```lua
keyboard.down("CTRL")
keyboard.press("C")
keyboard.up("CTRL")
```

---

# 15. Keyboard State

Lua should expose current keyboard modifier state.

Example:

```lua
if keyboard.is_down("SHIFT") then
    window.maximize()
end
```

Required function:

```lua
keyboard.is_down(key)
```

At minimum it must support modifier keys.

---

# 16. Mouse API

Lua must expose mouse input and position functionality.

Recommended namespace:

```lua
mouse
```

Required functions:

```lua
mouse.position()
mouse.move(x, y)

mouse.click(button)
mouse.double_click(button)

mouse.down(button)
mouse.up(button)
```

Example:

```lua
mouse.move(gesture.start.x, gesture.start.y)
mouse.click("left")
```

---

# 17. Mouse Position

Example:

```lua
local p = mouse.position()

print(p.x)
print(p.y)
```

Coordinates must use the Windows virtual desktop coordinate space.

Negative coordinates must be supported.

---

# 18. Mouse Buttons

Supported button names must include:

```text
left
right
middle
x1
x2
```

Names should be case-insensitive if practical.

Invalid button names must produce a Lua error rather than undefined behavior.

---

# 19. Process API

Lua must expose process-launch functionality.

Recommended namespace:

```lua
process
```

Required function:

```lua
process.launch(path)
```

Optional arguments must also be supported:

```lua
process.launch(path, arguments)
```

and:

```lua
process.launch(path, arguments, working_directory)
```

Example:

```lua
process.launch("wt.exe")
```

Example:

```lua
process.launch(
    "wt.exe",
    "-d C:\\Projects",
    "C:\\Projects"
)
```

---

# 20. Shell API

Lua must expose opening URLs and registered URIs.

Recommended namespace:

```lua
shell
```

Required function:

```lua
shell.open(uri)
```

Examples:

```lua
shell.open("https://example.com")
```

```lua
shell.open("ms-settings:display")
```

```lua
shell.open("mailto:test@example.com")
```

---

# 21. Media API

Lua must expose the Phase 2 media actions.

Recommended namespace:

```lua
media
```

Required functions:

```lua
media.play_pause()
media.next()
media.previous()
media.stop()
```

---

# 22. Volume API

Lua must expose system-volume controls.

Recommended namespace:

```lua
volume
```

Required functions:

```lua
volume.increase(amount)
volume.decrease(amount)
volume.toggle_mute()
```

The following should also be considered:

```lua
volume.get()
volume.set(value)
volume.is_muted()
```

If implemented, normalized values should use a clearly documented scale such as:

```text
0–100
```

---

# 23. Virtual Desktop API

Lua must expose available virtual-desktop operations.

Recommended namespace:

```lua
desktop
```

Required functions:

```lua
desktop.next()
desktop.previous()
```

Where supported:

```lua
desktop.create()
desktop.close()
```

Unsupported functionality must return or raise a defined error rather than crashing.

---

# 24. Notification API

Lua scripts should be able to provide basic feedback to the user.

Recommended namespace:

```lua
ui
```

At minimum:

```lua
ui.message(text)
```

Example:

```lua
ui.message("Gesture executed")
```

The notification should not require the settings application to be open.

---

# 25. On-Screen Display

A lightweight on-screen display function is desirable.

Example:

```lua
ui.osd("Volume: 50%")
```

The exact visual implementation is not prescribed.

The OSD should automatically disappear after a configurable or default duration.

---

# 26. Logging API

Lua scripts must be able to write diagnostic messages.

Recommended namespace:

```lua
log
```

Example functions:

```lua
log.debug(message)
log.info(message)
log.warn(message)
log.error(message)
```

Example:

```lua
log.info("Running Chrome gesture")
```

These messages should enter the application's normal diagnostic logging system.

---

# 27. Script Return Values

Lua scripts may return a value.

For Phase 3, the primary useful convention should be:

```lua
return true
```

for explicit success and:

```lua
return false
```

for explicit failure.

If the script returns nothing and completes without error, it should be considered successful.

---

# 28. Script Errors

Lua errors must be captured.

Examples include:

```text
syntax error
unknown global
invalid argument
runtime exception
unsupported operation
automation service failure
```

The user must receive enough information to identify:

```text
script
line
error message
```

where available.

---

# 29. Error Isolation

A Lua runtime failure must not:

- terminate the gesture engine;
- uninstall input hooks;
- corrupt gesture state;
- prevent future actions;
- leave synthetic input intentionally pressed.

After script failure:

```text
Executing
   ↓
Error
   ↓
Idle
```

must occur normally.

---

# 30. Protected Execution

Every user script must execute through protected Lua calls.

An uncaught Lua exception must be converted into an `ActionResult` failure.

---

# 31. Script Time Limits

Lua scripts must not be allowed to block action execution indefinitely.

The runtime must support an execution limit.

At minimum the application must be able to interrupt scripts that exceed a configured maximum execution duration or instruction limit.

Example problematic script:

```lua
while true do
end
```

must not permanently freeze action execution.

---

# 32. Default Execution Limit

The exact limit is implementation-defined.

A reasonable default should allow ordinary automation scripts while preventing accidental infinite loops.

The limit must be configurable internally and may later become user-configurable.

---

# 33. Cancellation

The action system should be capable of cancelling an executing Lua script.

At minimum cancellation should occur when:

- the application shuts down;
- the runtime detects that the execution limit has been exceeded.

Explicit user cancellation may be added if practical.

---

# 34. Standard Lua Libraries

The application should expose only the Lua standard libraries appropriate for the scripting environment.

Safe/common functionality may include:

```text
math
string
table
utf8
```

Potentially dangerous functionality must be considered separately.

---

# 35. Restricted Native Access

The supported Phase 3 Lua environment must not provide unrestricted native FFI.

The original StrokesPlus exposed Alien FFI.

The new application should instead require automation through documented APIs:

```lua
window.*
keyboard.*
mouse.*
process.*
shell.*
media.*
volume.*
desktop.*
```

This creates a stable application API and prevents scripts from depending directly on internal native implementation details.

---

# 36. Lua Global Environment

The runtime should minimize unnecessary global symbols.

Application functionality should primarily be organized into namespaces.

Preferred:

```lua
window.maximize()
keyboard.hotkey("CTRL", "W")
```

rather than:

```lua
acMaximizeWindow()
acSendKeys(...)
```

This API should remain consistent across future versions.

---

# 37. Shared Scripts

Users must be able to define reusable Lua code shared by multiple gesture actions.

Recommended directory:

```text
scripts/
```

Example:

```text
scripts/
├── init.lua
├── windows.lua
├── browser.lua
└── utilities.lua
```

The exact filesystem structure may differ.

---

# 38. Initialization Script

The application should support an optional initialization script.

Recommended:

```text
init.lua
```

This script executes when the Lua environment is initialized.

It may define reusable functions:

```lua
function close_tab()
    keyboard.hotkey("CTRL", "W")
end
```

A gesture action can then contain:

```lua
close_tab()
```

---

# 39. Initialization Failure

If `init.lua` contains an error:

- the error must be reported;
- the application must continue running;
- gestures that do not require unavailable initialization definitions should remain usable.

---

# 40. User Modules

The shared script system should permit user-defined Lua modules or script files.

Example:

```lua
local browser = require("browser")

browser.new_tab()
```

Module loading must be restricted to application-approved user script locations unless explicitly expanded later.

---

# 41. Script Search Path

Lua must not implicitly search arbitrary system directories for user modules.

The module search path should primarily include:

```text
application-provided Lua modules
user script directory
```

---

# 42. Script Storage

Lua action scripts must persist between application restarts.

Small scripts may be stored directly in gesture/profile configuration.

Example:

```json
{
  "type": "lua",
  "script": "keyboard.hotkey(\"CTRL\", \"W\")"
}
```

Larger scripts may optionally reference files.

Example:

```json
{
  "type": "lua_file",
  "path": "scripts/browser_close.lua"
}
```

File-backed Lua actions are desirable but may be deferred if inline actions satisfy the initial Phase 3 scope.

---

# 43. Lua Action Configuration

The action configuration interface must add:

```text
Lua Script
```

as an action type.

Selecting it must expose a script editor.

---

# 44. Script Editor

The script editor must support:

- multiline text;
- standard editing;
- copy/paste;
- undo/redo;
- saving;
- syntax-error reporting.

Syntax highlighting is strongly desirable.

---

# 45. Script Validation

The settings interface must allow a script to be validated before use.

Validation must identify Lua syntax errors.

Example:

```lua
if x == 1
    window.close()
end
```

should fail validation because `then` is missing.

---

# 46. Test Script

The settings interface should provide:

```text
Run/Test
```

functionality.

This allows the user to execute a script without first performing its gesture.

A test execution must clearly indicate that it is running outside a normal gesture if no real gesture context exists.

---

# 47. Test Context

When testing a script, the application may:

1. provide a synthetic test context; or
2. capture the current application/window context.

The behavior must be deterministic and documented.

Functions that require unavailable gesture information must produce a clear error or nullable value rather than undefined behavior.

---

# 48. Lua API Help

The script editor should provide access to documentation for the supported Lua API.

At minimum documentation must list:

```text
namespace
function
parameters
return value
description
```

Example:

```text
window.move(x, y [, target])

Moves the selected window to virtual-screen coordinates x, y.

target:
  "gesture"     default
  "foreground"
```

---

# 49. API Discoverability

The script editor should make available a list of namespaces:

```text
gesture
application
window
keyboard
mouse
process
shell
media
volume
desktop
ui
log
```

Autocomplete is desirable but not required for initial Phase 3 completion.

---

# 50. API Argument Validation

All Lua API bindings must validate arguments.

For example:

```lua
mouse.click("banana")
```

must produce a defined Lua error.

Likewise:

```lua
window.resize(-5, 800)
```

should reject invalid dimensions rather than forwarding unsafe values to Windows.

---

# 51. API Return Values

Functions that can meaningfully fail should expose failure to Lua.

A consistent convention must be chosen.

Recommended:

```lua
local ok, err = window.activate()

if not ok then
    log.error(err)
end
```

Alternatively, functions may raise Lua errors for failures.

Whichever convention is chosen must be applied consistently.

---

# 52. Context Objects

Context objects should behave as read-only values from Lua.

For example:

```lua
gesture.name = "fake"
```

must not alter the internal gesture.

Likewise, modifying:

```lua
application.process
```

must not mutate application state.

---

# 53. Conditional Automation

Lua must enable conditional behavior.

Example:

```lua
if application.process == "chrome.exe" then
    keyboard.hotkey("CTRL", "W")
else
    window.close()
end
```

This is the first phase where one action can choose behavior dynamically at runtime.

---

# 54. Modifier-Aware Automation

Example:

```lua
if keyboard.is_down("SHIFT") then
    window.maximize()
else
    window.minimize()
end
```

Scripts must therefore be able to combine gesture context and live input state.

---

# 55. Position-Aware Automation

Example:

```lua
if gesture.finish.x > gesture.start.x then
    desktop.next()
else
    desktop.previous()
end
```

Gesture coordinates must be accessible as numeric Lua values.

---

# 56. Application-Aware Automation

Example:

```lua
if application.process == "explorer.exe" then
    keyboard.hotkey("ALT", "LEFT")
elseif application.process == "chrome.exe" then
    keyboard.hotkey("ALT", "LEFT")
else
    window.minimize()
end
```

This functionality must not require separate application profiles.

Profiles remain supported, but scripts may perform their own contextual branching.

---

# 57. Reusable Functions

Users must be able to define reusable Lua functions.

Example:

```lua
function close_current_context()
    if application.process == "chrome.exe" then
        keyboard.hotkey("CTRL", "W")
    else
        window.close()
    end
end
```

Gesture actions may call such functions when they exist in the runtime environment.

---

# 58. Runtime Lifecycle

The implementation must define when Lua state is created and destroyed.

A persistent runtime is recommended so that initialization code and reusable functions do not need to reload for every gesture.

Conceptually:

```text
Application Startup
       ↓
Create Lua Runtime
       ↓
Register APIs
       ↓
Load init.lua
       ↓
Runtime Ready
       ↓
Execute Gesture Scripts
       ↓
Application Shutdown
       ↓
Destroy Runtime
```

---

# 59. Runtime Reload

Users must be able to reload scripts without restarting the gesture engine.

At minimum, saving shared scripts or initialization scripts should permit a Lua-runtime reload.

The reload operation must:

1. stop or complete current script execution safely;
2. create or reset the runtime;
3. register application APIs;
4. reload initialization code;
5. report initialization errors.

---

# 60. State Persistence Within Lua

Phase 3 may permit Lua global values to persist for the lifetime of the Lua runtime.

Example:

```lua
counter = counter or 0
counter = counter + 1
```

A later invocation may observe the incremented value.

This state is not required to persist across application restarts.

---

# 61. Configuration Reload Behavior

Reloading the Lua runtime may clear runtime-only Lua state.

This behavior should be documented.

Persistent user configuration must remain unaffected.

---

# 62. Sequential Script Execution

Initial Phase 3 behavior should execute scripts sequentially within a Lua runtime.

Two scripts must not mutate the same Lua state concurrently.

If gestures occur rapidly:

```text
Script A
   ↓
Script B
   ↓
Script C
```

may be queued.

---

# 63. Script Queue

If another Lua action is triggered while a script is executing, the application should:

1. queue the next script; or
2. reject it according to a defined policy.

Queueing is preferred for Phase 3.

The queue must be bounded or otherwise protected from unbounded growth.

---

# 64. Long-Running Script Behavior

Long-running scripts must not block:

- mouse input;
- gesture recognition;
- overlay rendering;
- system tray interaction.

Only Lua action execution may be delayed.

---

# 65. Script Security Boundary

The supported Lua API is an automation interface, not a security sandbox against intentionally malicious local users.

However, Phase 3 should reduce accidental or unnecessary native access by exposing only documented capabilities.

Users should not require native pointers, `HWND` manipulation, or Win32 FFI for ordinary automation.

---

# 66. Existing Action Compatibility

All Phase 1 and Phase 2 actions must continue working without Lua.

Users must not be required to rewrite existing actions as scripts.

For example:

```text
Keyboard Action
```

remains a first-class action rather than being internally replaced in configuration with:

```lua
keyboard.hotkey(...)
```

---

# 67. Lua and Built-In Action Parity

Where Phase 2 exposes a built-in action, Lua should expose equivalent functionality when practical.

Required parity includes:

```text
Keyboard
Process
URL/URI
Mouse
Window
Media
Volume
Virtual Desktop
```

This ensures Lua can compose existing capabilities.

---

# 68. Example Lua Scripts

## Close Tab or Window

```lua
if application.process == "chrome.exe"
    or application.process == "msedge.exe"
    or application.process == "firefox.exe" then

    keyboard.hotkey("CTRL", "W")
else
    window.close()
end
```

---

## Direction-Based Desktop Navigation

```lua
if gesture.finish.x > gesture.start.x then
    desktop.next()
else
    desktop.previous()
end
```

---

## Launch Terminal

```lua
process.launch("wt.exe")
```

---

## Open Project Directory

```lua
process.launch(
    "explorer.exe",
    "C:\\Projects"
)
```

---

## Move Window to Gesture Position

```lua
local bounds = window.bounds()

window.move(
    gesture.finish.x - bounds.width / 2,
    gesture.finish.y - bounds.height / 2
)
```

---

## Modifier-Sensitive Gesture

```lua
if keyboard.is_down("SHIFT") then
    window.maximize()
elseif keyboard.is_down("CTRL") then
    window.minimize()
else
    window.restore()
end
```

---

## Mouse Interaction

```lua
mouse.move(
    gesture.start.x,
    gesture.start.y
)

mouse.click("left")
```

---

## Media Gesture

```lua
media.play_pause()
ui.osd("Play / Pause")
```

---

# 69. Phase 3 Acceptance Scenarios

## Scenario 1: Execute simple Lua action

Given a gesture contains:

```lua
window.maximize()
```

When the gesture is recognized  
Then the gesture target window is maximized.

---

## Scenario 2: Send keyboard shortcut

Given a gesture contains:

```lua
keyboard.hotkey("CTRL", "W")
```

When the gesture executes  
Then Ctrl+W is sent.

---

## Scenario 3: Use gesture context

Given a script contains:

```lua
if gesture.finish.x > gesture.start.x then
    desktop.next()
else
    desktop.previous()
end
```

When a rightward gesture executes  
Then the script selects the next-desktop operation.

---

## Scenario 4: Use application context

Given a script branches on:

```lua
application.process
```

When the gesture executes in Chrome  
Then the process value identifies Chrome  
And the corresponding script branch executes.

---

## Scenario 5: Call reusable function

Given `init.lua` defines:

```lua
function close_tab()
    keyboard.hotkey("CTRL", "W")
end
```

And a gesture action contains:

```lua
close_tab()
```

When the gesture executes  
Then Ctrl+W is sent.

---

## Scenario 6: Syntax error

Given a Lua action contains invalid syntax  
When the action is validated or executed  
Then a syntax error is reported  
And no automation action executes  
And the gesture engine remains operational.

---

## Scenario 7: Runtime error

Given a Lua action calls an invalid API argument  
When the script executes  
Then a runtime error is reported  
And the gesture engine returns to Idle.

---

## Scenario 8: Infinite script

Given a Lua action contains:

```lua
while true do
end
```

When it executes  
Then the configured execution limit is eventually reached  
And execution is interrupted  
And the gesture engine remains responsive.

---

## Scenario 9: Lua script failure followed by normal action

Given a Lua gesture action fails  
When the user subsequently performs a gesture mapped to a built-in Phase 2 action  
Then the built-in action executes normally.

---

## Scenario 10: Runtime reload

Given the user changes `init.lua`  
When the Lua runtime is reloaded  
Then the new initialization script is loaded  
And subsequent Lua actions can use its definitions.

---

# 70. Definition of Phase 3 Complete

Phase 3 is complete when:

- Lua is embedded in the application.
- Lua actions can be assigned to gestures.
- Lua actions persist across restarts.
- Scripts execute through the existing action-execution system.
- Gesture context is available to Lua.
- Application context is available to Lua.
- Window automation is available to Lua.
- Keyboard automation is available to Lua.
- Mouse automation is available to Lua.
- Process launching is available to Lua.
- URL/URI opening is available to Lua.
- Media controls are available to Lua.
- Volume controls are available to Lua.
- Virtual desktop controls are available to Lua.
- Basic UI/OSD feedback is available to Lua.
- Lua diagnostic logging is available.
- Shared user scripts are supported.
- An initialization script is supported.
- Reusable Lua functions are supported.
- Lua syntax errors are reported.
- Lua runtime errors are reported.
- Failed scripts do not destabilize the gesture engine.
- Infinite or excessively long scripts can be interrupted.
- Lua execution does not block gesture input processing.
- The settings interface contains a multiline Lua editor.
- Scripts can be validated before being assigned.
- Scripts can be tested from the settings interface.
- Phase 1 and Phase 2 action mappings continue to function unchanged.
- Lua primarily reuses Phase 2 services rather than reimplementing Windows automation.

---

# 71. Phase 3 Architectural Outcome

At the end of Phase 3:

```text
                    Gesture
                       │
                       ▼
               Application Profile
                       │
                       ▼
                 Action Resolver
                       │
             ┌─────────┴──────────┐
             │                    │
             ▼                    ▼
       Built-in Action         LuaAction
             │                    │
             │                    ▼
             │               Lua Runtime
             │                    │
             └─────────┬──────────┘
                       ▼
                Automation API
                       │
       ┌───────────────┼────────────────┐
       │               │                │
    Keyboard         Window           Mouse
       │               │                │
    Process           Shell            Media
       │               │                │
    Volume        Virtual Desktop      UI
       │               │                │
       └───────────────┴────────────────┘
                       │
                       ▼
                    Windows
```

The major Phase 3 deliverable is therefore not simply "Lua support."

It is a stable programmable automation API over the capabilities created during Phase 2.

---

# 72. Preparation for Phase 4

After Phase 3, the engine will have three distinct layers:

```text
Gesture Input
      ↓
Action Resolution
      ↓
Automation Platform
```

with Lua providing programmable composition over that platform.

This creates the foundation for Phase 4 features such as:

```text
rocker gestures
mouse-wheel gestures
modifier gestures
multi-button gestures
multi-stroke gestures
advanced gesture sets
gesture sequencing
```

without requiring further redesign of the action system.