# Phase 2 TODO

Phase 2 turns the Phase 1 gesture-to-shortcut pipeline into a reusable, general-purpose Windows
action engine. Existing keyboard mappings and application-profile precedence must remain compatible.

Items reopened by the Phase 2 code review ([CODE_REVIEW_PHASE_2_V1.md](CODE_REVIEW_PHASE_2_V1.md))
are unchecked below. Two decisions were taken with it: action results are delivered asynchronously,
and virtual-desktop support is detected at runtime rather than assumed.

## 1. Generalized action model

- [x] Define a serializable `ActionDefinition` independent of concrete C++ action classes.
- [x] Add supported action types: keyboard, process, URL, mouse, window, media, volume, and virtual
      desktop.
- [x] Add action-specific operation enums and parameter structures.
- [x] Add an action schema version at the configuration or individual-action level.
- [x] Define a common executable action interface.
- [x] Define a structured `ActionResult` containing success, an error category/code, and a readable
      message.
- [x] Ensure malformed or unknown action types and operations are rejected without destabilizing the
      application.
- [x] Design the public action/service APIs so Phase 3 Lua bindings can call them without duplicating
      Windows behavior.

## 2. Action context and symbolic values

- [x] Define `ActionContext` with the gesture session, application context, modifier state, gesture
      start/end positions, target window, foreground window, target process, and current cursor
      position.
- [x] Preserve the original gesture target window from gesture start through action execution.
- [x] Resolve mouse position symbols: `current_cursor`, `gesture_start`, `gesture_end`, and `absolute`.
- [x] Resolve window target symbols: `gesture_window`, `foreground_window`, and
      `window_at_gesture_start`.
- [x] Make symbolic-value resolution extensible for future monitor, window geometry, and clipboard
      values.
- [x] Fail safely when a contextual target is missing, stale, or no longer valid; do not substitute a
      different target automatically.

## 3. Resolution, factory, and execution

- [x] Change global gesture mappings from keyboard shortcuts to generic action definitions.
- [x] Change profile gesture mappings from keyboard shortcuts to generic action definitions.
- [x] Preserve resolution precedence: first matching profile action, then global action, then no
      action.
- [x] Add an action factory that validates definitions and creates executable actions.
- [ ] Introduce an action executor and queue for sequential execution of short actions.
- [ ] Deliver action results asynchronously so the gesture worker never waits for an action to
      finish.
- [x] Keep action execution out of low-level mouse and keyboard hook callbacks.
- [x] Ensure action failures always return the gesture engine to Idle.
- [x] Ensure later gestures can execute after any action failure.
- [x] Ensure shutdown and reconfiguration handle queued or executing actions safely.

## 4. Reusable service boundaries

- [x] Define mockable platform-neutral interfaces for keyboard/input, process, shell, mouse, window,
      media, audio, and virtual-desktop services.
- [x] Implement Windows adapters separately from portable action validation and dispatch.
- [x] Keep direct Win32 calls out of action resolution and gesture recognition.
- [x] Return structured failures from every service operation.

## 5. Keyboard actions

- [x] Migrate the existing keyboard shortcut implementation behind the generalized action interface.
- [x] Preserve single-chord and comma-separated shortcut sequence behavior.
- [x] Preserve sided modifier handling and physically held modifier restoration.
- [x] Attempt to release every synthetically pressed key or modifier after partial failure.
- [x] Report injection and parsing failures through `ActionResult`.

## 6. Process actions

- [x] Implement executable launch actions.
- [x] Support optional command-line arguments.
- [x] Support an optional working directory.
- [x] Validate that an executable path is present before persistence.
- [x] Validate runtime path availability immediately before execution.
- [x] Report missing executables, blocked launches, and Windows launch errors without crashing.

## 7. URL and URI actions

- [x] Implement opening URLs and URIs through the registered Windows handler.
- [x] Support HTTP, HTTPS, `mailto`, `ms-settings`, and other registered URI schemes.
- [x] Reject empty and malformed URI definitions.
- [x] Report missing handlers and shell failures through `ActionResult`.

## 8. Mouse actions

- [x] Support left, right, middle, XButton1, and XButton2.
- [x] Implement click and double-click operations.
- [x] Implement explicit button-down and button-up operations.
- [x] Implement immediate pointer movement.
- [x] Support current cursor, gesture start, gesture end, and absolute position targets.
- [x] Support negative virtual-desktop coordinates and cross-monitor movement.
- [x] Normalize absolute coordinates correctly for Windows mouse injection.
- [x] Reject absolute positions without both X and Y coordinates.
- [x] Attempt to release a synthetically pressed button after partial execution failure.

## 9. Window actions and information

- [x] Implement normal close requests without terminating the owning process.
- [x] Implement minimize, maximize, and restore.
- [x] Implement foreground activation and accurately report Windows activation restrictions.
- [x] Implement move while preserving existing size.
- [x] Implement resize while preserving existing origin.
- [x] Implement combined move and resize.
- [x] Select window position and size fields by operation so a move cannot resize and a resize
      cannot move.
- [x] Support negative coordinates and multi-monitor virtual-desktop geometry.
- [x] Reject move, resize, and move-resize definitions with missing required values.
- [x] Validate the target HWND immediately before executing an operation.
- [x] Expose window left, top, right, bottom, width, and height.
- [x] Expose the monitor identifier, monitor bounds, and work area for a target window.

## 10. Media actions

- [x] Implement Play/Pause.
- [x] Implement Next Track.
- [x] Implement Previous Track.
- [x] Implement Stop.
- [x] Report Windows media-command failures through `ActionResult`.

## 11. Volume actions

- [x] Implement system output-volume increase and decrease.
- [x] Implement mute toggle.
- [x] Support a validated configurable increase/decrease amount where practical.
- [x] Clamp volume results to supported minimum and maximum values.
- [x] Handle unavailable or unsupported audio state safely.

## 12. Virtual desktop actions

- [x] Isolate all virtual-desktop behavior behind a dedicated service.
- [x] Implement Next Desktop and Previous Desktop using a reliable Windows mechanism.
- [x] Handle the absence of an adjacent desktop safely.
- [ ] Detect virtual-desktop capability at runtime instead of assuming the Windows shortcut
      worked.
- [ ] Investigate reliable Create Desktop and Close Desktop support on supported Windows versions.
- [ ] Implement Create Desktop and Close Desktop only where sufficiently reliable.
- [ ] Report unavailable or unsupported operations explicitly.
- [ ] Hide unsupported operations from configuration UI choices.

## 13. Persistence and compatibility

- [x] Extend gesture and profile codecs to serialize every generic action type and its parameters.
- [x] Decode each action independently so one malformed mapping does not discard valid siblings.
- [x] Preserve unknown or invalid action diagnostics without treating those actions as executable.
- [x] Load existing Phase 1 shortcut strings as keyboard action definitions.
- [x] Either migrate legacy mappings on save or continue writing/reading an explicitly supported
      compatible representation.
- [x] Preserve existing global mappings, profile mappings, and first-matching-profile precedence.
- [x] Add round-trip tests for every action type and optional parameter combination.
- [x] Add corrupt-record and mixed-validity recovery tests.

## 14. Settings UI

- [x] Ensure every Settings and action-editor dropdown expands to show its available choices at
      every supported DPI scale.
- [x] Keep the Match Value process suggestions responsive by listing only executables that own
      visible top-level application windows and batching combo-box population.
- [x] Add an action-type selector for Keyboard Shortcut, Launch Program, Open URL, Mouse, Window,
      Media, Volume, and Virtual Desktop.
- [x] Show only the controls relevant to the selected action type.
- [x] Preserve the existing keyboard shortcut assignment and removal workflow.
- [x] Add process path, arguments, working-directory, executable browser, and directory browser
      controls.
- [x] Add a URL/URI field.
- [x] Add mouse operation, button, position target, and conditional absolute-coordinate controls.
- [x] Add window operation, target, and conditional position/dimension controls.
- [x] Add media-operation controls.
- [x] Add volume-operation and conditional amount controls.
- [x] Add only supported virtual-desktop operations.
- [x] Validate the complete action before assigning or saving it.
- [x] Present actionable validation errors rather than silently dropping invalid values.
- [x] Preserve keyboard navigation, DPI behavior, focus order, and dialog Save/Cancel semantics.

## 15. Diagnostics and robustness

- [x] Log gesture name/id, matched profile, action type, operation, target, result, and failure reason.
- [x] Distinguish definition validation failures, runtime validation failures, unsupported operations,
      and platform execution failures.
- [x] Contain exceptions at the action-executor boundary.
- [x] Keep hooks operational and gesture input enabled after action failure.
- [x] Guarantee state-machine cleanup after success, failure, cancellation, shutdown, and
      reconfiguration.
- [x] Ensure no failure path intentionally leaves synthetic keys, modifiers, or mouse buttons held.

## 16. Unit tests

- [x] Test the factory with every valid action type, unknown types, missing required parameters, and
      invalid operations.
- [x] Test keyboard action success, sequence execution, injection failure, and input cleanup.
- [x] Test process launch with arguments and working directory, plus missing/invalid executable and
      launch failure.
- [x] Test HTTP, HTTPS, registered URI, empty/malformed URI, and handler failure.
- [x] Test every mouse button with click, double-click, down, and up operations.
- [x] Test mouse movement and clicks at gesture-start, gesture-end, current, absolute, negative, and
      cross-monitor coordinates.
- [x] Test window close, minimize, maximize, restore, activate, move, resize, and move-resize.
- [x] Test invalid/destroyed HWNDs, negative coordinates, and multi-monitor window operations.
- [x] Test window and monitor geometry queries.
- [x] Test every media operation.
- [x] Test volume increase, decrease, mute toggle, limits, configured amount, and unavailable audio.
- [x] Test virtual-desktop next, previous, optional create/close, unavailable operations, and Windows
      implementation failures.
- [x] Test action behavior without requiring gesture recognition.
- [x] Test engine recovery and a subsequent successful gesture after each action category fails.

## 17. Integration and acceptance tests

- [x] Launch an existing executable from a recognized gesture.
- [x] Open a website through the registered Windows handler.
- [x] Click at the recorded gesture-start position.
- [x] Maximize and minimize the original gesture target window.
- [x] Move a target window to configured coordinates.
- [x] Resize a target window to configured dimensions.
- [x] Send Play/Pause and adjust system volume.
- [x] Switch to the next virtual desktop where supported.
- [x] Verify an application-specific process action overrides a global URL action.
- [x] Verify a missing executable reports failure, returns the engine to Idle, and does not prevent
      the next gesture.
- [x] Verify all action mappings survive save, restart, and reload.
- [x] Verify Phase 1 keyboard configurations still load and execute.
- [ ] Verify action execution never blocks the low-level input-hook path.

## 18. Documentation and release

- [x] Update the README to describe generalized actions, configuration, limitations, and Windows
      security boundaries.
- [x] Document serialized action schemas and compatibility behavior.
- [x] Document Windows/version limitations for activation, URI handlers, audio, and virtual desktops.
- [x] Add Phase 2 changes, tests, and known issues to the changelog.
- [x] Update the project version when Phase 2 is ready for release.
