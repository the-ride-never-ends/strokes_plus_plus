# Changelog

All notable changes to Strokes++ will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Added `CODE_REVIEW_PHASE_2_V1.md`, a review of `src/` and `tests/` against `PHASE_2_SPEC.md`,
  `PHASE_2_SPEC.feature`, and `TODO.md`.
- Added mouse-over tooltips to the simple and advanced Settings labels, using the same descriptions
  as the unchanged Help dialog.

### Changed

- Renamed the application executable from `GestureEngine.exe` to `StrokesPlusPlus.exe`.
- Reorganized general settings, gesture editing, application profiles, and help into separate tabs.
- Added a scaled picture of the selected gesture's stored stroke to the Gestures tab, including a
  preview-only arrowhead that indicates direction without changing the gesture data.
- Made Advanced settings a collapsed-by-default caret section on the Settings tab.
- Renamed the Settings navigation tabs to Options, Global Actions, and Applications, and aligned
  the Help tab terminology with those names.
- Added a read-only Gestures inventory tab that displays every pattern, grouped into blue Activated
  and gray Not Activated galleries independently of action assignments.
- Styled the Help tab with bold section headings, divider lines, aligned label-and-description
  columns, and alternating row shading.
- Recorded two Phase 2 follow-up decisions from that review: action results are delivered
  asynchronously so the gesture worker never waits for an action to finish, and virtual-desktop
  support is detected at runtime instead of assumed.
- Unchecked the `TODO.md` items that were marked complete without a corresponding implementation:
  the action executor queue, the Create Desktop and Close Desktop reliability investigation and its
  conditional implementation, explicit unsupported-operation reporting, hiding unsupported
  operations from the Settings choices, gesture identity in the action log record, and the
  acceptance test for non-blocking hook-path execution. The newly decided work was added as
  unchecked items.
- Corrected the README's window action schema, which described `x`, `y`, `width`, and `height` as
  required for every move, resize, and move-resize action.
- Documented in the README that virtual desktop actions currently report success without confirming
  the operation, and that actions execute on their own worker while the engine waits for each one.
- Made window move and resize operations ignore fields belonging to other operations, defaulted
  omitted mouse positions to `current_cursor`, resolved that cursor position when the action runs,
  and changed fresh minimize/maximize mappings to native window actions.

### Fixed

- Included gesture identity in action execution records and added regression coverage for the
  documented mouse-action defaults and move/resize preservation guarantees.
- Prevented DPI rescaling from collapsing Settings and action-editor dropdown lists to the height
  of their closed selection field.
- Kept the Match Value process dropdown responsive by excluding background-only processes and
  batching allocation and redraw while its visible-application suggestions are populated.

## [0.8.0] - 2026-09-12

### Added

- Added a versioned generalized action model for keyboard, process, URL/URI, mouse, window, media,
  volume, and virtual-desktop actions.
- Added a validating action factory, sequential executor, structured results, contextual position
  and window resolution, and reusable platform-service interfaces suitable for future Lua bindings.
- Added Windows implementations for executable launches, shell URI handling, generalized mouse
  injection, window manipulation and geometry, media commands, Core Audio volume control, and
  virtual-desktop commands.
- Added generic-action persistence with record-level recovery and mapping-specific diagnostics.
- Added a generalized Settings action editor with type-specific fields, executable and directory
  browsers, validation, editing, and mapping removal for global and profile actions.

### Changed

- Migrated global and application-profile mappings from the keyboard-only representation to generic
  action definitions while retaining Phase 1 keyboard configuration compatibility.
- Expanded action logging with type, operation, target, result, error category, code, and message.

### Tests

- Added action-definition, validation, factory, queue ordering, exception recovery, symbolic target,
  persistence, Windows input construction, cleanup, geometry, and per-category engine recovery
  coverage.
- Added recognized-gesture acceptance coverage for every action category, Phase 1 compatibility
  execution, nonblocking hook-path delivery, volume limits, and shutdown cleanup.

## [0.7.0]

### Added

- Added a universal `Minimize` gesture, drawn diagonally from the top right to the bottom left,
  with an action that can be changed or cleared in Settings like any other global gesture mapping.
- Added comma-separated shortcut sequences, such as `ALT+SPACE,N`, for actions that require more
  than one key chord.
- Added a default `Maximize` gesture, drawn diagonally from the bottom left to the upper right,
  with an editable `WIN+UP` global shortcut.

### Changed

- Changed the default Minimize action from `WIN+DOWN`, which only restores a maximized window on
  its first invocation, to `ALT+SPACE,N`, which fully minimizes normal and maximized windows.
- Added a short pause between shortcut-sequence steps so the target application can process each
  chord before the next one is sent.

### Known Issues

- The `ALT+SPACE,N` MVP implementation briefly displays the active window's system menu and can
  exhibit minor timing slippage between opening the menu and sending `N`. A menu-free native
  minimize action may replace it after the MVP.

### Tests

- Added coverage for configurable minimize and maximize shortcuts, comma-separated shortcut
  parsing, and both direction-sensitive diagonal gesture pipelines.

## [0.6.0]

### Changed

- Made a right-click on the notification-area icon toggle gesture processing directly and display
  a grayscale icon while gestures are disabled; the command menu now opens with a left-click.
- Write the current run's `log.txt` to the application's launch directory, falling back to the
  executable directory when the launch directory is not writable, and continue truncating it at
  startup.
- Added a Help button to Settings with plain-language explanations of every global option, gesture
  control, shortcut assignment, application profile, and matching criterion.
- Reorganized Settings into Simple user-facing controls and Advanced recognition and stroke-sampling
  controls that expose implementation details.

### Fixed

- Prevented notification-area callbacks from opening Settings reentrantly while its modal message
  loop is active, and dispatch menu commands only after the popup menu closes, so Settings opens
  reliably on the first request.
- Place Settings explicitly within the active monitor's work area and briefly promote it above
  other windows while it is shown.
- Start each application process with gesture handling enabled, regardless of the previous run's
  temporary tray-disabled state.
- Exclude notification-area and taskbar clicks from gesture suppression so the tray menu remains
  accessible while gesture handling is enabled.
- Preserve the DPI-scaled Settings window dimensions when bringing it forward, preventing the
  right-side editors and bottom buttons from being clipped outside the client area.

### Tests

- Added Windows regressions for the shell callback mapping and for creating, displaying, closing,
  and leaving the modal loop of the Settings window, including containment checks for every visible
  child control.
- Added regressions for enabled-by-default startup and complete left-button pass-through while
  right-button gestures are enabled.

## [0.5.0]

### Changed

- Replaced the generic Windows notification-area glyphs with the Strokes++ logo and embedded it as
  the executable's native Windows icon resource.
- Moved the runtime log to `log.txt` beside the application executable and truncate it at startup
  so each run contains only its own structured events.

### Fixed

- Made shutdown discard queued input instead of recognizing and executing stale gestures, removing
  the visible delay when closing after heavy mouse activity.
- Kept the installed input hooks stable while the Settings worker is paused, avoiding the transient
  first-open hook restoration failure.
- Removed the notification-area icon immediately when Exit is selected, before the remaining
  worker and persistence cleanup completes.

### Tests

- Added regressions for discarding queued event-pump items and truncating log between sessions.

## [0.4.0]

### Changed

- Tracked physically held modifier keys in the low-level keyboard hook and answered held-key
  queries from that state instead of `GetAsyncKeyState`, whose table an action's own injection
  modifies; the hook seeds the state on installation and observes only modifier keys and Escape.
- Resolved gesture actions from the first matching application profile alone, falling back to the
  global mapping when that profile has no entry rather than continuing into later profiles.
- Coupled every event-pump wake token to exactly one consumed item by replacing the non-consuming
  drain with `take`, so tokens cannot accumulate across worker restarts.
- Split the settings window into a window that owns the modal loop, DPI scaling and global options
  plus separate gesture and profile editors over shared control helpers and identifiers.
- Bounded overlay opacity at the smallest value that survives conversion to an 8-bit alpha channel
  instead of rejecting only exactly zero.
- Consulted the cancellation handler on every physical Escape press so a latch whose key-up was
  never delivered clears itself instead of suppressing Escape for the rest of the session.

### Fixed

- Stopped silently releasing a modifier the user is physically holding: the restore pass now reads
  physical state that injection cannot corrupt, so a held Shift survives an action mapped to an
  unrelated shortcut, and the outcome no longer races the injection.
- Gave the settings window working keyboard navigation by making its controls tab stops and mapping
  the dialog manager's Enter and Escape onto Save and Cancel, which `IsDialogMessageW` alone could
  not reach and had begun swallowing.
- Kept the log size bound after a failed rotation by restarting the byte budget, ending a
  close-remove-rename-open cycle on every subsequent record and unbounded growth of the active log.
- Removed a duplicate modifier restore on the injection-failure path and the unreachable second
  activation-button bounds check in the settings save path.

### Tests

- Added regressions for injected key-ups leaving physical state unchanged, a stale Escape latch
  clearing on a declined press, global fallback past a matching profile that is silent on the
  gesture, drain-consumed wake tokens, and overlay opacity that rounds to a transparent window.
- The held-modifier tests now pass because the production implementation matches the fake's model
  of physical key state rather than because both possible platform behaviours were asserted.

## [0.3.0]

### Changed

- Coupled the input queue and its semaphore in a testable event pump with one wake token per
  accepted event, eliminating the producer-side empty-state race.
- Split recoverable decoder warnings from fatal errors, centralized match-criterion preparation
  and default normalizer construction, and generated numeric validation messages from their
  canonical limits.
- Added sided Shift, Control, and Alt handling and moved physical-modifier restoration into a
  second injection pass so released modifiers are not re-pressed.
- Added keyboard navigation to Settings, dynamic-length UTF-8 field reads, validated combo-box
  selections, unique label identifiers, an owned gesture trainer, and training-sample removal.
- Excluded wall-clock performance tests from the development runner's default test pass and
  tightened the idle CPU gate over a longer sample.

### Fixed

- Retained gestures and profiles when their optional `enabled` or `criteria` fields are malformed,
  disabling affected records instead of silently deleting them on the next save.
- Preserved pending mouse-release suppression across disable and reconfiguration operations, and
  made disabling an active interaction deliver cancellation at the router boundary.
- Kept logging usable after a failed rotation, reported logger startup failure, retained the
  configuration transaction marker after rollback failure, and surfaced hook-restart failure by
  disabling the tray and stored engine state.
- Rejected invisible overlays, unsupported left-button click replay, invalid combo selections, and
  shrinking overlay stroke updates without invoking undefined behavior.
- Made activation-button serialization round-trip every enum value, removed raw enum-order casts
  from Settings, and unregistered the Settings window class when its owning instance registered it.

### Tests

- Added regressions for event-pump wake-up reliability, closed and direction-sensitive circles,
  captured-window context lookup, disable-during-capture, modifier release during injection,
  malformed optional record fields, and every bounded global option.
- Strengthened exact batch-count and repeated profile-match assertions.

## [0.2.0]

### Changed

- Latched gesture capture after the movement threshold, sampled the target window at activation,
  replaced hook-path condition-variable notification with semaphore signaling, and limited Escape
  suppression to gestures that have actually entered capture.
- Made the overlay allocate its virtual-desktop backbuffer only while visible, coalesce refreshes,
  and use generation-tagged lifecycle messages so rapid gestures cannot reuse stale stroke state.
- Centralized numeric configuration limits, cached locale construction, separated configuration
  warnings from fatal errors, and isolated wall-clock performance gates in their own CTest target.
- Moved movement-threshold ownership into the synchronous input router while allowing physical pointer movement to pass through during activation-button holds.
- Deferred gesture-point allocation until capture begins, cached validated profile regular expressions, and moved DPI discovery out of the low-level mouse-hook callback.
- Reworked overlay drawing around a retained backbuffer, incremental dirty-region repainting, and display-geometry refresh on `WM_DISPLAYCHANGE`.
- Expanded the settings UI with profile enablement, editable process selection, full criterion management, action removal, UTF-8-safe profile names, default GUI fonts, and per-monitor DPI rescaling.
- Standardized C++ formatting and shortened the callable names identified by the code review; added API documentation to the central abstractions.
- Made the development runner prefer an explicit or `PATH` CMake installation before its machine-specific fallback, and aligned the README build directory with the runner.

### Fixed

- Contained all gesture-worker exceptions, bounded hand-edited point and overlay settings, clamped
  recoverable cross-field values, and prevented duplicate running instances.
- Routed left-button presses into invalid-sequence cancellation, preserved return-to-origin stroke
  points, rejected the transparent black overlay color, and balanced partial mouse injection.
- Corrected new-gesture shortcut state and made clearing a shortcut consistent between Assign and
  Save; malformed profile action collections now discard only those actions.
- Rotated log during long-running sessions, retained every quarantined configuration revision,
  retried tray-icon registration, unregistered the trainer window class, rounded replay coordinates,
  and documented elevated-process profile-matching limits.
- Unified keyboard balance ownership in the executor, handled the right Windows key independently,
  and restore neutralized modifiers only while they remain physically held.
- Preserved valid gesture and profile files when one configuration component is malformed; invalid files are quarantined independently instead of causing an all-default rewrite.
- Made three-file configuration updates transactional with startup rollback, durable flushes, and non-throwing filesystem error handling; tray-triggered saves now run on a coalescing background worker.
- Accepted the partial configuration examples in MVP.md, validated shortcuts and regular expressions at decode time, and retained valid gestures when individual templates are malformed.
- Restored an ordinary activation-button click at its original press position without freezing cursor movement, and safely cancelled malformed active input sequences.
- Balanced partial keyboard injection failures and temporarily neutralized unrelated held modifiers before restoring the user's physical modifier state.
- Bounded JSON nesting, emitted compact round-trip-safe numbers, rotated oversized log, and delayed gesture-start logging until capture actually begins.
- Made Escape suppression survive hook shutdown until the corresponding key-up, and limited Win32 class cleanup to classes registered by the owning component.
- Reported startup integration failures visibly and isolated the executable idle test from the user's real configuration directory.

### Tests

- Added regressions for closed-loop routing, left-button cancellation, configuration bounds and
  clamping, malformed profile actions, runtime log rotation, right-Windows-key handling, and native
  keyboard/mouse injection failure paths; made temporary store paths concurrency-safe.
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
- Persistence of tray enable/disable changes and matched-profile identity/source in action log.
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
