# Mouse Gesture Engine MVP Specification

## 1. Overview

### 1.1 Purpose

This project will implement a modern Windows 11 mouse gesture application inspired by the behavior and capabilities of classic StrokesPlus.

The MVP will establish the complete core gesture-processing pipeline:

```text
Global Input Capture
        ↓
Gesture Activation
        ↓
Stroke Collection
        ↓
Gesture Recognition
        ↓
Application Context Resolution
        ↓
Action Selection
        ↓
Action Execution
```

The MVP is intentionally limited in scope. It will validate the low-level Windows integration, gesture recognition architecture, application-specific action routing, configuration persistence, and basic user interaction before advanced automation and scripting features are added.

---

## 2. Goals

The MVP must allow a user to:

1. Run the application in the background on Windows 11.
2. Hold a configurable mouse button to begin a gesture.
3. Draw a gesture anywhere on the desktop.
4. See the gesture rendered as an overlay while drawing.
5. Release the gesture button to finish the gesture.
6. Recognize the drawn gesture against configured gesture templates.
7. Determine which application was active when the gesture began.
8. Resolve an application-specific or global action.
9. Execute a keyboard shortcut associated with that action.
10. Configure gestures and mappings persistently.
11. Temporarily enable or disable gesture recognition.
12. Exit and control the application through a system tray icon.

---

# 3. Non-Goals

The following are explicitly outside the MVP.

- Lua scripting
- JavaScript or Python scripting
- macro recording
- touchscreen gestures
- touchpad gestures
- pen input
- multi-stroke gestures
- mouse wheel gestures
- rocker gestures
- gesture chains
- arbitrary shell scripting
- plugin architecture
- remote/network actions
- cloud synchronization
- account system
- gesture sharing
- configuration synchronization
- advanced accessibility automation
- elevated-process automation
- UI Automation integration
- application installation/update system
- complex action workflows
- clipboard automation
- text expansion
- visual macro builder
- StrokesPlus configuration import
- StrokesPlus Lua compatibility
- browser extensions
- per-monitor gesture configuration

These may be considered for later releases.

---

# 4. Target Platform

## 4.1 Operating System

Primary target:

- Windows 11 x64

The MVP does not need to support:

- Windows 10
- Windows 8
- Windows 7
- ARM64
- Wine
- Linux
- macOS

Supporting Windows 10 later should remain technically possible.

---

# 5. Technology

## 5.1 Core Language

The core application should be implemented in:

```text
C++20 or later
```

The implementation should make direct use of native Windows APIs where appropriate.

---

## 5.2 Windows APIs

Expected APIs include:

- `SetWindowsHookEx`
- `WH_MOUSE_LL`
- `WH_KEYBOARD_LL`, where required
- `SendInput`
- `GetForegroundWindow`
- `GetWindowThreadProcessId`
- process-query APIs
- window class APIs
- window title APIs
- Windows message loops
- system tray APIs

Rendering may use:

- Direct2D
- DirectComposition

Fallback implementation using simpler Win32 rendering is acceptable during early development.

---

# 6. Process Architecture

The intended long-term architecture consists of two processes.

```text
GestureEngine.exe
        │
        ├── global input hooks
        ├── gesture state machine
        ├── gesture recognition
        ├── profile matching
        ├── action execution
        ├── overlay rendering
        └── tray icon


GestureSettings.exe
        │
        ├── gesture management
        ├── application profiles
        ├── action configuration
        └── preferences
```

The MVP may initially implement both responsibilities in a single executable if this materially reduces implementation complexity.

The internal architecture must nevertheless keep engine logic separate from configuration UI logic.

---

# 7. Runtime Threads

The MVP should separate latency-sensitive input capture from gesture processing.

Recommended architecture:

```text
Input Thread
    │
    │ input events
    ▼
Event Queue
    │
    ▼
Gesture Engine Thread
    │
    ├── state machine
    ├── recognition
    ├── profile matching
    └── action selection

Overlay/UI Thread
    │
    └── visual rendering

Action Execution
    │
    └── input injection
```

The low-level mouse hook callback must not perform expensive work.

It must not perform:

- disk I/O
- gesture recognition
- regular-expression evaluation
- configuration parsing
- process launches
- long-running synchronization
- UI operations
- logging that may block

The hook should capture the event and hand it to the gesture engine as quickly as possible.

---

# 8. Functional Requirements

# 8.1 Application Startup

The program must run as a background Windows desktop application.

On startup it must:

1. Load configuration.
2. Load gesture templates.
3. Load application profiles.
4. Initialize gesture recognition.
5. Install required mouse hooks.
6. Create the gesture overlay system.
7. Create a system tray icon.
8. Enter an operational idle state.

If configuration files do not exist, default configuration files must be created.

---

# 8.2 Mouse Hook

The application must install a global low-level mouse hook using:

```cpp
SetWindowsHookEx(WH_MOUSE_LL, ...)
```

The hook must receive global mouse events regardless of which normal desktop application is foreground.

The hook must be capable of suppressing mouse events when necessary.

---

# 8.3 Gesture Activation Button

The MVP activation button will default to:

```text
Right Mouse Button
```

The activation button must be configurable.

At minimum the configuration model should permit:

- right mouse button
- middle mouse button
- mouse XButton1
- mouse XButton2

Only one primary gesture activation button is required for the MVP.

---

# 8.4 Normal Mouse Button Behavior

Using the activation button for gestures must not eliminate ordinary button behavior.

Example using the right mouse button:

```text
RMB down
    ↓
RMB up without gesture movement
    ↓
normal right click
```

A normal right-click must behave as though the gesture application were not running.

The program may suppress the original mouse events and recreate the click using `SendInput` if necessary.

---

# 8.5 Gesture Movement Threshold

A mouse button press must not immediately become a gesture.

The application must distinguish between:

```text
click
```

and:

```text
gesture
```

using a configurable movement threshold.

Example:

```text
RMB down
    ↓
mouse moves less than threshold
    ↓
RMB up
    ↓
normal right click
```

Versus:

```text
RMB down
    ↓
mouse moves beyond threshold
    ↓
gesture begins
```

The threshold should be represented in physical or DPI-adjusted screen pixels.

---

# 8.6 Gesture State Machine

Gesture handling must use an explicit state machine.

Required states:

```text
Idle
ButtonPending
Capturing
Recognizing
Executing
Cancelled
```

Primary transitions:

```text
Idle
 │
 │ activation button down
 ▼
ButtonPending
 │
 ├── button release before threshold
 │        ↓
 │    emulate normal click
 │        ↓
 │       Idle
 │
 └── movement exceeds threshold
          ↓
       Capturing
          │
          ├── cancel
          │     ↓
          │  Cancelled
          │     ↓
          │    Idle
          │
          └── activation button released
                ↓
            Recognizing
                ↓
             Executing
                ↓
               Idle
```

Invalid state transitions must not trigger actions.

---

# 8.7 Gesture Session

Each gesture must be represented by a `GestureSession` or equivalent object.

At minimum it should contain:

```text
activationButton
startPosition
currentPosition
capturedPoints
startTimestamp
lastMovementTimestamp
targetWindow
targetProcess
modifierState
currentState
```

The target application context must be captured when the gesture begins rather than after the gesture completes.

---

# 8.8 Point Collection

While the gesture is in `Capturing` state, the application must record the mouse trajectory.

A point must contain at least:

```text
x
y
```

Optional fields may include:

```text
timestamp
pressure
velocity
```

Pressure is not required for mouse input.

---

# 8.9 Point Filtering

The program must avoid storing meaningless high-frequency mouse noise.

A configurable minimum point distance must be used.

For example, if the previous accepted point is:

```text
100, 100
```

the following movement:

```text
101, 100
```

may be discarded while:

```text
106, 103
```

may be accepted.

The exact filtering algorithm is implementation-defined.

---

# 8.10 Maximum Points

The engine must support a maximum number of captured points per gesture.

If this limit is reached, the implementation may:

- resample the existing stroke, or
- stop recording redundant points.

It must not crash or allocate memory without bounds.

---

# 9. Gesture Recognition

## 9.1 Recognition Model

The MVP must recognize arbitrary single-stroke gestures.

The recognizer should use a geometric template-matching algorithm similar to the `$1 Unistroke Recognizer`.

Recognition must not be limited to predefined directional tokens such as:

```text
L
R
U
D
```

The user must be able to define arbitrary shapes.

Examples:

```text
L shape

Z shape

circle

caret

down-right hook
```

---

# 9.2 Gesture Normalization

Before comparison, the gesture should undergo normalization.

Expected stages:

```text
raw stroke
    ↓
remove redundant points
    ↓
resample to fixed point count
    ↓
normalize scale
    ↓
normalize translation
    ↓
compare to templates
```

Rotation normalization must be disabled by default.

This is because directional orientation is semantically meaningful for mouse gestures.

For example:

```text
→
```

and:

```text
↑
```

must normally be recognized as different gestures.

---

# 9.3 Fixed Sample Count

Normalized strokes should be represented using a fixed point count.

Recommended initial value:

```text
64 points
```

The exact value should remain configurable internally.

---

# 9.4 Gesture Template

A gesture definition must contain:

```text
unique ID
name
one or more stroke templates
enabled state
```

Example:

```json
{
  "id": "close-tab",
  "name": "Close Tab",
  "enabled": true,
  "templates": []
}
```

---

# 9.5 Multiple Training Samples

A gesture may contain multiple training samples.

Example:

```text
Close Tab

├── Template 1
├── Template 2
├── Template 3
└── Template 4
```

Recognition should compare the input against all templates belonging to the gesture.

The best matching template determines the gesture score.

---

# 9.6 Recognition Score

Recognition must produce:

```text
gesture ID
gesture name
confidence or similarity score
```

Example:

```text
Gesture: Close Tab
Score: 0.91
```

---

# 9.7 Recognition Threshold

The program must have a configurable minimum recognition threshold.

If the best gesture does not exceed this threshold:

```text
NoMatch
```

must be returned.

No configured action may execute when recognition fails.

---

# 9.8 Recognition Determinism

Given:

- identical templates
- identical input points
- identical configuration

the recognition result must be deterministic.

---

# 10. Application Context

## 10.1 Target Application

The application active when the gesture begins must be recorded.

At minimum the following information should be available:

```text
window handle
process ID
process executable name
window title
window class
```

Example:

```text
process: chrome.exe
title: GitHub - Google Chrome
class: Chrome_WidgetWin_1
```

---

# 10.2 Application Profiles

Users must be able to define application profiles.

Example:

```text
Profile: Google Chrome

Process:
chrome.exe
```

A profile may contain one or more matching criteria.

---

# 10.3 Supported Match Criteria

The MVP must support at minimum:

### Process name

```text
chrome.exe
```

### Window title

```text
GitHub
```

### Window class

```text
Chrome_WidgetWin_1
```

---

# 10.4 Match Modes

Application matching should support:

```text
Exact
Contains
Regex
```

Regex support may be omitted from the first implementation iteration if necessary, but the profile model must accommodate it.

---

# 10.5 Profile Priority

Action resolution must follow this precedence:

```text
1. Matching application profile
2. Global gesture mapping
3. No action
```

Example:

```text
Gesture: Left

Chrome profile:
    Ctrl+Shift+Tab

Global:
    Alt+Left
```

When Chrome is active:

```text
Ctrl+Shift+Tab
```

must execute.

When another application without an override is active:

```text
Alt+Left
```

must execute.

---

# 11. Action System

## 11.1 Action Abstraction

Actions must use a common internal abstraction.

Conceptually:

```cpp
class IAction {
public:
    virtual ActionResult execute(
        const GestureContext&
    ) = 0;
};
```

This must allow additional action types to be introduced later without changing the gesture recognizer.

---

# 11.2 Keyboard Shortcut Action

The MVP must support keyboard shortcut actions.

Examples:

```text
Ctrl+W

Ctrl+T

Alt+Left

Win+D

Ctrl+Shift+Tab
```

Keyboard actions must use supported Windows input-injection APIs such as `SendInput`.

---

# 11.3 Key Down / Key Up Correctness

Injected shortcuts must correctly generate:

```text
modifier down
key down
key up
modifier up
```

The implementation must not leave modifier keys logically pressed after execution.

---

# 11.4 Existing Modifier State

The action engine must account for modifiers physically held by the user.

Executing an action must not leave keyboard state inconsistent.

---

# 11.5 Action Failure

If an action cannot be executed:

- the engine must return an error result;
- the application must remain operational;
- subsequent gestures must continue working.

---

# 12. Gesture Overlay

## 12.1 Overlay Behavior

When a gesture enters `Capturing` state, the program must display the stroke on screen.

Example:

```text
           ●
         ╱
       ╱
     ╱
   ╱
```

The line should follow the mouse trajectory.

---

# 12.2 Overlay Window

The overlay must:

- be transparent;
- not activate;
- not steal keyboard focus;
- not interfere with underlying mouse targeting;
- support the Windows virtual desktop coordinate space;
- cover multiple monitors.

---

# 12.3 Overlay Configuration

At minimum the following must be configurable:

```text
enabled
line width
opacity
```

Custom color support is desirable but not mandatory for the earliest MVP iteration.

---

# 12.4 Overlay Lifecycle

The overlay must disappear when the gesture:

- completes;
- is cancelled;
- fails recognition.

It must not leave visual artifacts.

---

# 13. System Tray

The application must expose a system tray icon.

Required commands:

```text
Enable Gestures
Disable Gestures
Settings
Exit
```

The tray icon should visually indicate whether gestures are currently enabled or disabled.

---

# 14. Enable / Disable

The user must be able to disable gesture processing without terminating the application.

When disabled:

- normal mouse behavior must remain unaffected;
- no gesture capture may occur;
- no gesture overlay may appear;
- no gesture actions may execute.

The global hook may remain installed if this simplifies implementation.

---

# 15. Configuration

## 15.1 Persistence Format

The MVP should use JSON configuration files.

Recommended structure:

```text
%LOCALAPPDATA%\MouseGesture\
│
├── config.json
├── gestures.json
└── profiles.json
```

The product name in the path may change later.

---

# 15.2 Configuration File

`config.json` should contain global options.

Example:

```json
{
  "gesture_button": "right",
  "movement_threshold": 8,
  "recognition_threshold": 0.80,
  "overlay": {
    "enabled": true,
    "line_width": 4,
    "opacity": 0.85
  }
}
```

---

# 15.3 Gesture File

`gestures.json` should contain gesture templates.

Conceptual example:

```json
{
  "gestures": [
    {
      "id": "left",
      "name": "Left",
      "enabled": true,
      "templates": []
    }
  ]
}
```

---

# 15.4 Profile File

`profiles.json` should contain profile/action mappings.

Example:

```json
{
  "profiles": [
    {
      "id": "chrome",
      "name": "Google Chrome",
      "enabled": true,
      "criteria": [
        {
          "property": "process",
          "mode": "exact",
          "value": "chrome.exe"
        }
      ],
      "actions": {
        "left": {
          "type": "keyboard",
          "shortcut": "CTRL+SHIFT+TAB"
        }
      }
    }
  ],
  "global_actions": {
    "left": {
      "type": "keyboard",
      "shortcut": "ALT+LEFT"
    }
  }
}
```

The exact schema may change during implementation.

---

# 16. Settings Interface

The MVP must provide a way to configure the application.

A polished WinUI settings application is not required for the first implementation.

At minimum the user must be able to:

- view gestures;
- add a gesture;
- rename a gesture;
- delete a gesture;
- train a gesture;
- assign a keyboard shortcut;
- create an application profile;
- select a target process;
- configure global actions;
- change the activation mouse button;
- change recognition sensitivity.

The initial interface may use:

- a simple native Win32 window;
- WinUI 3;
- or another local desktop interface.

Manual JSON editing must not be the only supported configuration method for the completed MVP.

---

# 17. Gesture Training Interface

The application must provide a gesture-training mode.

Workflow:

```text
Create Gesture
      ↓
Enter Name
      ↓
Begin Training
      ↓
Draw Gesture
      ↓
Save Template
      ↓
Repeat Optional Additional Samples
      ↓
Finish
```

A gesture must require at least one valid template before it can be used.

The training interface should display the captured stroke.

---

# 18. Cancellation

The user must be able to cancel an active gesture.

At minimum, cancellation must occur when:

- the configured cancellation key is pressed; or
- the gesture engine detects an invalid input sequence.

Recommended default cancellation key:

```text
Escape
```

On cancellation:

- no action executes;
- the overlay disappears;
- captured points are discarded;
- the state returns to `Idle`.

---

# 19. Multi-Monitor Support

Gesture capture must operate across the Windows virtual screen.

Coordinates may therefore include negative values.

Example:

```text
Monitor 1:
x = -1920 to -1

Monitor 2:
x = 0 to 1919
```

The engine must not assume `(0, 0)` is the upper-left corner of the entire desktop.

Gesture recognition must remain independent of absolute screen coordinates.

---

# 20. DPI Support

The application must behave correctly under common DPI configurations.

At minimum:

- 100%
- 125%
- 150%
- 200%

Gesture matching must not materially change because the same gesture was drawn on a monitor using a different scaling setting.

Stroke normalization should remove most DPI dependence.

---

# 21. Performance Requirements

## 21.1 Hook Latency

The low-level hook callback must return quickly enough that normal mouse interaction does not visibly lag.

The implementation should aim for hook processing substantially below:

```text
1 ms per event
```

under normal conditions.

---

## 21.2 Gesture Recognition

Recognition of a typical gesture should complete in:

```text
< 10 ms
```

on a normal modern desktop CPU for the expected MVP gesture library.

---

## 21.3 Memory

The background engine should target approximately:

```text
< 50 MB idle memory
```

A lower footprint is preferred.

---

## 21.4 CPU

When the mouse is idle, CPU consumption should be effectively negligible.

Expected idle target:

```text
approximately 0%
```

excluding normal scheduling noise.

---

# 22. Reliability Requirements

The application must survive:

- repeated gesture execution;
- failed gesture recognition;
- malformed configuration entries where recoverable;
- target applications closing during gesture execution;
- multi-monitor movement;
- rapid mouse movement;
- rapid repeated gestures;
- suspend/resume where reasonably possible.

One failed action must not terminate the engine.

---

# 23. Logging

The MVP should implement structured logging.

Useful events include:

```text
application start
hook installation
hook failure
gesture start
gesture cancellation
gesture completion
recognition result
recognition score
profile match
action execution
action failure
configuration load
configuration error
application shutdown
```

Mouse movement events should not normally be logged individually.

log should be stored under the user's application data directory.

---

# 24. Security Constraints

The application must not:

- transmit gesture data over the network;
- transmit application titles;
- collect analytics;
- upload configuration;
- record mouse activity while a gesture is not being performed;
- act as a general-purpose keylogger.

Keyboard state should only be processed insofar as necessary to:

- detect gesture modifiers;
- detect cancellation;
- safely execute configured actions.

---

# 25. Elevated Applications

The MVP does not guarantee automation of processes running at a higher Windows integrity level than the gesture engine.

For example:

```text
GestureEngine.exe
normal privileges

        ↓

Administrator Command Prompt
```

input injection may fail because of Windows UIPI restrictions.

The MVP may document this limitation.

The application should not require administrator privileges for normal operation.

---

# 26. Internal Module Structure

Recommended source organization:

```text
src/
│
├── input/
│   ├── mouse_hook.cpp
│   ├── mouse_hook.h
│   ├── keyboard_hook.cpp
│   ├── keyboard_hook.h
│   ├── input_event.h
│   ├── input_queue.h
│   └── gesture_state_machine.*
│
├── gestures/
│   ├── point.h
│   ├── stroke.*
│   ├── normalizer.*
│   ├── recognizer.*
│   ├── gesture_template.*
│   └── gesture_store.*
│
├── context/
│   ├── window_context.*
│   ├── application_profile.*
│   └── profile_matcher.*
│
├── actions/
│   ├── action.h
│   ├── keyboard_action.*
│   └── action_executor.*
│
├── overlay/
│   └── gesture_overlay.*
│
├── config/
│   ├── configuration.*
│   ├── gesture_repository.*
│   └── profile_repository.*
│
├── tray/
│   └── tray_icon.*
│
├── ui/
│   ├── settings_window.*
│   └── gesture_trainer.*
│
└── main.cpp
```

Exact names are not mandatory.

Subsystem separation is mandatory.

---

# 27. Core Data Models

## 27.1 Point

```cpp
struct Point {
    float x;
    float y;
};
```

---

## 27.2 Stroke

Conceptually:

```cpp
struct Stroke {
    std::vector<Point> points;
};
```

---

## 27.3 Gesture Template

```text
GestureTemplate
├── ID
├── gesture ID
└── normalized stroke
```

---

## 27.4 Gesture Definition

```text
GestureDefinition
├── ID
├── name
├── enabled
└── templates[]
```

---

## 27.5 Application Context

```text
ApplicationContext
├── HWND
├── process ID
├── executable
├── window title
└── window class
```

---

## 27.6 Gesture Context

```text
GestureContext
├── recognized gesture
├── confidence
├── start position
├── end position
├── application context
├── modifiers
└── timing information
```

---

## 27.7 Application Profile

```text
ApplicationProfile
├── ID
├── name
├── enabled
├── matching criteria
└── gesture/action mappings
```

---

# 28. Testing Requirements

The core engine must be designed for automated testing.

Windows-specific hooks should be isolated from platform-independent gesture logic.

---

## 28.1 Recognizer Unit Tests

Tests must cover:

- identical stroke recognition;
- translated stroke recognition;
- scaled stroke recognition;
- moderately noisy stroke recognition;
- incorrect stroke rejection;
- threshold behavior;
- multiple gesture templates;
- empty stroke;
- one-point stroke;
- duplicate points;
- extreme coordinate values.

---

## 28.2 State Machine Tests

Tests must cover:

```text
click without movement
movement past threshold
gesture completion
gesture cancellation
unexpected button release
rapid repeated gestures
disable during capture
```

---

## 28.3 Profile Matching Tests

Tests must cover:

- process exact match;
- process mismatch;
- title exact match;
- title contains match;
- class match;
- multiple matching criteria;
- global fallback;
- disabled profile.

---

## 28.4 Action Tests

Tests must verify:

- shortcut parsing;
- correct modifier ordering;
- key release;
- unsupported shortcut handling;
- action execution failure.

Where possible, input injection should be abstracted so tests do not generate real keyboard input.

---

# 29. MVP Acceptance Scenarios

## Scenario 1: Normal right click

Given gestures are enabled  
And the activation button is the right mouse button  
When the user presses and releases the right mouse button without exceeding the movement threshold  
Then the foreground application receives an ordinary right click  
And no gesture is recognized  
And no gesture action executes.

---

## Scenario 2: Begin gesture

Given gestures are enabled  
When the user presses the activation button  
And moves the pointer farther than the gesture threshold  
Then the engine enters gesture capture mode  
And normal activation-button behavior is suppressed  
And the gesture overlay appears.

---

## Scenario 3: Recognize gesture

Given the user has trained a gesture named `Left`  
When the user performs a sufficiently similar stroke  
Then the recognizer returns `Left`  
And the similarity score exceeds the configured recognition threshold.

---

## Scenario 4: Reject unknown gesture

Given multiple gestures exist  
When the user draws a stroke that does not sufficiently match any gesture  
Then no gesture action executes  
And the engine returns to the idle state.

---

## Scenario 5: Global keyboard action

Given the `Left` gesture is globally mapped to:

```text
ALT+LEFT
```

When the user performs `Left` in an application without an application-specific override  
Then the action engine sends `ALT+LEFT`.

---

## Scenario 6: Application override

Given the Chrome profile matches:

```text
chrome.exe
```

And the Chrome `Left` gesture is assigned:

```text
CTRL+SHIFT+TAB
```

And the global `Left` gesture is assigned:

```text
ALT+LEFT
```

When the user performs `Left` while Chrome is foreground  
Then `CTRL+SHIFT+TAB` executes  
And `ALT+LEFT` does not execute.

---

## Scenario 7: Gesture cancellation

Given a gesture is being captured  
When the user presses Escape  
Then the gesture is cancelled  
And no action executes  
And the overlay disappears  
And the engine returns to idle.

---

## Scenario 8: Disabled engine

Given the user disables gestures from the tray icon  
When the user presses and moves the configured gesture button  
Then the mouse behaves normally  
And no gesture is captured  
And no overlay appears  
And no action executes.

---

## Scenario 9: Persistence

Given the user defines a gesture and assigns an action  
When the application exits and is started again  
Then the gesture exists  
And its templates exist  
And its action mapping remains configured.

---

## Scenario 10: Multi-monitor gesture

Given the user has two monitors  
When a gesture begins on one monitor and continues onto another  
Then the complete stroke is captured  
And gesture recognition operates normally.

---

# 30. Definition of MVP Complete

The MVP is complete when all of the following are true:

- The application launches successfully on Windows 11 x64.
- A global mouse hook operates reliably.
- Right-click behavior remains normal when no gesture is drawn.
- Moving beyond a threshold starts gesture capture.
- A visible gesture trail follows the pointer.
- Single-stroke gestures can be trained.
- Multiple templates can belong to a gesture.
- Gestures can be recognized geometrically.
- Recognition provides a score.
- Unknown gestures are rejected.
- Application process context is identified.
- Per-application mappings work.
- Global mappings work as fallback.
- Keyboard-shortcut actions execute.
- Gesture mappings persist between restarts.
- Gestures can be enabled and disabled.
- A tray icon exposes basic controls.
- Gesture recognition is unit tested.
- The state machine is unit tested.
- Profile matching is unit tested.
- Failure of a single gesture or action does not terminate the application.

---

# 31. Post-MVP Roadmap

After the MVP is stable, development can proceed toward StrokesPlus-level capabilities.

## Phase 2 — Action Expansion

Add:

- launch executable
- launch URL
- mouse click
- mouse movement
- window close
- minimize
- maximize
- restore
- move window
- resize window
- virtual desktop actions
- media controls
- volume controls

---

## Phase 3 — Lua Runtime

Embed Lua.

Expose APIs such as:

```lua
keyboard.hotkey("CTRL", "W")

mouse.click("left")

window.close()

process.launch("wt.exe")
```

Provide gesture context:

```lua
gesture.name
gesture.score

gesture.start.x
gesture.start.y

gesture.finish.x
gesture.finish.y

window.title
window.class
window.process
```

---

## Phase 4 — Advanced Gesture Input

Add:

- wheel gestures
- rocker gestures
- modifier gestures
- multi-button gestures
- multi-stroke gestures
- gesture timeout configuration
- alternate activation buttons
- separate gesture sets

---

## Phase 5 — Advanced Context Matching

Add matching against:

- executable path
- parent window
- child control
- process metadata
- wildcard patterns
- regular expressions
- window hierarchy
- Windows UI Automation elements

---

## Phase 6 — Modern Settings Application

Create a dedicated WinUI 3 configuration application containing:

- gesture library
- live gesture preview
- gesture trainer
- profile editor
- action editor
- Lua editor
- diagnostics
- recognition testing
- configuration backup/import/export

---

# 32. Guiding Architectural Principle

The original StrokesPlus source should be treated as a behavioral and historical reference rather than as a codebase to port.

The new implementation should preserve:

```text
StrokesPlus behavior
StrokesPlus flexibility
StrokesPlus low latency
StrokesPlus application-specific gestures
StrokesPlus programmable architecture
```

while replacing its tightly coupled implementation with independent components:

```text
Input
  ↓
Gesture Session
  ↓
Recognition
  ↓
Context Matching
  ↓
Action Resolution
  ↓
Action Execution
```

Every major component should be independently testable.

The MVP succeeds if this pipeline is reliable, fast, extensible, and sufficiently decoupled that advanced StrokesPlus features can subsequently be added without restructuring the core application.
