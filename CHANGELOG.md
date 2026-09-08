# Changelog

All notable changes to Strokes++ will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The project does not yet have a versioned release.

## [Unreleased]

### Added

- C++20 CMake project with separate portable `strokes_core` and Windows-specific `strokes_windows` libraries.
- Visual Studio 2026 x64 build support and CTest-based unit-test executable.
- Core point, stroke, gesture-template, and gesture-definition models.
- `$1`-style single-stroke normalization with duplicate-point removal, fixed 64-point resampling, scale normalization, and translation normalization.
- Orientation-preserving gesture recognition with deterministic scoring, configurable thresholds, multiple templates, and disabled-gesture filtering.
- Explicit gesture state machine covering idle, pending-button, capturing, recognizing, executing, and cancelled states.
- Configurable movement and point-distance thresholds with bounded stroke collection.
- Gesture-session snapshots containing activation button, positions, timestamps, modifier state, captured points, application context, and current state.
- Application-context and application-profile models.
- Case-insensitive exact, contains, and regular-expression profile matching for process names, window titles, and window classes.
- Safe handling of invalid profile regular expressions.
- Application-specific action resolution with global fallback.
- Keyboard-shortcut parsing for modifiers, letters, digits, navigation keys, and function keys.
- Deterministic keyboard press/release sequencing that preserves modifiers already held by the user.
- Testable keyboard-input abstraction and Windows `SendInput` implementation.
- Replaceable application-context provider and Windows foreground-application implementation.
- Win32 foreground HWND, process ID, executable name, window title, and window-class capture.
- RAII Windows low-level mouse hook using `WH_MOUSE_LL`.
- Translation of global movement, right-button, middle-button, XButton1, and XButton2 messages into engine input events.
- Filtering of injected mouse input to prevent recursive processing.
- Bounded lock-free single-producer/single-consumer input queue.
- Synchronous mouse-input router for configured-button selection, click/gesture threshold handling, input suppression, disabled-state pass-through, and unrelated-input pass-through.
- End-to-end gesture engine pipeline connecting sessions, recognition, profile/global action resolution, and keyboard execution.
- Abstract mouse-click replay boundary and Windows `SendInput` implementation for preserving ordinary activation-button clicks.
- Integration tests covering ordinary clicks, global actions, profile overrides using captured context, and unknown gestures.
- Runnable `GestureEngine.exe` host with a Windows message loop, global hook, bounded input queue, and stoppable worker thread.
- System tray lifecycle controls for enabling, disabling, opening settings, and exiting, including visual status and Explorer-restart recovery.
- Transparent, click-through, non-activating gesture overlay spanning the Windows virtual desktop with thread-safe live stroke rendering and lifecycle cleanup.
- Dependency-free JSON parser and deterministic serializer with full value types, escape/Unicode handling, numeric validation, duplicate-key rejection, and parse-error offsets.
- Versioned JSON codecs and atomic three-file configuration storage for global options, gesture templates, profiles, and actions.
- `%LOCALAPPDATA%\StrokesPlusPlus` startup integration with first-run defaults, backup recovery, and runtime application of saved thresholds, activation button, enabled state, recognition sensitivity, and overlay options.
- Thread-safe JSON Lines logging under Local AppData for application, configuration, hook, gesture, recognition, and action lifecycle events.
- Minimal RAII low-level keyboard hook for Escape cancellation, including injected-key filtering and balanced suppression of Escape down/up events.
- Native settings window for engine, recognition, activation-button, overlay, and default global-action configuration with validation, atomic persistence, and live worker reload.
- Persistence of tray enable/disable changes and matched-profile identity/source in action logs.
- Native gesture library controls for creation, rename, deletion, live single-stroke training/preview, multiple samples, and automatic post-training enablement.
- Native application-profile creation/deletion with process matching plus selected-gesture global and profile-specific shortcut assignment.
- Explicit gesture enable/disable controls and profile rename/process-update controls.
- Profile matching controls for process, window-title, and window-class criteria using exact, contains, or regular-expression modes.
- Deterministic first-configured-match precedence when multiple application profiles match.
- Suspend/resume lifecycle handling that cancels capture, removes global hooks before sleep, and safely reinstalls them after wake.
- Hook suspension while the modal settings editor owns configuration, preventing stale input from accumulating during edits.
- Record-level configuration recovery that skips malformed gestures, templates, profiles, criteria, and actions while retaining valid siblings and logging useful warnings.
- End-to-end engine coverage for cancellation, closed original targets, and strokes crossing negative-to-positive virtual-screen coordinates.
- Automated latency gate for the synchronous low-level mouse-hook routing hot path.
- Foreground-window DPI-scaled activation thresholds, frozen per interaction and tested at 100%, 125%, 150%, and 200% scaling.
- Abstract gesture-feedback port and end-to-end router, engine, overlay, context, recognition, and action integration coverage.
- Rapid repeated-overlay lifecycle and cancellation-cleanup regression coverage.
- Non-disruptive Win32 smoke tests for hook lifecycle, hidden overlay creation, foreground context capture, native click construction, Escape filtering, and idle CPU/memory budgets.
- Per-monitor-v2 DPI awareness and reliability coverage for extreme coordinates, rapid repeated gestures, and recovery after action failure.
- Reserved input-queue capacity for release/cancel control events during high-frequency pointer movement.
- Automated recognition performance gate using a 32-template library and 1,000 recognition iterations.
- Corrected the tray host to use a hidden broadcast-capable window so Explorer restart notifications are received.
- Validated gesture and application-profile repositories supporting identity-safe edits, enablement invariants, training-template management, matching criteria, and action mappings.
- Implementation backlog in `TODO.md`, derived from the MVP specification and Gherkin feature file.
- Unit coverage for normalization, recognition, state transitions, gesture sessions, profile matching, action resolution, shortcut handling, input queues, and mouse routing.

### Changed

- Enabled standard C++ exception-unwinding semantics for MSVC targets.
- Separated platform-independent behavior from direct Win32 integrations to keep the core independently testable.
- Documented the in-process settings architecture, background host operation, local-only data behavior, and Windows UIPI limitations.

### Fixed

- Prevented disabled gestures and profiles from participating in matching.
- Prevented malformed shortcuts and regular expressions from disrupting the engine.
- Prevented injected keyboard modifiers from being left logically pressed after an action.
- Prevented physically held modifiers from being released by injected actions.
- Prevented a failed mouse-event delivery from leaving the synchronous router permanently active.
- Prevented resolved actions from borrowing configuration storage that may no longer exist at execution time.
- Prevented held-Escape key repeats and suspend/resume boundaries from leaking unmatched Escape events.

### Verification

- Builds successfully with Visual Studio Community 2026, MSVC 19.51, and Windows SDK 10.0.26100.0.
- Both the portable core suite and non-disruptive Win32 smoke suite pass through CTest.
