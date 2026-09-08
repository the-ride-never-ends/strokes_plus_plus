# Strokes++ MVP TODO

Derived from [MVP.md](MVP.md) and [mvp.feature](mvp.feature). Completed items reflect the current repository, not merely planned behavior.

## 1. Project foundation

- [x] Configure a C++20 CMake project.
- [x] Separate portable core code from Windows-specific integrations.
- [x] Build with Visual Studio 2026 and Windows SDK for x64.
- [x] Add a CTest-driven unit-test executable.
- [x] Add the `GestureEngine.exe` Windows application target.
- [x] Keep settings in-process for the MVP.
- [x] Establish message/hook-thread and worker-engine-thread ownership.
- [x] Keep tray/settings on the message thread and recognition, overlay updates, and actions on the worker thread.
- [x] Add a bounded, non-blocking SPSC event queue between low-level hooks and the engine.

## 2. Gesture input and state

- [x] Define the explicit `Idle`, `ButtonPending`, `Capturing`, `Recognizing`, `Executing`, and `Cancelled` states.
- [x] Distinguish clicks from gestures using a configurable movement threshold.
- [x] Collect points only while capturing.
- [x] Filter points using a configurable minimum distance.
- [x] Bound captured points using a configurable maximum.
- [x] Handle gesture completion, cancellation, no-match, and action-completion transitions.
- [x] Cancel an active interaction when gestures are disabled.
- [x] Define the complete `GestureSession` model:
  - [x] Activation button.
  - [x] Start and current positions.
  - [x] Start and last-movement timestamps.
  - [x] Captured modifier state.
  - [x] Original target window and process context.
- [x] Model right, middle, XButton1, and XButton2 activation buttons.
- [x] Default activation to the right mouse button in the engine model.
- [x] Add Escape-key cancellation.
- [x] Handle unexpected input sequences safely.
- [x] Ensure rapid repeated gestures create independent sessions.

## 3. Global Windows input

- [x] Implement an RAII `WH_MOUSE_LL` hook using `SetWindowsHookEx`.
- [x] Keep the hook callback free of disk I/O, recognition, regex, UI work, and blocking logging.
- [x] Translate Win32 movement and supported-button messages into engine input events.
- [x] Route and suppress configured activation-button interactions synchronously.
- [x] Preserve normal activation-button behavior for clicks below the threshold.
- [x] Recreate a suppressed ordinary click safely with `SendInput`.
- [x] Pass unrelated mouse input through normally.
- [x] Ignore injected input so it is not processed recursively.
- [x] Install a minimal keyboard hook that observes only Escape for active-gesture cancellation.
- [x] Provide reliable mouse-hook installation/uninstallation primitives.
- [x] Wire mouse-hook installation/uninstallation into application startup and shutdown.

## 4. Gesture recognition

- [x] Define `Point`, `Stroke`, `GestureTemplate`, and `GestureDefinition` models.
- [x] Remove duplicate and invalid points.
- [x] Resample strokes to a fixed 64-point representation.
- [x] Normalize scale and translation.
- [x] Preserve directional orientation by leaving rotation normalization disabled.
- [x] Match arbitrary single-stroke shapes geometrically.
- [x] Support multiple templates per gesture.
- [x] Exclude disabled gestures from recognition.
- [x] Return gesture ID, name, and similarity score.
- [x] Apply a configurable recognition threshold and return no match below it.
- [x] Make matching deterministic for identical inputs and configuration.
- [x] Add validated repository operations to create, rename, enable, disable, and delete gestures.
- [x] Add repository operations to add and delete training templates.
- [x] Add an automated average-recognition performance gate against the `< 10 ms` target.

## 5. Application context and profiles

- [x] Define application context data for HWND, process ID, executable, title, and class.
- [x] Define enabled application profiles with multiple required criteria.
- [x] Match process name, window title, and window class.
- [x] Support case-insensitive exact, contains, and regex modes.
- [x] Treat malformed regular expressions as a safe non-match.
- [x] Exclude disabled and criterion-free profiles.
- [x] Implement a replaceable foreground-application context provider.
- [x] Capture the foreground HWND with `GetForegroundWindow`.
- [x] Resolve process ID and executable name from the captured HWND.
- [x] Resolve window title and class from the captured HWND.
- [x] Invoke context capture when gesture activation begins.
- [x] Preserve the original application context if focus changes mid-gesture.
- [x] Define explicit precedence when more than one application profile matches (first configured match wins).
- [x] Add profile repository operations for create, rename, enable, disable, delete, criteria, and actions.

## 6. Actions

- [x] Define the initial action data model and keyboard-shortcut action type.
- [x] Resolve application-profile actions before global actions.
- [x] Fall back globally when a matching profile has no mapping.
- [x] Resolve no action when neither profile nor global mapping exists.
- [x] Parse modifiers, letters, digits, navigation keys, and F1-F24 shortcuts.
- [x] Reject malformed, duplicate, modifier-only, and unsupported shortcuts.
- [x] Generate modifier-down, key-down, key-up, and reverse modifier-up ordering.
- [x] Avoid releasing modifiers that were already physically held.
- [x] Implement a testable keyboard-input abstraction.
- [x] Implement the Windows `SendInput` keyboard backend.
- [x] Report input-injection failure without throwing or terminating the engine.
- [x] Connect recognized gestures through profile resolution to action execution.
- [x] Return structured recognition/action outcomes from the engine pipeline.
- [x] Connect structured engine outcomes to logging.
- [x] Confirm behavior when the foreground target exits before execution.
- [x] Document UIPI limitations for elevated target applications.

## 7. Gesture overlay

- [x] Create a transparent, non-activating, click-through Win32 overlay window.
- [x] Cover the full Windows virtual-screen rectangle, including negative coordinates.
- [x] Show the overlay only after entering `Capturing`.
- [x] Render the filtered stroke as points arrive.
- [x] Keep the overlay from stealing keyboard focus.
- [x] Model configurable enabled state, line width, opacity, and color.
- [x] Load overlay configuration from persistent settings.
- [x] Hide and clear the overlay after completion, cancellation, or recognition failure.
- [x] Verify that rapid gestures leave no retained overlay state or lifecycle artifacts.

## 8. Tray and lifecycle

- [x] Start as a background Windows desktop application.
- [x] Create a system tray icon with Explorer-restart recovery.
- [x] Add **Enable Gestures**, **Disable Gestures**, **Settings**, and **Exit** commands.
- [x] Reflect enabled/disabled state in the tray icon and menu.
- [x] Disable input routing and cancel an active gesture without terminating the process.
- [x] Keep mouse routing unaffected while disabled.
- [x] Perform orderly hook, tray, and worker-thread shutdown.
- [x] Include overlay and logging in orderly shutdown.
- [x] Handle suspend/resume where reasonably possible.

## 9. Configuration and persistence

- [x] Implement a dependency-free JSON parser and deterministic serializer.
- [x] Use `%LOCALAPPDATA%\StrokesPlusPlus` as the product directory.
- [x] Define versioned JSON schemas for:
  - [x] `config.json` global options.
  - [x] `gestures.json` definitions and templates.
  - [x] `profiles.json` match criteria and action mappings.
- [x] Create defaults when required files do not exist.
- [x] Load saved enabled state and activation button at startup.
- [x] Persist movement, point-distance, point-limit, recognition, and overlay options.
- [x] Persist gesture identities, names, enabled states, and all templates.
- [x] Persist profiles and global/application-specific actions.
- [x] Save changes using temporary files, backups, and interrupted-save recovery.
- [x] Recover from malformed entries where possible and report useful errors.
- [x] Verify definitions, templates, mappings, and settings survive restart.

## 10. Settings and training UI

- [x] Provide a native local settings interface; manual JSON editing is not the only option.
- [x] View, create, rename, enable/disable, and delete gestures.
- [x] Draw and preview a training stroke.
- [x] Require at least one valid template before enabling a gesture.
- [x] Save multiple training samples for one gesture.
- [x] Assign and validate keyboard shortcuts.
- [x] Create, rename, update, and delete application profiles.
- [x] Enter a target process for a profile.
- [x] Configure process, title, and class matching criteria/modes.
- [x] Configure per-gesture application overrides and global actions.
- [x] Configure activation button and recognition sensitivity.
- [x] Configure movement, point filtering/limit, engine enabled state, and overlay behavior.
- [x] Display recoverable validation and persistence errors.

## 11. Multi-monitor and DPI

- [x] Use Windows virtual-screen coordinates throughout capture and overlay rendering.
- [x] Accept negative screen coordinates.
- [x] Capture strokes that cross monitor boundaries in the input/overlay pipeline.
- [x] Declare per-monitor-v2 DPI awareness at startup.
- [x] Keep threshold behavior sensible across differently scaled monitors.
- [x] Verify threshold behavior at 100%, 125%, 150%, and 200% scaling.

## 12. Logging, reliability, and security

- [x] Add JSON Lines structured logs under the user's application-data directory.
- [x] Log startup/shutdown, hook status, gesture lifecycle, recognition result/score, actions, and configuration errors.
- [x] Include matched profile identity and resolution source in action logs.
- [x] Do not log every mouse-movement event.
- [x] Keep one recognition/action/configuration failure from stopping subsequent gestures.
- [x] Test rapid movement, maximum-size strokes, repeated gestures, and closed targets.
- [x] Keep idle CPU effectively at 0% and target less than 50 MB idle memory.
- [x] Measure the synchronous hook-routing hot path substantially below 1 ms per event.
- [x] Confirm there is no network transmission, analytics, or cloud behavior.
- [x] Never collect movement outside an active gesture.
- [x] Limit keyboard observation to modifiers and cancellation.
- [x] Run without administrator privileges for normal use.

## 13. Automated tests

- [x] Identical, translated, scaled, noisy, wrong, empty, one-point, duplicate, and non-finite stroke tests.
- [x] Threshold, multiple-template, disabled-gesture, and orientation tests.
- [x] Click, capture, point filtering/limit, completion, cancellation, disable-during-capture, and unexpected-release state tests.
- [x] Process/title/class, exact/contains/regex, combined-criteria, malformed-regex, and disabled-profile tests.
- [x] Profile override, global fallback, and no-action tests.
- [x] Shortcut parsing, modifier ordering, held-modifier, unsupported-input, and action-failure tests.
- [x] Add extreme finite-coordinate recognizer tests.
- [x] Add rapid repeated-gesture state tests.
- [x] Add end-to-end tests around abstract hook, overlay, context, and action ports.
- [x] Add JSON round-trip, missing-file, malformed-file, and backup-recovery tests.
- [x] Add Win32 integration smoke tests that do not disrupt normal desktop input.

## 14. MVP acceptance checklist

- [x] Scenario 1: A right-button press/release below threshold produces a normal right click.
- [x] Scenario 2: Movement past threshold begins capture, suppresses button behavior, and shows the overlay.
- [x] Scenario 3: A trained gesture is recognized above threshold with a score.
- [x] Scenario 4: An unknown gesture executes nothing and returns the engine to idle.
- [x] Scenario 5: A global keyboard mapping executes in an application without an override.
- [x] Scenario 6: A matching application mapping overrides the global mapping.
- [x] Scenario 7: Escape cancels capture, hides the overlay, executes nothing, and returns to idle.
- [x] Scenario 8: Disabling gestures leaves mouse behavior normal and prevents capture, overlay, and actions.
- [x] Scenario 9: Gestures, templates, and mappings persist across restart.
- [x] Scenario 10: A cross-monitor stroke is fully captured and recognized.

## Deferred until after MVP

- Additional action types such as process/URL launch, mouse actions, window management, media, and volume.
- Lua runtime and gesture/window APIs.
- Wheel, rocker, modifier, multi-button, and multi-stroke gestures.
- Advanced context matching and Windows UI Automation.
- Full WinUI 3 settings experience, diagnostics, and import/export.
