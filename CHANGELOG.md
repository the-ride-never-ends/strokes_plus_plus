# Changelog

All notable changes to Strokes++ will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.10.0]

### Fixed

- Raise the mouse trace overlay above application windows each time a gesture starts, while keeping
  focus on the active application.
- Relinked and tested the Debug and Release development executables with the trace fix.

### Public release preparation

- Updated the README to describe the current application, installer, configuration, logging, and
  action behavior, with a short first-run guide and release verification steps.
- Added the Lua 5.4.9 copyright and MIT license notice to both release packages.
- Aligned the executable's copyright metadata with the project license.
- Added user-facing release notes and a repeatable publication checklist for the installer and
  portable ZIP.
- Included the logo referenced by the packaged README in both release formats.
- Made the packaging script find NSIS in standard Program Files locations or through
  `STROKES_NSIS`, as well as on `PATH`.
- Consolidated the 0.10.0 release notes into the README while keeping the publication checklist
  in `RELEASING.md`.

### Packaging

- Built the 0.10.0 Windows NSIS64 executable installer and its SHA-256 checksum from the Release
  configuration.
- Made the idle performance gate detach its verified input hooks before measuring resources, so
  unrelated mouse activity on the test desktop cannot make an active host look like a failed idle
  host. A failed sample reports measured CPU time, working set, and query status.
- Passed CMake and CTest arguments explicitly in the development runner so configuring a fresh build
  directory does not fail PowerShell parameter binding on `-S`.
- Made the development runner use an alternate build directory when its target executable is in
  use, allowing build-and-test runs while Strokes++ is running.

### Global Actions labels

- Labeled the built-in screen snip and tab shortcuts by their actions, and displayed the actual
  keys for shortcuts with no known action label.
- Split the combined global-action command into Add Action and Edit Action. Add opens the action
  editor with a picker for unassigned gestures and wheel or rocker triggers; Edit changes only the
  selected existing mapping.
- Excluded gestures already mapped in application profiles from the Add Action picker, in addition
  to those with global mappings.
- Clarified Help for the separate Add, Edit, and Delete Action controls and Add Action's gesture
  picker.

### Phase 3 Lua scripting

- Added the first-class Lua action definition and inline-script persistence for global and
  application-profile gesture mappings.
- Added a managed Lua execution-service boundary that receives the immutable action context,
  returns structured service-unavailable failures until a runtime is installed, and leaves all
  existing built-in action types unchanged.
- Added validation, display summaries, factory delegation, context-forwarding, and configuration
  round-trip coverage for Lua actions.
- Embedded and checksum-pinned Lua 5.4.9 from the official source distribution, with the runtime
  owned by the gesture engine and reused across action invocations.
- Added protected syntax validation and execution, structured syntax and runtime errors with source
  locations, persistent runtime globals, and explicit `true`, `false`, or no-value result handling.
- Restricted the initial standard-library surface to base, math, string, table, and UTF-8 support,
  with direct file-loading functions removed.
- Enabled synchronized MSVC program-database writes for every project target to keep parallel
  compilation reliable as the embedded runtime expands the build.
- Exposed read-only `gesture` and `application` Lua context objects containing recognition identity
  and score, start and finish coordinates, duration, point count, traveled distance, process
  identity, window metadata, and the executable path when Windows makes it available.
- Captured the full executable path alongside the existing executable name without changing the
  meaning of existing `ApplicationContext` aggregate initializers.
- Added engine-level coverage proving recognized Lua actions receive their recognition and captured
  application context, plus runtime coverage for context values and rejected mutation attempts.
- Added the Lua `keyboard` namespace with variadic hotkeys, single-key presses, explicit key-down
  and key-up events, and live physical key-state queries.
- Extended the shared keyboard service with validated individual-key injection and logical modifier
  queries backed by the existing sided physical-key state, keeping Lua automation on the Phase 2
  service boundary.
- Added Lua argument and key-name validation plus coverage for Ctrl+Shift+T, F5, held/released
  Shift detection, balanced Ctrl down/up calls, and rejected invalid inputs.
- Added Lua `process.launch` with optional arguments and working directory, plus `shell.open` for
  registered URLs and URIs.
- Added complete Lua `media` and `desktop` namespaces over the existing Phase 2 services, including
  play/pause, track navigation, stop, desktop navigation, creation, and closing.
- Added strict argument validation and structured propagation of process-launch, shell-open, and
  unsupported virtual-desktop failures into protected Lua runtime errors.
- Added the Lua `mouse` namespace for reading and moving the pointer and for clicking,
  double-clicking, holding, and releasing left, right, middle, X1, and X2 buttons.
- Preserved Windows virtual-desktop coordinates, including negative monitor positions, and added
  defined errors for invalid buttons, non-finite or nonnumeric coordinates, and unavailable cursor
  state.
- Added Lua window control for close, minimize, maximize, restore, activate, move, resize, and
  move-resize operations, defaulting to the captured gesture window with optional live foreground
  targeting.
- Added read-only window bounds and existence queries, strict target and dimension validation, and
  propagation of activation and other platform failures as protected Lua errors.
- Added Lua window title, class, and process-name queries for gesture and foreground targets, backed
  by live Windows application-context capture.
- Added the complete Lua `volume` namespace for relative changes, mute toggling, reading and setting
  the normalized 0-100 output level, and reading mute state.
- Extended the Core Audio service with reusable endpoint acquisition and query/set operations while
  preserving the existing volume-action behavior and structured HRESULT failures.
- Added configurable Lua execution deadlines enforced through instruction hooks, interrupting
  infinite and excessively long scripts with structured runtime failures.
- Added thread-safe Lua cancellation and connected engine shutdown to cancel an active script before
  joining the action executor, preventing runaway automation from blocking application exit.
- Bounded the sequential action queue at 64 pending actions and added an immediate structured
  `action_queue_full` result when rapid triggers exceed that limit.
- Added deterministic concurrency coverage proving Lua actions never execute concurrently through
  the shared executor and queued scripts retain submission order.
- Added engine-level Lua precedence coverage for application-script overrides, application built-in
  overrides, and global Lua fallback through the existing resolver.
- Added end-to-end failure recovery coverage proving the engine returns to idle and continues
  accepting both Lua and built-in gestures after a script error.
- Added Lua `log.debug`, `log.info`, `log.warn`, and `log.error` functions backed by the existing
  structured application logger, including strict message validation and protected reporting of
  unavailable or failed diagnostic writes.
- Added automatic shared Lua initialization from `scripts/init.lua` in the user configuration
  directory, making its functions and values reusable across gesture actions while allowing normal
  startup when the file is absent.
- Contained initialization syntax and runtime errors, reported them through application diagnostics,
  and kept the Lua runtime available for later actions.
- Added a restricted Lua `require` implementation rooted at the user configuration directory's
  `scripts/modules` folder, with cached module exports, nested dotted module names, clear missing
  module errors, and rejection of traversal, slash, and other unapproved path syntax.
- Added atomic Lua runtime reload support that rebuilds the restricted environment, reruns updated
  initialization code, invalidates cached user modules, and clears runtime-only globals.
- Kept the clean replacement runtime operational when reloaded initialization code fails, with
  coverage for updated functions and modules, failure reporting, and process-lifetime-only state.
- Added Lua Script to the shared global/profile action editor with a native multiline Windows edit
  control, including standard clipboard and undo/redo behavior, validation, saving, and restoration
  of existing scripts when mappings are edited.
- Extended Windows smoke coverage to verify all nine action types and the multiline Lua editor while
  preserving the existing dialog navigation and built-in-action controls.
- Added Lua editor Validate and Test controls: validation reports protected syntax errors without
  changing the script, while testing runs in an isolated managed runtime with empty gesture and
  application context and reports either completion or the protected execution failure.
- Added `ui.message` and `ui.osd` Lua feedback functions through a Windows user-feedback service;
  OSD messages dismiss automatically and unsupported timed-message environments report a protected
  Lua error.
- Added an API Help view to the Lua editor and matching README documentation covering every exposed
  namespace, function signature, parameter family, return convention, context object, shared
  initialization path, and restricted module location.

### Phase 3 code review fixes

- Built the embedded Lua library as C++ so that an error raised inside an API binding propagates as
  a C++ exception. The bindings own strings, paths and an open file stream while they raise, and the
  previous longjmp did not destroy them.
- Made shutdown cancellation reachable. Cancellation is now requested by the application host before
  the engine worker is joined, rather than by the engine destructor, which could only run once the
  action had already finished; a cancelled runtime also refuses later scripts until it is reloaded
  instead of clearing the request at the start of the next script.
- Replaced the modal message boxes behind `ui.message` and `ui.osd` with a layered on-screen window
  owned by its own message thread. Both return immediately, so feedback no longer blocks the action
  worker, gesture recognition or application exit, and no longer depends on the undocumented
  `MessageBoxTimeoutW` export.
- Gave the action editor's Validate and Test controls the real automation services, the shared
  initialization script and the module directory, so a tested script runs the automation it will
  perform instead of reporting that every service is unavailable.
- Made a script that no gesture produced, such as one run from Test, see no `gesture` and no
  `application` globals at all, instead of default-constructed coordinates and empty process
  identity.
- Read the script from the editor using the count the copy reports rather than the rich edit
  control's CRLF-based length estimate, which could append NUL characters to a saved multiline
  script and make it fail to load.
- Added Reload Lua Scripts to the notification-area menu, and re-read the initialization script
  whenever the engine is rebuilt, so edits to `init.lua` no longer require restarting the
  application while edits to modules did not.
- Created the `scripts` and `scripts/modules` folders with a commented sample initialization script
  on first run, and documented Lua scripting on the Settings Help tab.
- Made `keyboard.press` send one key down and up through the individual-key service instead of the
  shortcut-sequence parser, and rejected `+` and `,` inside `keyboard.press` and `keyboard.hotkey`
  key names, where they silently produced a chord or a timed sequence.
- Removed `rawset`, `rawget`, `rawequal` and `rawlen` from the script environment, because they
  write straight through the metatables that keep the context objects read-only, and replaced
  `print` with a binding that writes to the application log.
- Replaced the single-message-box API help with a scrollable reference that lists every namespace
  and function with its parameters, return value and description.
- Replaced the action editor's hardcoded combo-box indices with named action types and
  enum-derived comparisons, and pinned the combo and enum ordering with static assertions.
- Reported a failed initialization script through user feedback as well as the application log, and
  gave the engine an explicit, named Lua execution limit instead of relying on a constructor
  default.
- Captured a window's application context once per Lua window query instead of once per property.
- Prevented gestures and window actions from minimizing or otherwise changing the Windows taskbar
  and notification-area flyouts, including the Windows 11 XAML hidden-icons panel, which could
  leave the notification-area caret active while its panel remained inaccessible.
- Corrected the horizontally mirrored built-in `e` gesture and migrated its built-in sample in
  existing configurations without changing user-trained samples.

### Tests

- Rewrote two assertions that passed for the wrong reason: a false `is_down` result was asserted
  through the script's own false return value, and the module-traversal rejection was a Lua
  string-escape syntax error that never reached the module loader.
- Added coverage for engine-level shared initialization and runtime reload, the absence of file,
  package and raw-access functions, `print`, cancellation that persists until reload, the empty
  context, the new `keyboard.press` behaviour, rejected module names, and a configuration round
  trip for a script containing carriage returns and a NUL character.


## [0.9.0]

### Windows installer

- Added CMake install rules and CPack packaging for a versioned 64-bit NSIS installer and portable
  ZIP, including the application icon, bundled Visual C++ runtime, Start Menu/Desktop shortcuts,
  Add/Remove Programs registration, upgrade-time uninstall, and a reproducible `package.ps1` flow.
- Added Windows file and product version metadata sourced from the CMake project version.

### StrokesPlus.net default gesture mapping

- Remapped the built-in gestures to direction- and shape-based StrokesPlus.net defaults for
  clipboard, selection, window management, media control, navigation, and Explorer launch.
- Added M, P, e, C, chevron, rectangle, and diagonal compound gesture patterns, including
  maximize/restore toggle and center-window operations.
- Added right-button mouse-wheel volume controls and left/right rocker navigation triggers.
- Made non-drawn wheel and rocker actions clear the gesture fields and display
  `No Gesture Assigned` instead of leaving the previously selected gesture visible.
- Made mouse-click cleanup tests independent of the interactive desktop and corrected the window
  activation smoke test to recognize Windows' documented foreground-policy denial explicitly.
- Updated Help for the 41-pattern catalog, StrokesPlus.net defaults, Chrome and Excel overrides,
  wheel and rocker triggers, assigned-action details, and the `No Gesture Assigned` state.
- Added Chrome defaults for new tab, reload, and reopen-closed-tab, plus Excel defaults for
  previous/next worksheet. Application overrides continue to take precedence over global actions.

### Global Actions UI refresh

- Reworked Global Actions around an assigned-action list, gesture selector, gesture-management
  commands, live gesture preview, and explicit add/edit/delete action controls modeled after the
  classic StrokesPlus.net layout.
- Kept the current one-global-action-per-gesture backend for this iteration. Named reusable action
  records, mouse-modifier conditions, keyboard-modifier conditions, and Lua scripting remain
  deferred and are intentionally not exposed as nonfunctional controls.
- Replaced generic action labels such as `keyboard.shortcut` with parameter-aware summaries across
  Global Actions and Applications, including actual key sequences, targets, paths, URIs, positions,
  dimensions, and configured amounts.
- Changed the Global Actions list to concise semantic labels such as Minimize, Maximize, Copy, or
  Launch Program while retaining the exact shortcut or parameter summary in Assigned action.
- Added non-destructive default assignments for all 30 activated built-in gestures, covering common
  navigation, editing, window, Explorer, Task Manager, media, mute, and volume actions. Existing
  customized mappings are preserved and only missing defaults are added during migration.

### Added

- Added `CODE_REVIEW_PHASE_2_V1.md`, a review of `src/` and `tests/` against `PHASE_2_SPEC.md`,
  `PHASE_2_SPEC.feature`, and `TODO.md`.
- Added mouse-over tooltips to the simple and advanced Settings labels, using the same descriptions
  as the unchanged Help dialog.
- Added the complete 36-pattern direction-named gesture catalog from StrokesPlus.net, with 30
  patterns activated and 6 inactive independently of action assignment.

### Changed

- Renamed the application executable from `GestureEngine.exe` to `StrokesPlusPlus.exe`.
- Reorganized general settings, gesture editing, application profiles, and help into separate tabs.
- Added a scaled picture of the selected gesture's stored stroke to the Gestures tab, including a
  preview-only arrowhead that indicates direction without changing the gesture data.
- Made Advanced settings a collapsed-by-default caret section on the Settings tab.
- Renamed the Settings navigation tabs to Options, Global Actions, and Applications, and aligned
  the Help tab terminology with those names.
- Expanded Help to explain direction-based gesture names, catalog activation, preview arrows, and
  the separation between gesture patterns and action assignments.
- Replaced ambiguous slash and backslash gesture labels with explicit diagonal directions such as
  `Diagonal Down-Left` and `Diagonal Up-Right`.
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
