# Phase 3 TODO: Lua Scripting

Every scenario in `PHASE_3_SPEC.feature` as a TODO item, grouped by feature.
Line references point into `PHASE_3_SPEC.feature`. 41 features, 155 scenarios.

## Lua action type (L1)

- [x] Assign a Lua action to a gesture (L6)
- [x] Execute a Lua action (L12)
- [x] Complete a successful Lua action (L17)
- [x] Preserve built-in actions (L22)

## Lua action persistence (L29)

- [x] Save an inline Lua action (L34)
- [x] Preserve application-specific Lua action (L41)
- [x] Preserve global Lua action (L46)

## Lua gesture context (L52)

- [x] Access gesture identifier (L57)
- [x] Access gesture name (L62)
- [x] Access recognition score (L67)
- [x] Access gesture start coordinates (L72)
- [x] Access gesture finish coordinates (L77)
- [x] Access optional gesture duration (L82)
- [x] Access optional gesture point count (L87)
- [x] Access optional gesture distance (L92)

## Lua application context (L98)

- [x] Access target process name (L103)
- [x] Access target process identifier (L108)
- [x] Access target window title (L113)
- [x] Access target window class (L118)
- [x] Access executable path when available (L123)

## Read-only Lua context (L129)

- [x] Attempt to change the gesture name (L134)
- [x] Attempt to change application process (L139)

## Lua window actions (L145)

- [x] Close the gesture window (L150)
- [x] Minimize the gesture window (L155)
- [x] Maximize the gesture window (L160)
- [x] Restore the gesture window (L165)
- [x] Activate the gesture window (L170)
- [x] Move the gesture window (L176)
- [x] Resize the gesture window (L181)
- [x] Move and resize the gesture window (L186)

## Lua explicit window targets (L193)

- [x] Use the gesture window explicitly (L198)
- [x] Use the foreground window (L203)
- [x] Default to gesture target (L208)

## Lua window information (L214)

- [x] Read target window title (L219)
- [x] Read target window class (L224)
- [x] Read target window process (L229)
- [x] Read target window bounds (L234)
- [x] Check whether target window exists (L242)

## Lua keyboard hotkeys (L248)

- [x] Send Ctrl+W (L253)
- [x] Send Ctrl+Shift+T (L257)
- [x] Send a single key (L261)
- [x] Send modifier key down (L265)
- [x] Send modifier key up (L269)

## Lua keyboard state (L274)

- [x] Detect a pressed modifier (L279)
- [x] Detect a released modifier (L284)

## Lua mouse position (L290)

- [x] Read current mouse position (L295)
- [x] Move the pointer (L300)
- [x] Move to negative desktop coordinates (L304)

## Lua mouse clicks (L310)

- [x] Click a supported mouse button (L315)
- [x] Double-click a supported mouse button (L327)
- [x] Hold a supported mouse button (L339)
- [x] Release a supported mouse button (L351)
- [x] Reject an invalid mouse button (L363)

## Lua process launching (L369)

- [x] Launch a process (L374)
- [x] Launch a process with arguments (L379)
- [x] Launch a process with a working directory (L384)
- [x] Report process launch failure (L389)

## Lua shell opening (L396)

- [x] Open an HTTPS URL (L401)
- [x] Open Windows settings (L405)
- [x] Open a mail URI (L409)
- [x] Report shell-open failure (L413)

## Lua media controls (L420)

- [x] Toggle play and pause (L425)
- [x] Play next track (L429)
- [x] Play previous track (L433)
- [x] Stop playback (L437)

## Lua volume controls (L442)

- [x] Increase volume (L447)
- [x] Decrease volume (L452)
- [x] Toggle mute (L457)
- [x] Read current volume (L461)
- [x] Set current volume (L466)
- [x] Read mute state (L471)

## Lua virtual desktop controls (L477)

- [x] Move to next desktop (L482)
- [x] Move to previous desktop (L487)
- [x] Create a virtual desktop (L492)
- [x] Close a virtual desktop (L497)
- [x] Report unsupported desktop operation (L503)

## Lua user messages (L510)

- [x] Display a user message (L515)
- [x] Display an on-screen message (L519)

## Lua logging (L526)

- [x] Write a log message (L531)

## Lua script return values (L543)

- [x] Return explicit success (L548)
- [x] Return explicit failure (L553)
- [x] Return no value (L558)

## Lua syntax errors (L565)

- [x] Validate a script with valid syntax (L570)
- [x] Validate a script with invalid syntax (L575)
- [x] Report syntax-error location (L581)
- [x] Prevent syntax-error script execution (L586)

## Lua runtime errors (L593)

- [x] Report invalid API argument (L598)
- [x] Report unavailable operation (L603)
- [x] Report runtime error location (L608)
- [x] Recover after runtime error (L613)

## Lua execution limits (L620)

- [x] Complete a short-running script (L625)
- [x] Interrupt an infinite loop (L630)
- [x] Interrupt an excessively long script (L637)

## Lua shutdown cancellation (L644)

- [x] Shut down while a Lua script is executing (L649)

## Lua API argument validation (L656)

- [x] Reject negative window width (L661)
- [x] Reject invalid mouse button (L666)
- [x] Reject invalid process arguments (L670)
- [x] Reject invalid target identifier (L675)

## Lua API failure reporting (L681)

- [x] Detect failed window activation (L686)
- [x] Detect failed process launch (L691)
- [x] Detect failed shell operation (L696)

## Lua conditional automation (L702)

- [x] Branch on application process (L707)
- [x] Branch on modifier state (L713)
- [x] Branch on gesture direction (L719)

## Reusable Lua functions (L726)

- [x] Define a reusable function (L731)
- [x] Call a reusable function from a gesture (L736)
- [x] Reuse a function across multiple gestures (L742)

## Lua initialization script (L748)

- [x] Load a valid initialization script (L753)
- [x] Start without an initialization script (L759)
- [x] Handle initialization syntax error (L764)
- [x] Handle initialization runtime error (L770)

## User Lua modules (L777)

- [x] Load a valid user module (L782)
- [x] Reuse a loaded module (L788)
- [x] Fail to load a missing module (L793)
- [x] Do not load an unapproved module path (L799)

## Lua runtime reload (L805)

- [x] Reload valid initialization code (L810)
- [x] Reload valid shared modules (L816)
- [x] Report reload initialization failure (L821)

## Lua runtime state (L828)

- [x] Preserve a Lua global during the current runtime (L833)
- [x] Reset runtime-only state on reload (L838)
- [x] Do not persist runtime-only state across application restart (L843)

## Sequential Lua execution (L849)

- [x] Execute one Lua action (L854)
- [x] Trigger a second Lua action during execution (L859)
- [x] Execute queued Lua action (L865)

## Lua action queue protection (L871)

- [x] Queue actions within the supported limit (L876)
- [x] Reach the Lua action queue limit (L881)

## Lua script editor (L888)

- [x] Select Lua Script action type (L893)
- [x] Enter multiline Lua code (L898)
- [x] Copy and paste script text (L903)
- [x] Undo script edit (L908)
- [x] Redo script edit (L913)
- [x] Save valid script (L918)

## Lua script validation (L924)

- [x] Validate valid script (L929)
- [x] Validate invalid script (L934)
- [x] Preserve script after failed validation (L940)

## Lua script testing (L946)

- [x] Run a valid script from the editor (L951)
- [x] Report test execution failure (L957)
- [x] Test script without normal gesture context (L963)

## Lua API documentation (L971)

- [x] View available namespaces (L976)
- [x] View function documentation (L992)

## Lua and application-profile precedence (L1001)

- [x] Application-specific Lua action overrides global built-in action (L1006)
- [x] Application-specific built-in action overrides global Lua action (L1013)
- [x] Fall back to global Lua action (L1020)

## Lua automation parity (L1027)

- [x] Use keyboard capability from Lua (L1032)
- [x] Use process capability from Lua (L1036)
- [x] Use shell capability from Lua (L1040)
- [x] Use mouse capability from Lua (L1044)
- [x] Use window capability from Lua (L1048)
- [x] Use media capability from Lua (L1052)
- [x] Use volume capability from Lua (L1056)
- [x] Use virtual desktop capability from Lua (L1060)

## Lua failure recovery (L1065)

- [x] Return to idle after script failure (L1070)
- [x] Run another Lua action after failure (L1075)
- [x] Run built-in action after Lua failure (L1081)
- [x] Preserve gesture input after Lua failure (L1086)
