# Changelog

All notable changes to Strokes++ will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The project does not yet have a versioned release.

## [Unreleased]

### Changed

- Moved movement-threshold ownership into the synchronous input router while allowing physical pointer movement to pass through during activation-button holds.
- Deferred gesture-point allocation until capture begins, cached validated profile regular expressions, and moved DPI discovery out of the low-level mouse-hook callback.
- Reworked overlay drawing around a retained backbuffer, incremental dirty-region repainting, and display-geometry refresh on `WM_DISPLAYCHANGE`.
- Expanded the settings UI with profile enablement, editable process selection, full criterion management, action removal, UTF-8-safe profile names, default GUI fonts, and per-monitor DPI rescaling.
- Standardized C++ formatting and shortened the callable names identified by the code review; added API documentation to the central abstractions.
- Made the development runner prefer an explicit or `PATH` CMake installation before its machine-specific fallback, and aligned the README build directory with the runner.

### Fixed

- Preserved valid gesture and profile files when one configuration component is malformed; invalid files are quarantined independently instead of causing an all-default rewrite.
- Made three-file configuration updates transactional with startup rollback, durable flushes, and non-throwing filesystem error handling; tray-triggered saves now run on a coalescing background worker.
- Accepted the partial configuration examples in MVP.md, validated shortcuts and regular expressions at decode time, and retained valid gestures when individual templates are malformed.
- Restored an ordinary activation-button click at its original press position without freezing cursor movement, and safely cancelled malformed active input sequences.
- Balanced partial keyboard injection failures and temporarily neutralized unrelated held modifiers before restoring the user's physical modifier state.
- Bounded JSON nesting, emitted compact round-trip-safe numbers, rotated oversized logs, and delayed gesture-start logging until capture actually begins.
- Made Escape suppression survive hook shutdown until the corresponding key-up, and limited Win32 class cleanup to classes registered by the owning component.
- Reported startup integration failures visibly and isolated the executable idle test from the user's real configuration directory.

### Tests

- Added direct decoding tests for all MVP configuration examples, corrupt-component and interrupted-transaction recovery, two-thread SPSC behavior, disabled end-to-end routing, and additional malformed-input paths.
- Added a `GestureEngine.exe --idle-test` CTest that measures the live process against the idle CPU and 50 MB working-set targets.
- Expanded the routing benchmark to cover complete button-down, pointer-move, and button-up interactions with DPI scaling enabled.

## [0.1.0]

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
- Versioned JSON codecs and configuration storage with atomic per-file writes for global options, gesture templates, profiles, and actions.
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
- Foreground-window DPI-scaled activation thresholds, frozen per interaction and tested at 100%, 125%, 150%, and 200% scaling.
- Abstract gesture-feedback port and end-to-end router, engine, overlay, context, recognition, and action integration coverage.
- Rapid repeated-overlay lifecycle and cancellation-cleanup regression coverage.
- Non-disruptive Win32 smoke tests for hook lifecycle, hidden overlay creation, foreground context capture, native click construction, and Escape filtering.
- Per-monitor-v2 DPI awareness and reliability coverage for extreme coordinates, rapid repeated gestures, and recovery after action failure.
- Reserved input-queue capacity for release/cancel control events during high-frequency pointer movement.
- Automated recognition performance gate using a 32-template library and 1,000 recognition iterations.
- Corrected the tray host to use a hidden broadcast-capable window so Explorer restart notifications are received.
- Validated gesture and application-profile repositories supporting identity-safe edits, enablement invariants, training-template management, matching criteria, and action mappings.
- Implementation backlog in `TODO.md`, derived from the MVP specification and Gherkin feature file.
- MVP code review in `CODE_REVIEW.md`, tracing `src/` and `tests/` against the specification.
- Unit coverage for normalization, recognition, state transitions, gesture sessions, profile matching, action resolution, shortcut handling, input queues, and mouse routing.

### Changed

- Enabled standard C++ exception-unwinding semantics for MSVC targets.
- Separated platform-independent behavior from direct Win32 integrations to keep the core independently testable.
- Documented the in-process settings architecture, background host operation, local-only data behavior, and Windows UIPI limitations.

### Fixed

- Prevented disabled gestures and profiles from participating in matching.
- Prevented malformed shortcuts and regular expressions from disrupting the engine.
- Prevented physically held modifiers from being released by injected actions.
- Prevented a failed mouse-event delivery from leaving the synchronous router permanently active.
- Prevented resolved actions from borrowing configuration storage that may no longer exist at execution time.
- Prevented held-Escape key repeats from leaking unmatched Escape events.

### Verification

- Builds successfully with Visual Studio Community 2026, MSVC 19.51, and Windows SDK 10.0.26100.0.
- Both the portable core suite and non-disruptive Win32 smoke suite pass through CTest.
