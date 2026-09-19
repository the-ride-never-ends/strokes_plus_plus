Feature: Lua action type
  As a user
  I want to assign Lua scripts to gestures
  So that gestures can execute programmable automation

  Scenario: Assign a Lua action to a gesture
    Given a gesture exists
    When the user assigns a Lua script action to the gesture
    And saves the configuration
    Then the gesture is mapped to the Lua script action

  Scenario: Execute a Lua action
    Given a recognized gesture is mapped to a valid Lua script
    When the gesture action executes
    Then the Lua script is executed

  Scenario: Complete a successful Lua action
    Given a Lua script executes without error
    When script execution finishes
    Then the action reports success

  Scenario: Preserve built-in actions
    Given existing built-in gesture actions are configured
    When Lua support is enabled
    Then existing built-in actions remain available
    And users are not required to convert them to Lua


Feature: Lua action persistence
  As a user
  I want Lua gesture actions to persist
  So that scripts remain assigned after restarting the application

  Scenario: Save an inline Lua action
    Given a gesture has an inline Lua script action
    When the configuration is saved
    And the application restarts
    Then the Lua action is restored
    And the script contents are restored

  Scenario: Preserve application-specific Lua action
    Given a Lua action is assigned inside an application profile
    When the application restarts
    Then the Lua action remains assigned to that application profile

  Scenario: Preserve global Lua action
    Given a Lua action is assigned as a global gesture action
    When the application restarts
    Then the global Lua action is restored


Feature: Lua gesture context
  As a script author
  I want access to information about the triggering gesture
  So that scripts can react to how the gesture was performed

  Scenario: Access gesture identifier
    Given a Lua action was triggered by a recognized gesture
    When the script reads gesture.id
    Then the value identifies the triggering gesture

  Scenario: Access gesture name
    Given a Lua action was triggered by a recognized gesture
    When the script reads gesture.name
    Then the value contains the gesture name

  Scenario: Access recognition score
    Given a Lua action was triggered by a recognized gesture
    When the script reads gesture.score
    Then the value contains the gesture recognition score

  Scenario: Access gesture start coordinates
    Given the triggering gesture has a recorded start position
    When the script reads gesture.start.x and gesture.start.y
    Then the recorded gesture start coordinates are returned

  Scenario: Access gesture finish coordinates
    Given the triggering gesture has a recorded finish position
    When the script reads gesture.finish.x and gesture.finish.y
    Then the recorded gesture finish coordinates are returned

  Scenario: Access optional gesture duration
    Given gesture duration information is available
    When the script reads gesture.duration
    Then the gesture duration is returned

  Scenario: Access optional gesture point count
    Given gesture point-count information is available
    When the script reads gesture.point_count
    Then the captured point count is returned

  Scenario: Access optional gesture distance
    Given gesture-distance information is available
    When the script reads gesture.distance
    Then the gesture distance is returned


Feature: Lua application context
  As a script author
  I want access to the application associated with the gesture
  So that scripts can behave differently in different applications

  Scenario: Access target process name
    Given a Lua action was triggered by a gesture
    When the script reads application.process
    Then the process name captured for the gesture is returned

  Scenario: Access target process identifier
    Given a Lua action was triggered by a gesture
    When the script reads application.process_id
    Then the captured process identifier is returned

  Scenario: Access target window title
    Given a Lua action was triggered by a gesture
    When the script reads application.title
    Then the captured window title is returned

  Scenario: Access target window class
    Given a Lua action was triggered by a gesture
    When the script reads application.class
    Then the captured window class is returned

  Scenario: Access executable path when available
    Given the executable path was captured for the target application
    When the script reads application.executable_path
    Then the executable path is returned


Feature: Read-only Lua context
  As the application
  I want gesture and application context to be read-only
  So that scripts cannot corrupt internal runtime state

  Scenario: Attempt to change the gesture name
    Given a Lua script assigns a new value to gesture.name
    When the script executes
    Then the internal gesture name is not modified

  Scenario: Attempt to change application process
    Given a Lua script assigns a new value to application.process
    When the script executes
    Then the internal application context is not modified


Feature: Lua window actions
  As a script author
  I want Lua to manipulate windows
  So that gestures can automate window management

  Scenario: Close the gesture window
    Given a valid gesture target window exists
    When the script calls window.close()
    Then the gesture target window receives a close request

  Scenario: Minimize the gesture window
    Given a valid gesture target window exists
    When the script calls window.minimize()
    Then the gesture target window is minimized

  Scenario: Maximize the gesture window
    Given a valid gesture target window exists
    When the script calls window.maximize()
    Then the gesture target window is maximized

  Scenario: Restore the gesture window
    Given a valid gesture target window exists
    When the script calls window.restore()
    Then the gesture target window is restored

  Scenario: Activate the gesture window
    Given a valid gesture target window exists
    And Windows permits the window to be activated
    When the script calls window.activate()
    Then the application requests activation of the gesture target window

  Scenario: Move the gesture window
    Given a valid gesture target window exists
    When the script calls window.move(100, 200)
    Then the gesture target window is moved to desktop position 100,200

  Scenario: Resize the gesture window
    Given a valid gesture target window exists
    When the script calls window.resize(1200, 800)
    Then the gesture target window is resized to 1200 by 800

  Scenario: Move and resize the gesture window
    Given a valid gesture target window exists
    When the script calls window.move_resize(100, 200, 1200, 800)
    Then the gesture target window is moved to desktop position 100,200
    And the gesture target window is resized to 1200 by 800


Feature: Lua explicit window targets
  As a script author
  I want window functions to target different contextual windows
  So that scripts can choose which window to manipulate

  Scenario: Use the gesture window explicitly
    Given a valid gesture target window exists
    When the script calls a window action with target "gesture"
    Then the gesture target window is used

  Scenario: Use the foreground window
    Given a foreground window exists
    When the script calls a window action with target "foreground"
    Then the foreground window at execution time is used

  Scenario: Default to gesture target
    Given no explicit window target is provided
    When a window action executes
    Then the gesture target window is used


Feature: Lua window information
  As a script author
  I want to inspect target-window properties
  So that scripts can make decisions based on window state

  Scenario: Read target window title
    Given a valid target window exists
    When the script calls window.title()
    Then the target window title is returned

  Scenario: Read target window class
    Given a valid target window exists
    When the script calls window.class()
    Then the target window class is returned

  Scenario: Read target window process
    Given a valid target window exists
    When the script calls window.process()
    Then the target window process is returned

  Scenario: Read target window bounds
    Given a valid target window exists
    When the script calls window.bounds()
    Then the result includes the window X coordinate
    And the result includes the window Y coordinate
    And the result includes the window width
    And the result includes the window height

  Scenario: Check whether target window exists
    Given a target window reference is available
    When the script calls window.exists()
    Then the result indicates whether the target window still exists


Feature: Lua keyboard hotkeys
  As a script author
  I want Lua to send keyboard shortcuts
  So that scripts can control applications through keyboard commands

  Scenario: Send Ctrl+W
    When the script calls keyboard.hotkey("CTRL", "W")
    Then Ctrl+W is sent

  Scenario: Send Ctrl+Shift+T
    When the script calls keyboard.hotkey("CTRL", "SHIFT", "T")
    Then Ctrl+Shift+T is sent

  Scenario: Send a single key
    When the script calls keyboard.press("F5")
    Then an F5 key press is sent

  Scenario: Send modifier key down
    When the script calls keyboard.down("CTRL")
    Then a Ctrl key-down event is sent

  Scenario: Send modifier key up
    When the script calls keyboard.up("CTRL")
    Then a Ctrl key-up event is sent


Feature: Lua keyboard state
  As a script author
  I want to inspect keyboard state
  So that gestures can perform modifier-sensitive actions

  Scenario: Detect a pressed modifier
    Given Shift is physically pressed
    When the script calls keyboard.is_down("SHIFT")
    Then the result is true

  Scenario: Detect a released modifier
    Given Shift is not physically pressed
    When the script calls keyboard.is_down("SHIFT")
    Then the result is false


Feature: Lua mouse position
  As a script author
  I want to inspect and change the mouse position
  So that scripts can perform pointer automation

  Scenario: Read current mouse position
    When the script calls mouse.position()
    Then the current desktop X coordinate is returned
    And the current desktop Y coordinate is returned

  Scenario: Move the pointer
    When the script calls mouse.move(800, 500)
    Then the pointer moves to desktop coordinates 800,500

  Scenario: Move to negative desktop coordinates
    Given a valid monitor occupies negative virtual-screen coordinates
    When the script calls mouse.move(-500, 300)
    Then the pointer moves to desktop coordinates -500,300


Feature: Lua mouse clicks
  As a script author
  I want Lua to generate mouse clicks
  So that scripts can interact with pointer-based interfaces

  Scenario Outline: Click a supported mouse button
    When the script calls mouse.click("<button>")
    Then a click is generated for "<button>"

    Examples:
      | button |
      | left   |
      | right  |
      | middle |
      | x1     |
      | x2     |

  Scenario Outline: Double-click a supported mouse button
    When the script calls mouse.double_click("<button>")
    Then a double-click is generated for "<button>"

    Examples:
      | button |
      | left   |
      | right  |
      | middle |
      | x1     |
      | x2     |

  Scenario Outline: Hold a supported mouse button
    When the script calls mouse.down("<button>")
    Then a button-down event is generated for "<button>"

    Examples:
      | button |
      | left   |
      | right  |
      | middle |
      | x1     |
      | x2     |

  Scenario Outline: Release a supported mouse button
    When the script calls mouse.up("<button>")
    Then a button-up event is generated for "<button>"

    Examples:
      | button |
      | left   |
      | right  |
      | middle |
      | x1     |
      | x2     |

  Scenario: Reject an invalid mouse button
    When the script calls mouse.click("banana")
    Then a Lua error is reported
    And no mouse click is generated


Feature: Lua process launching
  As a script author
  I want Lua to launch programs
  So that gesture scripts can start applications

  Scenario: Launch a process
    Given "wt.exe" can be launched
    When the script calls process.launch("wt.exe")
    Then "wt.exe" is launched

  Scenario: Launch a process with arguments
    Given the target executable can be launched
    When the script calls process.launch with arguments
    Then the executable is launched with the supplied arguments

  Scenario: Launch a process with a working directory
    Given the target executable can be launched
    When the script calls process.launch with a working directory
    Then the executable is launched using the supplied working directory

  Scenario: Report process launch failure
    Given the specified executable cannot be launched
    When the script calls process.launch
    Then the operation reports failure
    And the gesture engine remains operational


Feature: Lua shell opening
  As a script author
  I want Lua to open registered URLs and URIs
  So that scripts can launch websites and Windows handlers

  Scenario: Open an HTTPS URL
    When the script calls shell.open("https://example.com")
    Then the URI is opened using its registered Windows handler

  Scenario: Open Windows settings
    When the script calls shell.open("ms-settings:display")
    Then Windows opens the registered display settings destination

  Scenario: Open a mail URI
    When the script calls shell.open with a valid mailto URI
    Then the URI is opened using the registered mail handler

  Scenario: Report shell-open failure
    Given the supplied URI cannot be opened
    When the script calls shell.open
    Then the operation reports failure
    And the gesture engine remains operational


Feature: Lua media controls
  As a script author
  I want Lua to control media playback
  So that gestures can interact with system media

  Scenario: Toggle play and pause
    When the script calls media.play_pause()
    Then the system receives a Play/Pause command

  Scenario: Play next track
    When the script calls media.next()
    Then the system receives a Next Track command

  Scenario: Play previous track
    When the script calls media.previous()
    Then the system receives a Previous Track command

  Scenario: Stop playback
    When the script calls media.stop()
    Then the system receives a Stop command


Feature: Lua volume controls
  As a script author
  I want Lua to control system volume
  So that gestures can manage audio output

  Scenario: Increase volume
    Given the current volume can be increased
    When the script calls volume.increase(5)
    Then the system output volume increases by the requested amount within supported limits

  Scenario: Decrease volume
    Given the current volume can be decreased
    When the script calls volume.decrease(5)
    Then the system output volume decreases by the requested amount within supported limits

  Scenario: Toggle mute
    When the script calls volume.toggle_mute()
    Then the output mute state is toggled

  Scenario: Read current volume
    Given volume querying is supported
    When the script calls volume.get()
    Then the current output volume is returned

  Scenario: Set current volume
    Given volume setting is supported
    When the script calls volume.set(50)
    Then the system output volume is set to 50 on the documented scale

  Scenario: Read mute state
    Given mute-state querying is supported
    When the script calls volume.is_muted()
    Then the current mute state is returned


Feature: Lua virtual desktop controls
  As a script author
  I want Lua to control Windows virtual desktops
  So that gesture scripts can navigate workspaces

  Scenario: Move to next desktop
    Given a next virtual desktop is available
    When the script calls desktop.next()
    Then Windows switches to the next virtual desktop

  Scenario: Move to previous desktop
    Given a previous virtual desktop is available
    When the script calls desktop.previous()
    Then Windows switches to the previous virtual desktop

  Scenario: Create a virtual desktop
    Given virtual desktop creation is supported
    When the script calls desktop.create()
    Then a new virtual desktop is created

  Scenario: Close a virtual desktop
    Given virtual desktop closing is supported
    And the current desktop may be closed
    When the script calls desktop.close()
    Then the current virtual desktop is closed according to Windows behavior

  Scenario: Report unsupported desktop operation
    Given a requested virtual desktop operation is unsupported
    When the script requests the operation
    Then the operation reports failure
    And the gesture engine remains operational


Feature: Lua user messages
  As a script author
  I want scripts to display simple messages
  So that gestures can provide feedback

  Scenario: Display a user message
    When the script calls ui.message("Gesture executed")
    Then the user is shown the message "Gesture executed"

  Scenario: Display an on-screen message
    Given on-screen display is supported
    When the script calls ui.osd("Volume: 50%")
    Then "Volume: 50%" is displayed as an on-screen message
    And the message disappears automatically


Feature: Lua logging
  As a script author
  I want scripts to write diagnostic messages
  So that script behavior can be inspected

  Scenario Outline: Write a log message
    When the script calls log.<level>("Test message")
    Then "Test message" is written to application diagnostics at "<level>" level

    Examples:
      | level |
      | debug |
      | info  |
      | warn  |
      | error |


Feature: Lua script return values
  As a script author
  I want script return values to communicate action status
  So that scripts can explicitly succeed or fail

  Scenario: Return explicit success
    Given a Lua script completes with "return true"
    When execution finishes
    Then the Lua action reports success

  Scenario: Return explicit failure
    Given a Lua script completes with "return false"
    When execution finishes
    Then the Lua action reports failure

  Scenario: Return no value
    Given a Lua script completes without error
    And returns no value
    When execution finishes
    Then the Lua action reports success


Feature: Lua syntax errors
  As a user
  I want Lua syntax errors to be reported
  So that broken scripts can be corrected

  Scenario: Validate a script with valid syntax
    Given a Lua script contains valid syntax
    When the user validates the script
    Then validation succeeds

  Scenario: Validate a script with invalid syntax
    Given a Lua script contains invalid syntax
    When the user validates the script
    Then validation fails
    And the syntax error is reported

  Scenario: Report syntax-error location
    Given Lua can determine the source location of a syntax error
    When validation fails
    Then the error includes the relevant line information

  Scenario: Prevent syntax-error script execution
    Given a Lua action contains invalid syntax
    When execution is attempted
    Then no automation operation from that script executes
    And the Lua action reports failure


Feature: Lua runtime errors
  As a user
  I want runtime script errors to be reported
  So that failed scripts can be diagnosed

  Scenario: Report invalid API argument
    Given a Lua script passes an invalid argument to an application API
    When the script executes
    Then a Lua runtime error is reported

  Scenario: Report unavailable operation
    Given a Lua script requests an unavailable application operation
    When the script executes
    Then the failure is reported to the script or runtime

  Scenario: Report runtime error location
    Given Lua can determine the source location of a runtime error
    When the runtime error occurs
    Then the reported error includes the relevant script line

  Scenario: Recover after runtime error
    Given a Lua script encounters a runtime error
    When execution ends
    Then the gesture engine returns to the Idle state
    And subsequent gesture actions may execute


Feature: Lua execution limits
  As a user
  I want runaway scripts to be interrupted
  So that a programming mistake cannot permanently block Lua actions

  Scenario: Complete a short-running script
    Given a Lua script completes within the permitted execution limit
    When the script executes
    Then it is allowed to complete normally

  Scenario: Interrupt an infinite loop
    Given a Lua script contains an infinite loop
    When the configured execution limit is exceeded
    Then the script is interrupted
    And the Lua action reports failure
    And the gesture engine remains operational

  Scenario: Interrupt an excessively long script
    Given a Lua script exceeds the configured execution limit
    When the limit is reached
    Then the script execution is stopped
    And the Lua action reports that its execution limit was exceeded


Feature: Lua shutdown cancellation
  As the application
  I want executing scripts to stop during shutdown
  So that application shutdown can complete safely

  Scenario: Shut down while a Lua script is executing
    Given a Lua action is still executing
    When the application begins shutting down
    Then the Lua execution is cancelled or terminated safely
    And the application can complete shutdown


Feature: Lua API argument validation
  As a script author
  I want invalid API inputs to fail predictably
  So that scripting errors do not produce undefined behavior

  Scenario: Reject negative window width
    When the script calls window.resize(-5, 800)
    Then a Lua error or defined failure is returned
    And the invalid resize is not performed

  Scenario: Reject invalid mouse button
    When the script calls mouse.click("invalid")
    Then a Lua error or defined failure is returned

  Scenario: Reject invalid process arguments
    Given a required process-launch argument is invalid
    When process.launch is called
    Then the call fails predictably

  Scenario: Reject invalid target identifier
    Given a window action uses an unsupported target identifier
    When the function executes
    Then the call fails predictably


Feature: Lua API failure reporting
  As a script author
  I want automation APIs to expose operation failures
  So that scripts can respond to failures

  Scenario: Detect failed window activation
    Given Windows refuses a requested window activation
    When window.activate() executes
    Then the script receives a failure result or Lua error

  Scenario: Detect failed process launch
    Given a process cannot be launched
    When process.launch() executes
    Then the script receives a failure result or Lua error

  Scenario: Detect failed shell operation
    Given a URI cannot be opened
    When shell.open() executes
    Then the script receives a failure result or Lua error


Feature: Lua conditional automation
  As a script author
  I want scripts to branch based on runtime conditions
  So that one gesture can perform contextual actions

  Scenario: Branch on application process
    Given application.process is "chrome.exe"
    And a script tests application.process
    When the script executes
    Then the Chrome-specific branch can execute

  Scenario: Branch on modifier state
    Given Shift is physically pressed
    And the script checks keyboard.is_down("SHIFT")
    When the script executes
    Then the Shift-specific branch can execute

  Scenario: Branch on gesture direction
    Given the gesture finish X coordinate is greater than the gesture start X coordinate
    And the script compares those coordinates
    When the script executes
    Then the branch representing rightward movement can execute


Feature: Reusable Lua functions
  As a script author
  I want to define reusable Lua functions
  So that automation logic does not need to be copied between gestures

  Scenario: Define a reusable function
    Given shared Lua initialization code defines a function named close_tab
    When initialization succeeds
    Then close_tab is available to Lua actions

  Scenario: Call a reusable function from a gesture
    Given a shared function named close_tab is available
    And a gesture script calls close_tab()
    When the gesture executes
    Then the shared close_tab function executes

  Scenario: Reuse a function across multiple gestures
    Given two Lua gesture actions call the same shared function
    When either gesture executes
    Then the same shared function can be invoked


Feature: Lua initialization script
  As a script author
  I want initialization code to load automatically
  So that shared functions and values are available to gesture scripts

  Scenario: Load a valid initialization script
    Given a valid initialization script exists
    When the Lua runtime is initialized
    Then the initialization script executes
    And its shared definitions become available

  Scenario: Start without an initialization script
    Given no initialization script exists
    When the Lua runtime is initialized
    Then the runtime becomes available normally

  Scenario: Handle initialization syntax error
    Given the initialization script contains invalid Lua syntax
    When the Lua runtime initializes
    Then the initialization error is reported
    And the gesture engine remains operational

  Scenario: Handle initialization runtime error
    Given the initialization script encounters a runtime error
    When the Lua runtime initializes
    Then the error is reported
    And the gesture engine remains operational


Feature: User Lua modules
  As a script author
  I want to organize reusable automation into Lua modules
  So that larger script libraries can remain maintainable

  Scenario: Load a valid user module
    Given a valid Lua module exists in an approved user script location
    When a Lua script requires that module
    Then the module is loaded
    And its exported values are available

  Scenario: Reuse a loaded module
    Given a valid user module can be required
    When multiple scripts require the module
    Then each script can use the module's exported functionality

  Scenario: Fail to load a missing module
    Given a Lua script requires a module that does not exist
    When the script executes
    Then a Lua error is reported
    And the gesture engine remains operational

  Scenario: Do not load an unapproved module path
    Given a Lua script requests a module outside approved script locations
    When module loading is attempted
    Then the module is not loaded through the supported module system


Feature: Lua runtime reload
  As a script author
  I want to reload shared Lua code without restarting the application
  So that I can iterate on scripts quickly

  Scenario: Reload valid initialization code
    Given the initialization script has been changed
    When the user reloads the Lua runtime
    Then the updated initialization script is loaded
    And subsequent Lua actions use the updated definitions

  Scenario: Reload valid shared modules
    Given shared Lua modules have been changed
    When the Lua runtime is reloaded
    Then subsequent Lua actions can use the updated module definitions

  Scenario: Report reload initialization failure
    Given updated initialization code contains an error
    When the runtime is reloaded
    Then the initialization error is reported
    And the gesture engine remains operational


Feature: Lua runtime state
  As a script author
  I want runtime values to persist between script calls
  So that scripts can maintain temporary session state

  Scenario: Preserve a Lua global during the current runtime
    Given a script stores a value in a Lua global
    When another Lua action executes before the runtime is reset
    Then the later action can observe the stored global value

  Scenario: Reset runtime-only state on reload
    Given a Lua global exists only in runtime memory
    When the Lua runtime is reloaded
    Then the previous runtime-only value is not required to remain

  Scenario: Do not persist runtime-only state across application restart
    Given a Lua global exists only in runtime memory
    When the application restarts
    Then the previous runtime-only value is not required to be restored


Feature: Sequential Lua execution
  As the application
  I want Lua scripts to execute sequentially
  So that multiple scripts do not concurrently mutate the same runtime

  Scenario: Execute one Lua action
    Given no Lua action is currently executing
    When a Lua action is triggered
    Then that action begins execution

  Scenario: Trigger a second Lua action during execution
    Given one Lua action is executing
    When another Lua gesture action is triggered
    Then the second action is queued or handled according to the configured sequential execution policy
    And the two scripts do not execute concurrently in the same runtime

  Scenario: Execute queued Lua action
    Given a Lua action is waiting in the action queue
    When the currently executing Lua action finishes
    Then the queued Lua action becomes eligible to execute


Feature: Lua action queue protection
  As the application
  I want repeated script triggers to be controlled
  So that rapid gestures cannot create unbounded queued work

  Scenario: Queue actions within the supported limit
    Given the Lua action queue has available capacity
    When another Lua action is triggered
    Then the action may be queued

  Scenario: Reach the Lua action queue limit
    Given the Lua action queue has reached its supported limit
    When another Lua action is triggered
    Then the application does not allow unbounded queue growth
    And the condition is handled predictably


Feature: Lua script editor
  As a user
  I want to edit Lua actions inside the settings interface
  So that scripts can be created without external tools

  Scenario: Select Lua Script action type
    Given the user is configuring a gesture action
    When the user selects "Lua Script"
    Then a Lua script editor is displayed

  Scenario: Enter multiline Lua code
    Given the Lua script editor is open
    When the user enters code containing multiple lines
    Then the complete multiline script is retained

  Scenario: Copy and paste script text
    Given the Lua script editor is open
    When the user copies and pastes text
    Then standard copy and paste behavior is available

  Scenario: Undo script edit
    Given the user modifies script text
    When the user performs Undo
    Then the previous editor state is restored

  Scenario: Redo script edit
    Given the user has undone a script edit
    When the user performs Redo
    Then the undone edit is restored

  Scenario: Save valid script
    Given the script contains valid Lua code
    When the user saves the Lua action
    Then the script is persisted with the action


Feature: Lua script validation
  As a user
  I want to validate Lua scripts before using them
  So that syntax errors can be corrected before gesture execution

  Scenario: Validate valid script
    Given the editor contains syntactically valid Lua
    When the user selects Validate
    Then validation reports success

  Scenario: Validate invalid script
    Given the editor contains syntactically invalid Lua
    When the user selects Validate
    Then validation reports failure
    And the Lua error is displayed

  Scenario: Preserve script after failed validation
    Given script validation fails
    When the validation result is displayed
    Then the user's script remains available for editing


Feature: Lua script testing
  As a user
  I want to test scripts from the settings interface
  So that automation can be verified without drawing the gesture

  Scenario: Run a valid script from the editor
    Given the editor contains a valid Lua script
    When the user selects Run or Test
    Then the script is executed through the Lua action environment
    And the execution result is displayed

  Scenario: Report test execution failure
    Given a test script encounters an error
    When the user runs the script
    Then the error is displayed
    And the settings interface remains usable

  Scenario: Test script without normal gesture context
    Given the user runs a script from the editor
    And no real triggering gesture exists
    When the script requests unavailable gesture-specific data
    Then the application returns a defined unavailable value or clear error
    And does not expose undefined behavior


Feature: Lua API documentation
  As a script author
  I want supported Lua functions to be discoverable
  So that I can write scripts without consulting source code

  Scenario: View available namespaces
    When the user opens Lua API help
    Then the available namespaces are listed
    And the list includes gesture
    And the list includes application
    And the list includes window
    And the list includes keyboard
    And the list includes mouse
    And the list includes process
    And the list includes shell
    And the list includes media
    And the list includes volume
    And the list includes desktop
    And the list includes ui
    And the list includes log

  Scenario: View function documentation
    Given a documented Lua API function exists
    When the user views its documentation
    Then the function name is shown
    And its parameters are shown
    And its return behavior is shown
    And its description is shown


Feature: Lua and application-profile precedence
  As a user
  I want Lua actions to follow existing action precedence
  So that scripting behaves consistently with other action types

  Scenario: Application-specific Lua action overrides global built-in action
    Given a gesture has a global built-in action
    And the matching application profile defines a Lua action for the same gesture
    When the gesture is performed in that application
    Then the application-specific Lua action executes
    And the global action does not execute

  Scenario: Application-specific built-in action overrides global Lua action
    Given a gesture has a global Lua action
    And the matching application profile defines a built-in action for the same gesture
    When the gesture is performed in that application
    Then the application-specific built-in action executes
    And the global Lua action does not execute

  Scenario: Fall back to global Lua action
    Given no matching application profile defines an action for the gesture
    And a global Lua action exists
    When the gesture is recognized
    Then the global Lua action executes


Feature: Lua automation parity
  As a script author
  I want Lua to access Phase 2 automation capabilities
  So that scripts can compose existing actions dynamically

  Scenario: Use keyboard capability from Lua
    When a Lua script requests a supported keyboard operation
    Then the equivalent keyboard automation is available

  Scenario: Use process capability from Lua
    When a Lua script requests a supported process launch
    Then the equivalent process automation is available

  Scenario: Use shell capability from Lua
    When a Lua script requests a supported URL or URI action
    Then the equivalent shell automation is available

  Scenario: Use mouse capability from Lua
    When a Lua script requests a supported mouse operation
    Then the equivalent mouse automation is available

  Scenario: Use window capability from Lua
    When a Lua script requests a supported window operation
    Then the equivalent window automation is available

  Scenario: Use media capability from Lua
    When a Lua script requests a supported media operation
    Then the equivalent media automation is available

  Scenario: Use volume capability from Lua
    When a Lua script requests a supported volume operation
    Then the equivalent volume automation is available

  Scenario: Use virtual desktop capability from Lua
    When a Lua script requests a supported virtual desktop operation
    Then the equivalent virtual desktop automation is available


Feature: Lua failure recovery
  As a user
  I want failed scripts to leave the gesture system usable
  So that a script bug does not disable mouse gestures

  Scenario: Return to idle after script failure
    Given a Lua action encounters an error
    When script execution ends
    Then the gesture engine returns to the Idle state

  Scenario: Run another Lua action after failure
    Given a previous Lua action failed
    And the gesture engine returned to Idle
    When another valid Lua gesture is performed
    Then the later Lua action can execute normally

  Scenario: Run built-in action after Lua failure
    Given a previous Lua action failed
    When the user performs a gesture mapped to a built-in action
    Then the built-in action can execute normally

  Scenario: Preserve gesture input after Lua failure
    Given a Lua action fails
    Then gesture capture remains enabled unless the user explicitly disables it
