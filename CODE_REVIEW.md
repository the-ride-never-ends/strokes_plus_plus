# Strokes++ MVP Code Review

Review of `src/` (66 files) and `tests/` (15 files) against `MVP.md` and `mvp.feature`, 8 September 2026.

Findings are from reading, not from running the binary. **C2** in particular is a behavioural claim worth confirming on hardware before refactoring around it.

**Tally:** 2 critical · 5 high · 15 moderate · 10 low · 8 test issues · 6 convention issues

---

## 1. What holds up

Worth stating before the defect list, because the architecture is the expensive part and it is right.

- **Subsystem separation is real, not nominal.** `strokes_core` compiles and tests without a single Windows header, and every platform dependency sits behind an interface (`IApplicationContextProvider`, `IKeyboardInput`, `IMouseClick`, `IGestureFeedback`). This is what MVP.md §32 asks for and most projects fail it.
- **The application-context snapshot** is captured at `button_down` and carried through resolution, with a test that mutates the foreground app mid-gesture to prove it. §7.7 and §10.1 satisfied.
- **Profile-matching coverage is complete against §28.3** — process, title, class, exact, contains, regex, malformed regex, combined criteria, disabled profile, global fallback, and first-match precedence.
- **The recognizer** preserves orientation by never rotating, uses uniform (aspect-preserving) scaling rather than $1's bounding-box stretch, and rejects non-finite and degenerate strokes cleanly.
- **The SPSC queue, click-emulation path, and cancelled-release suppression** (`cancelled_release_pending_`) show that orphan button-up events were thought about, not stumbled into.

---

## 2. Critical

Both are reachable in normal use on a normal desktop.

### C1 — One bad byte in `config.json` silently destroys the user's gesture library
*MVP.md §22, §15*

`decode_global_options` is all-or-nothing: any invalid or missing field fails the whole load. `load_or_create` then returns an error for the entire bundle, and `EngineHost` falls back to `ConfigurationStore::defaults()` — which contains exactly one gesture and one global action. `gestures.json` and `profiles.json` were never read and may be perfectly valid, but they are now gone from memory.

The next tray click seals it. "Disable Gestures" sets `gestures_enabled=false` and calls `save_configuration()`, which writes *all three* files from the defaults bundle. The user's entire trained gesture library and every application profile are overwritten by a single default "Right" gesture. Opening Settings and pressing Save does the same thing.

`configuration_codec.cpp:63` · `configuration_store.cpp:56,70-72` · `main.cpp:62-66,172,180,226`

**Fix:** Load the three files independently so one failure cannot poison the others; on a decode failure, rename the bad file aside rather than shadowing it in memory, and refuse to save over a file that failed to load.

### C2 — The mouse cursor freezes for the entire time the activation button is held
*MVP.md §8.4, §12.1*

`MouseInputRouter::route` returns `suppress_input = true` for *every* `pointer_moved` event while an interaction is active — both below the threshold and during capture. `on_mouse_event` hands that straight back to the hook as a nonzero return, which stops the system processing the move, and a blocked `WM_MOUSEMOVE` in a `WH_MOUSE_LL` hook does not just hide the message: the cursor does not move.

The visible result is that the overlay trail follows the mouse while the pointer arrow sits pinned at the press point — for gestures, and for right-clicks with a few pixels of hand jitter. Only the button down/up needs suppressing; movement never did.

`mouse_input_router.cpp:52-61` · `windows_mouse_hook.cpp:52` · `main.cpp:150-153`

**Fix:** Return `suppress_input = false` for `pointer_moved` while still delivering the event to the engine. Note the coupling: `WindowsMouseClick::click` injects at the current cursor position and is only correct today *because* the cursor was frozen — it will need `MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE` to the recorded start position once movement is allowed through.

---

## 3. High

### H1 — Throwing `std::filesystem` calls inside `noexcept` functions terminate the process
*MVP.md §22*

`atomic_write` uses the throwing overload of `std::filesystem::remove` on both failure paths, and `load_or_create` uses the throwing `exists` three times — everywhere else in the same file correctly passes an `error_code`. Those calls are reached from `save_configuration()` and `on_tray_command()`, both declared `noexcept`. A `filesystem_error` (locked file, antivirus scan, permission change) becomes `std::terminate`. "One failed action must not terminate the engine" is the exact wording of §22.

`configuration_store.cpp:22,24,53-55` · `main.cpp:166,208`

**Fix:** Use the `error_code` overloads consistently, and wrap the `noexcept` tray/save handlers in `try/catch(...)` as `open_settings` already does.

### H2 — The spec's own example configuration files are all rejected
*MVP.md §15.2–15.4*

Every field in `config.json` is mandatory with no defaulting. The example in §15.2 has no `version`, no `gestures_enabled`, no `minimum_point_distance`, no `maximum_points` and no `overlay.color` — five independent rejections. The §15.3 gesture example and §15.4 profile example both lack `version` and fail outright.

Since §16 requires that manual JSON editing not be the *only* configuration method, hand-editing has to remain viable, and today omitting any single key bricks startup into C1.

`configuration_codec.cpp:63-66,80,101`

**Fix:** Default every absent field to the `GlobalOptions` member initializer and reject only fields that are present *and* invalid. Treat a missing `version` as the current version.

### H3 — Win32 calls run inside the low-level mouse hook
*MVP.md §7, §21.1*

The `threshold_scale_provider` lambda calls `GetForegroundWindow()` then `GetDpiForWindow()`, and `MouseInputRouter::route` invokes it on every activation-button down — inside the hook callback. `GetDpiForWindow` on a foreign process window is a cross-process query. §7 lists exactly what the hook must not do and the spirit is unambiguous; exceeding `LowLevelHooksTimeout` gets the hook silently removed by Windows. TODO.md §3 claims this constraint is met.

`main.cpp:74-75,234-235` · `mouse_input_router.cpp:37-39`

**Fix:** Cache the DPI scale on the worker thread and on `WM_DPICHANGED` / `WM_DISPLAYCHANGE`, and have the hook read a plain `std::atomic<double>`.

### H4 — The overlay repaints the entire virtual desktop once per captured point
*MVP.md §12.1, §21.4*

`update()` copies the whole stroke under a mutex and posts a refresh; `handle_message` answers with `InvalidateRect(window_, nullptr, TRUE)` across a window sized to the full virtual screen. `paint()` then `FillRect`s that entire area and redraws the complete polyline, unbuffered. On a dual-4K desktop at 125 Hz mouse reporting that is a full-screen GDI fill plus an O(n) polyline per point, with an O(n²) copy cost across the gesture and visible flicker.

`windows_gesture_overlay.cpp:53-60,85,104-129`

**Fix:** Invalidate only the bounding rectangle of the newly added segment, append points rather than copying the stroke, and draw into a memory DC. §5.2 explicitly permits Direct2D here if you would rather solve it once.

### H5 — A partial `SendInput` can leave modifiers logically held down
*MVP.md §11.3*

`WindowsKeyboardInput::send` returns `SendInput(...) == input_count` and does nothing else. If Windows accepts the modifier-down and key-down events but blocks the tail — the documented UIPI case the README itself describes — the modifier key-ups are never delivered and Ctrl or Alt stays logically pressed system-wide. §11.3 states the implementation must not leave modifier keys logically pressed after execution; there is no cleanup path.

`windows_keyboard_input.cpp:31` · `keyboard_action.cpp:20-23`

**Fix:** On a short return, issue a second `SendInput` containing only the key-up events for everything this action injected, then report failure.

---

## 4. Moderate

| ID | Finding | Location |
|----|---------|----------|
| **M1** | **An enabled gesture whose templates all fail validation is deleted, not disabled.** One non-finite coordinate drops the template, leaving `templates.empty()`, and the enabled gesture is then skipped entirely — it vanishes from the file on the next save. mvp.feature says such a gesture is "not eligible for recognition", not that it ceases to exist. `configuration_tests.cpp:38-41` asserts the current behaviour as correct. | `configuration_codec.cpp:89` |
| **M2** | **The repository accepts templates the recognizer will later reject, and one bad template kills the whole gesture.** `add_template` only checks `points.size() >= 2`, so two identical points pass and persist; `Recognizer::add_gesture` then returns false for the *entire* gesture including its good templates. A gesture trained with one degenerate stroke silently stops working after restart. §9.5 implies per-template evaluation. | `gesture_repository.cpp:20`, `recognizer.cpp:40-46` |
| **M3** | **`std::regex` is constructed on every match attempt and never validated at configuration time.** A fresh `std::regex` per criterion per profile per recognized gesture is a millisecond-scale cost against a 10 ms budget (§21.2). Separately, `add_criterion` stores an uncompilable pattern without complaint and the matcher swallows the `regex_error` — the profile just never matches, with no diagnostic. | `profile_matcher.cpp:61-68`, `profile_repository.cpp:17-18` |
| **M4** | **Extraneous physically-held modifiers are not neutralised.** The executor skips injecting a modifier that is already down, which handles the matching case. It does not handle the mismatched one: with Shift physically held, a `CTRL+W` action delivers `CTRL+SHIFT+W` to the target. §11.4 asks the action engine to account for modifiers the user is holding. | `keyboard_action.cpp:12-17` |
| **M5** | **The JSON parser has no recursion depth limit.** `parse_array` and `parse_object` recurse through `parse_value` unbounded, so a corrupt or hostile config of nested brackets is a stack overflow rather than a parse error. §22 requires surviving malformed configuration where recoverable; this one is recoverable with a counter. | `json.cpp:135,151` |
| **M6** | **The movement threshold is implemented twice, with DPI scaling in only one.** The router compares against `movement_threshold × dpi_scale`; the state machine compares the same distance against the raw `movement_threshold`. Two sources of truth for one rule. | `mouse_input_router.cpp:56`, `gesture_state_machine.cpp:60` |
| **M7** | **Configuration is written synchronously on the thread that owns the mouse hook.** Tray Enable/Disable calls `save_configuration()` inside the window procedure, serialising and writing three files while the message loop — and therefore the low-level hook — is blocked. Desktop-wide mouse lag on a menu click. | `main.cpp:172,180,208-213` |
| **M8** | **The overlay never responds to display changes.** Its geometry is read once from `SM_*VIRTUALSCREEN` at creation and there is no `WM_DISPLAYCHANGE` handling. Plug in a monitor, change resolution, or dock, and the overlay is the wrong size at the wrong origin — with `paint()` reading a fresh origin the window no longer matches. Directly undercuts §19. | `windows_gesture_overlay.cpp:26-32,118-119` |
| **M9** | **Every right-click allocates ~128 KB.** `button_down` reserves `maximum_points` (4096 × 16 B = 64 KB) before it is known whether this is a gesture at all, and `executable_name` allocates a 32768-wchar path buffer (64 KB) on the same event. Both per-click, on the latency path, for interactions that are usually plain right-clicks. | `gesture_state_machine.cpp:48`, `windows_application_context.cpp:87` |
| **M10** | **Two enable flags exist; only one is used.** `GestureStateMachine::enabled_` and `set_enabled` are never called by `GestureEngine` or `main.cpp` — production disables gestures purely at the router. The state-machine flag is exercised only by tests, so `state_machine_tests.cpp` is testing a path that never runs. Either wire it or delete it. | `gesture_state_machine.cpp:107-113` |
| **M11** | **Case-insensitive matching is byte-wise and breaks on non-ASCII.** `lowercase()` applies `std::tolower` to each byte of a UTF-8 string. Window titles are the criterion most likely to contain non-ASCII. | `profile_matcher.cpp:12-19` |
| **M12** | **Profile names are mangled in the settings list.** `load_values` builds the listbox string with `std::wstring(p.name.begin(), p.name.end())` — a byte-by-byte widen of UTF-8 — while every other call site correctly uses the `wide()` helper. | `windows_settings_window.cpp:140` |
| **M13** | **Settings cannot reach parts of its own data model.** No way to enable or disable an existing profile (mvp.feature has explicit scenarios for both), no way to remove an assigned global or override action, and only `criteria.front()` is editable although §10.2 and the matcher both support multiple required criteria. §16 also asks the user to *select* a target process; the UI offers free text only. | `windows_settings_window.cpp:114-127,240-254` |
| **M14** | **`save()` is not transactional across the three files.** The `&&` chain means a failure writing `gestures.json` leaves an already-committed `config.json` from the same edit. Each file is individually atomic; the bundle is not. | `configuration_store.cpp:70-72` |
| **M15** | **No "invalid input sequence" cancellation.** §18 names it as a required cancellation trigger alongside Escape. Left, and any non-activation button, pass straight through to the foreground application mid-gesture — so a left-click lands in the target while a gesture is being drawn over it, and the gesture continues. | `mouse_input_router.cpp:71`, `windows_mouse_hook.cpp:93-94` |

---

## 5. Low

| ID | Finding | Location |
|----|---------|----------|
| **L1** | Numbers serialise with `chars_format::general` at precision 17, so `0.85` is written to `config.json` as `0.84999999999999998`. Round-trips exactly, reads terribly for a file §16 expects people to open. Drop the precision argument for shortest round-trip formatting. | `json.cpp:225` |
| **L2** | The tray menu omits the `PostMessage(window_, WM_NULL, 0, 0)` that must follow `TrackPopupMenu` — the standard notification-area quirk where the menu fails to dismiss when the user clicks away. | `windows_tray_icon.cpp:126-128` |
| **L3** | Neither the settings window nor the trainer sends `WM_SETFONT`, so every control renders in the legacy system bitmap font rather than Segoe UI, and all coordinates are hardcoded pixels that do not scale under the per-monitor-v2 awareness declared in `wWinMain`. | `windows_settings_window.cpp:93-130` |
| **L4** | Silent failures at startup. If `windows_configuration_directory()` fails, `startup_error_` is set but `run()` returns before anything displays it; hook installation failure logs and exits with no user-visible message. The process just does not appear. | `main.cpp:48-53,113-130` |
| **L5** | The log file grows without bound — no rotation, no size cap — and every record is flushed individually. With a `gesture_start` record on every right-click that accumulates faster than it looks. | `structured_logger.cpp:26-27` |
| **L6** | Every activation-button press writes the foreground window title and cursor coordinates to disk, including plain right-clicks that never become gestures. §24 is about network transmission, so this is not a violation, but it sits against the spirit of "never record mouse activity while a gesture is not being performed". Log at capture start instead. | `main.cpp:284-289` |
| **L7** | `atomic_write` flushes the `ofstream` but never the OS cache, so the rename can commit ahead of the data. A `FlushFileBuffers` before the rename makes the crash-safety story real. | `configuration_store.cpp:18-19` |
| **L8** | The trainer hardcodes a 2-pixel minimum point distance rather than using the configured `minimum_point_distance`, so templates are sampled differently from live strokes. | `windows_gesture_trainer.cpp:32` |
| **L9** | Both the overlay and the tray call `UnregisterClassW` unconditionally in `destroy()`, including on paths where the class was pre-existing rather than registered by that instance. | `windows_gesture_overlay.cpp:43`, `windows_tray_icon.cpp:47` |
| **L10** | If the keyboard hook stops between a suppressed Escape key-down and its key-up, `suppress_escape_up_` is reset and the target application receives an unpaired key-up. Reachable via tray Suspend or opening Settings with Escape held. | `windows_keyboard_hook.cpp:17-23` |

---

## 6. Tests

Coverage against §28 is genuinely good — the recognizer, state machine, profile matching and action suites map onto the required cases almost item for item. The problems are concentrated in what the tests *claim* to prove.

### T1 — The idle CPU and memory assertions measure the wrong process *(high)*

`windows_smoke_tests.cpp` sleeps 500 ms in a bare console executable — hooks already uninstalled, overlay already destroyed — then asserts that *this* process used <25 ms CPU and <50 MB working set. That is trivially true of any console program and says nothing about `GestureEngine.exe`. §21.3 and §21.4 are effectively unverified while TODO.md §12 marks them done.

`windows_smoke_tests.cpp:53-65`

### T2 — The hook-path benchmark skips the expensive part *(high)*

`input_routing_performance` constructs the router with an empty `threshold_scale_provider` and times only `pointer_moved` events, with the single `button_down` outside the timed loop. The production configuration — the DPI callback of H3, invoked on exactly that excluded event — is never measured.

`performance_tests.cpp:25-34`

### T3 — `check()` does not abort, so a regression crashes instead of reporting

Several assertions dereference the result immediately — `recognizer.recognize(...)->gesture_id`. When recognition regresses, the suite dies on a null optional dereference rather than printing which check failed. `check` also carries no file or line information.

`gesture_tests.cpp:82,92` · `test_support.h:10-15`

### T4 — The lock-free queue is only tested single-threaded

`InputQueue` is the one piece of genuinely concurrent code in the project and every acquire/release claim in it is untested. A two-thread push/pop test asserting FIFO order and no loss would cost twenty lines.

`input_queue_tests.cpp`

### T5 — No test decodes the spec's example JSON

Every configuration test round-trips the encoder's own output, which is why H2 went unnoticed — the decoder is only ever fed input the encoder produced. Pasting the three documents from MVP.md §15 into a test would have caught it immediately.

`configuration_tests.cpp:9-14`

### T6 — Missing scenarios

No end-to-end "gestures disabled" test through router + engine (mvp.feature has one, and TODO.md checks off Scenario 8); no "resume gesture processing after re-enabling"; no test that a stroke rejected below threshold still hides the overlay; no partial-`SendInput` or extraneous-held-modifier case for H5/M4; no degenerate-template case for M2; no deep-nesting JSON case for M5.

### T7 — "Overlay disabled but recognition still works" is structurally untestable

The `enabled` check lives inside `WindowsGestureOverlay` rather than at the `IGestureFeedback` boundary, so no portable test can reach it. Moving the flag up to the engine would make the mvp.feature scenario testable.

`windows_gesture_overlay.cpp:48,54`

### T8 — Test hygiene

Wall-clock assertions in a unit suite will flake on a loaded CI machine; the store and logger tests both use a fixed path under `temp_directory_path()` and will collide between parallel runs; `foreground_application().has_value()` fails on a headless agent; and the assertion message on `WIN+D` reads "letter shortcut parses".

`performance_tests.cpp:20,34` · `keyboard_action_tests.cpp:39`

---

## 7. Spec traceability

Requirements where the implementation diverges. Everything not listed I read as met.

| Requirement | Source | Status | Finding |
|---|---|---|---|
| Hook callback performs no expensive work | §7 | **Not met** | H3 |
| Normal right click behaves as if the app were absent | §8.4 / Scenario 1 | Partial | C2 |
| Documented example configuration files load | §15.2–15.4 | **Not met** | H2 |
| Survives malformed configuration where recoverable | §22 | Partial | C1, M5 |
| One failed action does not terminate the engine | §22 / §11.5 | Partial | H1 |
| Modifier keys never left logically pressed | §11.3 | Partial | H5 |
| Accounts for modifiers physically held by the user | §11.4 | Partial | M4 |
| Cancels on an invalid input sequence | §18 | **Not met** | M15 |
| Overlay covers the virtual screen across monitors | §12.2 / §19 | Partial | M8 |
| Recognition completes in <10 ms | §21.2 | Partial | M3 (regex path unmeasured) |
| Idle CPU ≈0%, idle memory <50 MB | §21.3 / §21.4 | **Unverified** | T1 |
| Enable and disable an application profile | mvp.feature | **No UI** | M13 |
| Select a target process for a profile | §16 / mvp.feature | Free text only | M13 |
| Untrained gesture is ineligible, not deleted | mvp.feature | **Not met** | M1 |
| Gesture recognition, state machine, profile matching unit tested | §30 | Met | — |
| Per-application and global mappings, persistence, tray controls | §30 | Met | — |

---

## 8. Conventions

Measured against your stated rules rather than general style opinion.

### K1 — No docstrings anywhere
A grep for `/**` and `///` across `src/` returns **zero matches in 66 files**. Google-style docstrings are required; there are currently four ordinary comments in the whole tree.

### K2 — Roughly eighteen callables exceed two words
`scale_and_translate`, `filter_escape_event`, `decode_global_options`, `decode_gesture_file`, `decode_profile_file`, `load_selected_gesture`, `load_selected_profile`, `assign_global_action`, `assign_profile_action`, `set_activation_button`, `make_input_sequence`, `windows_configuration_directory`, `parse_keyboard_shortcut`, `utf8_from_control`, `set_error_string`, `load_or_create`, `on_tray_command`, `on_mouse_event`.

### K3 — DRY
The queue-sink lambda is written out three times in `main.cpp` (the constructor's initialiser is dead — it is overwritten in the constructor body) and the DPI-scale lambda twice. `GestureRepository` and `ProfileRepository` are near-identical CRUD-over-vector classes. The threshold rule is duplicated per M6.

`main.cpp:39-47,76-82,236-239`

### K4 — Fail-fast
Three inputs are accepted without validation and fail silently much later: regex patterns (M3), shortcut strings in `profiles.json` — which parse only at execution time, so a typo produces a dead gesture with no diagnostic — and gesture templates that cannot normalise (M2). Also unvalidated: `minimum_point_distance` greater than `movement_threshold`, which makes every stroke a single point and recognition impossible.

`gesture_engine.cpp:100-108` · `gesture_state_machine.cpp:14-18`

### K5 — Two formatting dialects
About a dozen files — both repositories, the codec, the store, the settings window, the keyboard hook, and parts of `main.cpp` — are written with whitespace stripped out entirely, while the rest of the tree is conventionally formatted. It is the difference between `gestures/recognizer.cpp` and `gestures/gesture_repository.cpp`, side by side in the same directory. A committed `.clang-format` would settle it.

### K6 — Warning-prone precedence
Both repositories use `if(!p||enabled&&p->criteria.empty())`. The precedence is correct but it is exactly the pattern `/W4` flags, and the build enables `/W4`. Parenthesise it.

`gesture_repository.cpp:16` · `profile_repository.cpp:15`

---

## 9. Suggested order

1. **C1 and H1 together.** Both live in `configuration_store.cpp` and both are data-loss or process-death on a path a user hits by accident. Independent per-file loading plus consistent `error_code` usage closes them in one pass.
2. **H2 next, in the same file.** Defaulting absent fields makes C1's blast radius much smaller on its own and unblocks hand-editing.
3. **C2.** A two-line change in the router plus absolute positioning in `WindowsMouseClick`. Verify on the device first — the cursor-freeze behaviour deserves five minutes with the built binary before you refactor around it.
4. **H3, then H4.** The cached DPI scale is small; the overlay repaint is the larger job and the one most worth doing properly, since it is also the answer to the unmeasured §21.4 idle-CPU claim.
5. **T1, T2 and T5.** These three tests are what let the issues above pass as done. Fix the tests before the remaining moderates, so the next round of work is checked by something real.
6. **H5, then the moderates.**

---
