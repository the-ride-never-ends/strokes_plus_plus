Feature: Generic action resolution
  As a user
  I want gestures to resolve to different kinds of actions
  So that gestures can perform more than keyboard shortcuts

  Scenario: Resolve a keyboard action
    Given a recognized gesture is mapped to a keyboard action
    When the gesture action is resolved
    Then the resolved action type is Keyboard

  Scenario: Resolve a process action
    Given a recognized gesture is mapped to a process action
    When the gesture action is resolved
    Then the resolved action type is Process

  Scenario: Resolve a URL action
    Given a recognized gesture is mapped to a URL action
    When the gesture action is resolved
    Then the resolved action type is URL

  Scenario: Resolve a mouse action
    Given a recognized gesture is mapped to a mouse action
    When the gesture action is resolved
    Then the resolved action type is Mouse

  Scenario: Resolve a window action
    Given a recognized gesture is mapped to a window action
    When the gesture action is resolved
    Then the resolved action type is Window

  Scenario: Resolve a media action
    Given a recognized gesture is mapped to a media action
    When the gesture action is resolved
    Then the resolved action type is Media

  Scenario: Resolve a volume action
    Given a recognized gesture is mapped to a volume action
    When the gesture action is resolved
    Then the resolved action type is Volume

  Scenario: Resolve a virtual desktop action
    Given a recognized gesture is mapped to a virtual desktop action
    When the gesture action is resolved
    Then the resolved action type is Virtual Desktop


Feature: Action execution context
  As a user
  I want actions to use information from the gesture that triggered them
  So that actions can behave relative to the original interaction

  Scenario: Provide gesture start position
    Given a recognized gesture has a recorded start position
    When its action executes
    Then the action can access the gesture start position

  Scenario: Provide gesture end position
    Given a recognized gesture has a recorded end position
    When its action executes
    Then the action can access the gesture end position

  Scenario: Provide current cursor position
    Given an action is executing
    When the action requests the current cursor position
    Then the current cursor position is available to the action

  Scenario: Provide gesture target window
    Given a gesture began over a valid target window
    When its action executes
    Then the action can access that gesture target window

  Scenario: Provide foreground window
    Given an action is executing
    When the action requests the current foreground window
    Then the foreground window is available to the action

  Scenario: Provide gesture application context
    Given a gesture has recorded application context
    When its action executes
    Then the executable name is available
    And the window title is available
    And the window class is available
    And the process identifier is available

  Scenario: Provide modifier state
    Given modifier keys were recorded for the gesture
    When its action executes
    Then the recorded modifier state is available to the action


Feature: Action result handling
  As a user
  I want action execution to report whether it succeeded
  So that failed actions can be diagnosed without breaking gesture handling

  Scenario: Report successful action execution
    Given a valid action can be completed
    When the action executes
    Then the action reports success

  Scenario: Report failed action execution
    Given an action cannot be completed
    When the action executes
    Then the action reports failure
    And the action provides a failure reason

  Scenario: Continue after action failure
    Given an action fails
    When action execution finishes
    Then the gesture engine remains operational
    And subsequent gestures can still execute actions


Feature: Persist generic actions
  As a user
  I want action mappings to persist
  So that configured actions remain available after restart

  Scenario: Save an action definition
    Given a gesture is assigned a valid action
    When the configuration is saved
    Then the action type is persisted
    And the action parameters are persisted

  Scenario: Reload an action definition
    Given a valid action definition was previously saved
    When the application restarts
    Then the action definition is restored
    And the gesture remains mapped to that action

  Scenario: Reject an unknown action type
    Given a stored action definition contains an unsupported action type
    When the configuration is loaded
    Then the unsupported action is not treated as executable
    And the application continues operating


Feature: Keyboard shortcut actions
  As a user
  I want existing keyboard shortcut actions to continue working
  So that Phase 1 gesture mappings remain useful

  Scenario Outline: Execute a keyboard shortcut
    Given a gesture is mapped to keyboard shortcut "<shortcut>"
    When the gesture action executes
    Then the application sends keyboard shortcut "<shortcut>"

    Examples:
      | shortcut       |
      | CTRL+W         |
      | CTRL+T         |
      | ALT+LEFT       |
      | WIN+D          |
      | CTRL+SHIFT+TAB |

  Scenario: Release injected modifier keys
    Given a keyboard action injects one or more modifier keys
    When the keyboard action finishes
    Then all modifier keys injected by the action are released

  Scenario: Report keyboard action failure
    Given a keyboard action cannot be completed
    When the action executes
    Then the action reports failure
    And the gesture engine remains operational


Feature: Launch executable actions
  As a user
  I want a gesture to launch an executable
  So that applications and tools can be started directly

  Scenario: Launch an executable
    Given a gesture is mapped to a process launch action
    And the configured executable can be launched
    When the gesture action executes
    Then the configured executable is launched

  Scenario: Launch an executable with arguments
    Given a process action contains command-line arguments
    When the process action executes
    Then the executable is launched with the configured arguments

  Scenario: Launch an executable with a working directory
    Given a process action contains a working directory
    When the process action executes
    Then the executable is launched using the configured working directory

  Scenario: Launch an executable with arguments and working directory
    Given a process action contains command-line arguments
    And the process action contains a working directory
    When the process action executes
    Then the executable is launched with the configured arguments
    And the configured working directory is used

  Scenario: Fail when executable does not exist
    Given a process action references an executable that does not exist
    When the action executes
    Then the action reports failure
    And no process is launched

  Scenario: Fail when executable cannot be started
    Given a process action references an executable that cannot be started
    When the action executes
    Then the action reports failure
    And the gesture engine remains operational


Feature: URL and URI actions
  As a user
  I want gestures to open URLs and registered URIs
  So that gestures can launch websites and Windows handlers

  Scenario: Open an HTTPS URL
    Given a gesture is mapped to a valid HTTPS URL
    When the action executes
    Then the URL is opened using the registered Windows handler

  Scenario: Open an HTTP URL
    Given a gesture is mapped to a valid HTTP URL
    When the action executes
    Then the URL is opened using the registered Windows handler

  Scenario: Open a registered non-HTTP URI
    Given a gesture is mapped to a URI with a registered Windows handler
    When the action executes
    Then the URI is opened using its registered handler

  Scenario: Open a mail URI
    Given a gesture is mapped to a valid mailto URI
    When the action executes
    Then Windows opens the URI using the registered mail handler

  Scenario: Open a Windows settings URI
    Given a gesture is mapped to a supported ms-settings URI
    When the action executes
    Then Windows opens the requested settings destination

  Scenario: Reject an empty URI
    Given a URL action contains no URI
    When the action is validated
    Then the action is invalid

  Scenario: Fail when no handler is available
    Given a URI has no usable registered handler
    When the action executes
    Then the action reports failure
    And the gesture engine remains operational


Feature: Mouse click actions
  As a user
  I want gestures to generate mouse clicks
  So that gestures can automate pointer interactions

  Scenario Outline: Perform a mouse click
    Given a gesture is mapped to a "<button>" click action
    When the action executes
    Then one click of the "<button>" button is generated

    Examples:
      | button   |
      | Left     |
      | Right    |
      | Middle   |
      | XButton1 |
      | XButton2 |

  Scenario Outline: Perform a mouse double click
    Given a gesture is mapped to a "<button>" double-click action
    When the action executes
    Then two valid clicks of the "<button>" button are generated

    Examples:
      | button   |
      | Left     |
      | Right    |
      | Middle   |
      | XButton1 |
      | XButton2 |

  Scenario Outline: Perform mouse button down
    Given a gesture is mapped to a "<button>" button-down action
    When the action executes
    Then a button-down event is generated for "<button>"

    Examples:
      | button   |
      | Left     |
      | Right    |
      | Middle   |
      | XButton1 |
      | XButton2 |

  Scenario Outline: Perform mouse button up
    Given a gesture is mapped to a "<button>" button-up action
    When the action executes
    Then a button-up event is generated for "<button>"

    Examples:
      | button   |
      | Left     |
      | Right    |
      | Middle   |
      | XButton1 |
      | XButton2 |


Feature: Mouse position targets
  As a user
  I want mouse actions to target contextual positions
  So that gestures can interact with locations related to the gesture

  Scenario: Use current cursor position
    Given a mouse action uses position target "current_cursor"
    When the action executes
    Then the mouse action uses the cursor position at execution time

  Scenario: Use gesture start position
    Given a mouse action uses position target "gesture_start"
    When the action executes
    Then the mouse action uses the recorded gesture start position

  Scenario: Use gesture end position
    Given a mouse action uses position target "gesture_end"
    When the action executes
    Then the mouse action uses the recorded gesture end position

  Scenario: Use absolute position
    Given a mouse action uses an absolute position
    And valid X and Y coordinates are configured
    When the action executes
    Then the mouse action uses the configured desktop coordinates

  Scenario: Use negative absolute coordinates
    Given a mouse action uses an absolute position on a monitor with negative virtual-screen coordinates
    When the action executes
    Then the negative coordinates are accepted
    And the requested desktop position is used

  Scenario: Reject absolute position without coordinates
    Given a mouse action uses an absolute position
    And required coordinates are missing
    When the action is validated
    Then the action is invalid


Feature: Mouse movement actions
  As a user
  I want gestures to move the pointer
  So that the cursor can be repositioned automatically

  Scenario: Move to an absolute position
    Given a mouse move action specifies valid absolute coordinates
    When the action executes
    Then the pointer moves to the configured coordinates

  Scenario: Move to the gesture start position
    Given a mouse move action targets "gesture_start"
    When the action executes
    Then the pointer moves to the gesture start position

  Scenario: Move to the gesture end position
    Given a mouse move action targets "gesture_end"
    When the action executes
    Then the pointer moves to the gesture end position

  Scenario: Move across monitor boundaries
    Given the target position is on another monitor
    When the mouse move action executes
    Then the pointer moves to the target position on that monitor


Feature: Mouse click at contextual position
  As a user
  I want clicks to occur at gesture-related positions
  So that gestures can operate on the location where they were drawn

  Scenario: Click at gesture start
    Given a mouse click action targets "gesture_start"
    When the action executes
    Then the click occurs at the recorded gesture start position

  Scenario: Click at gesture end
    Given a mouse click action targets "gesture_end"
    When the action executes
    Then the click occurs at the recorded gesture end position

  Scenario: Click at an absolute position
    Given a mouse click action contains an absolute position
    When the action executes
    Then the click occurs at the configured desktop position


Feature: Window target resolution
  As a user
  I want window actions to target contextual windows
  So that gestures can manipulate the intended application

  Scenario: Target the gesture window
    Given a window action targets "gesture_window"
    And the gesture recorded a valid target window
    When the action executes
    Then the recorded gesture window is used

  Scenario: Target the current foreground window
    Given a window action targets "foreground_window"
    When the action executes
    Then the window that is foreground at execution time is used

  Scenario: Target the window at gesture start
    Given a window action targets "window_at_gesture_start"
    When the action executes
    Then the window recorded at the gesture start position is used

  Scenario: Fail when the target window no longer exists
    Given a window action references a window that has been destroyed
    When the action executes
    Then the action reports failure
    And no different window is substituted automatically


Feature: Close window actions
  As a user
  I want gestures to close windows
  So that I can dismiss applications without clicking their controls

  Scenario: Close the gesture target window
    Given a gesture is mapped to a close-window action
    And the gesture target window exists
    When the action executes
    Then the target window receives a normal request to close

  Scenario: Fail to close an invalid target
    Given a close-window action has no valid target window
    When the action executes
    Then the action reports failure
    And the gesture engine remains operational


Feature: Minimize window actions
  As a user
  I want gestures to minimize windows
  So that I can quickly hide them

  Scenario: Minimize a normal window
    Given a gesture is mapped to a minimize-window action
    And the target window can be minimized
    When the action executes
    Then the target window is minimized

  Scenario: Minimize an already minimized window
    Given the target window is already minimized
    When a minimize-window action executes
    Then the action completes without destabilizing the gesture engine


Feature: Maximize window actions
  As a user
  I want gestures to maximize windows
  So that I can quickly expand them

  Scenario: Maximize a normal window
    Given a gesture is mapped to a maximize-window action
    And the target window can be maximized
    When the action executes
    Then the target window is maximized

  Scenario: Maximize an already maximized window
    Given the target window is already maximized
    When a maximize-window action executes
    Then the action completes without destabilizing the gesture engine


Feature: Restore window actions
  As a user
  I want gestures to restore minimized or maximized windows
  So that they can return to their normal state

  Scenario: Restore a minimized window
    Given the target window is minimized
    When a restore-window action executes
    Then the target window is restored

  Scenario: Restore a maximized window
    Given the target window is maximized
    When a restore-window action executes
    Then the target window returns to its normal restored state


Feature: Activate window actions
  As a user
  I want gestures to activate windows
  So that I can bring a target window to the foreground

  Scenario: Activate a valid target window
    Given a gesture is mapped to an activate-window action
    And the target window may be activated
    When the action executes
    Then the application requests that the target window become foreground

  Scenario: Report activation failure
    Given Windows prevents the target window from becoming foreground
    When the activate-window action executes
    Then the action reports failure
    And the gesture engine remains operational


Feature: Move window actions
  As a user
  I want gestures to reposition windows
  So that I can arrange applications quickly

  Scenario: Move a window to positive coordinates
    Given a move-window action specifies X coordinate 100
    And the action specifies Y coordinate 100
    When the action executes
    Then the target window is moved to desktop position 100,100
    And its existing size is preserved

  Scenario: Move a window to negative coordinates
    Given a move-window action specifies negative virtual-screen coordinates
    When the action executes
    Then the target window is moved to the configured coordinates

  Scenario: Move a window to another monitor
    Given the configured coordinates lie on another monitor
    When the move-window action executes
    Then the target window is moved to that monitor


Feature: Resize window actions
  As a user
  I want gestures to resize windows
  So that I can adjust application dimensions quickly

  Scenario: Resize a window
    Given a resize-window action specifies width 1200
    And the action specifies height 800
    When the action executes
    Then the target window is resized to 1200 by 800
    And its existing origin is preserved

  Scenario: Reject resize without width
    Given a resize-window action does not specify a width
    When the action is validated
    Then the action is invalid

  Scenario: Reject resize without height
    Given a resize-window action does not specify a height
    When the action is validated
    Then the action is invalid


Feature: Move and resize window actions
  As a user
  I want gestures to reposition and resize windows together
  So that a complete window layout operation can be performed at once

  Scenario: Move and resize a window
    Given a move-resize action specifies valid X and Y coordinates
    And valid width and height values are configured
    When the action executes
    Then the target window is moved to the configured position
    And the target window is resized to the configured dimensions

  Scenario: Reject incomplete move-resize action
    Given a move-resize action is missing a required position or size value
    When the action is validated
    Then the action is invalid


Feature: Window information
  As a user
  I want window-related actions to have access to window geometry
  So that later actions can use the current window state

  Scenario: Retrieve window bounds
    Given a valid target window exists
    When the current window bounds are requested
    Then the window left coordinate is available
    And the window top coordinate is available
    And the window right coordinate is available
    And the window bottom coordinate is available
    And the window width is available
    And the window height is available

  Scenario: Determine the target window monitor
    Given a valid target window exists
    When its monitor information is requested
    Then the monitor containing the window is identified

  Scenario: Retrieve monitor bounds
    Given a target monitor has been identified
    When its geometry is requested
    Then the monitor bounds are available
    And the monitor work area is available


Feature: Media playback actions
  As a user
  I want gestures to control system media playback
  So that playback can be controlled from any application

  Scenario: Toggle play and pause
    Given a gesture is mapped to Play/Pause
    When the action executes
    Then the system receives a Play/Pause media command

  Scenario: Play the next track
    Given a gesture is mapped to Next Track
    When the action executes
    Then the system receives a Next Track media command

  Scenario: Play the previous track
    Given a gesture is mapped to Previous Track
    When the action executes
    Then the system receives a Previous Track media command

  Scenario: Stop playback
    Given a gesture is mapped to Stop
    When the action executes
    Then the system receives a Stop media command


Feature: System volume actions
  As a user
  I want gestures to control system output volume
  So that audio can be adjusted without using system controls

  Scenario: Increase system volume
    Given a gesture is mapped to Volume Increase
    When the action executes
    Then the system output volume increases

  Scenario: Decrease system volume
    Given a gesture is mapped to Volume Decrease
    When the action executes
    Then the system output volume decreases

  Scenario: Toggle mute
    Given a gesture is mapped to Mute Toggle
    When the action executes
    Then the system output mute state is toggled

  Scenario: Increase volume by a configured amount
    Given a volume increase action specifies an amount
    When the action executes
    Then the system output volume increases by the configured amount subject to supported system limits

  Scenario: Decrease volume by a configured amount
    Given a volume decrease action specifies an amount
    When the action executes
    Then the system output volume decreases by the configured amount subject to supported system limits

  Scenario: Prevent volume from exceeding the supported maximum
    Given the system volume is near its maximum
    When a volume increase action executes
    Then the resulting volume does not exceed the supported maximum

  Scenario: Prevent volume from dropping below the supported minimum
    Given the system volume is near its minimum
    When a volume decrease action executes
    Then the resulting volume does not fall below the supported minimum


Feature: Virtual desktop navigation
  As a user
  I want gestures to switch between virtual desktops
  So that I can navigate workspaces quickly

  Scenario: Switch to the next virtual desktop
    Given a next-desktop action is supported
    When the action executes
    Then Windows switches to the next available virtual desktop

  Scenario: Switch to the previous virtual desktop
    Given a previous-desktop action is supported
    When the action executes
    Then Windows switches to the previous available virtual desktop

  Scenario: Handle next desktop when no next desktop is available
    Given the current desktop has no next available desktop
    When a next-desktop action executes
    Then the application handles the condition without destabilizing the gesture engine

  Scenario: Handle previous desktop when no previous desktop is available
    Given the current desktop has no previous available desktop
    When a previous-desktop action executes
    Then the application handles the condition without destabilizing the gesture engine


Feature: Virtual desktop creation
  As a user
  I want a gesture to create a virtual desktop when supported
  So that I can create workspaces quickly

  Scenario: Create a virtual desktop
    Given virtual desktop creation is supported
    And a gesture is mapped to Create Desktop
    When the action executes
    Then a new virtual desktop is created

  Scenario: Handle unsupported desktop creation
    Given virtual desktop creation is unavailable
    When a Create Desktop action executes
    Then the action reports that the operation is unsupported
    And the gesture engine remains operational


Feature: Virtual desktop closing
  As a user
  I want a gesture to close a virtual desktop when supported
  So that I can remove workspaces quickly

  Scenario: Close the current virtual desktop
    Given virtual desktop closing is supported
    And the current desktop may be closed
    When a Close Desktop action executes
    Then the current virtual desktop is closed according to Windows behavior

  Scenario: Handle unsupported desktop closing
    Given virtual desktop closing is unavailable
    When a Close Desktop action executes
    Then the action reports that the operation is unsupported
    And the gesture engine remains operational


Feature: Contextual symbolic positions
  As a user
  I want action definitions to reference contextual positions
  So that actions do not always require hard-coded coordinates

  Scenario: Resolve gesture_start
    Given an action parameter references "gesture_start"
    When the parameter is resolved
    Then it resolves to the recorded gesture start coordinates

  Scenario: Resolve gesture_end
    Given an action parameter references "gesture_end"
    When the parameter is resolved
    Then it resolves to the recorded gesture end coordinates

  Scenario: Resolve current_cursor
    Given an action parameter references "current_cursor"
    When the parameter is resolved
    Then it resolves to the current cursor coordinates


Feature: Contextual symbolic windows
  As a user
  I want action definitions to reference contextual windows
  So that actions can operate on the correct window dynamically

  Scenario: Resolve gesture_window
    Given an action target references "gesture_window"
    When the target is resolved
    Then it resolves to the window associated with the gesture

  Scenario: Resolve foreground_window
    Given an action target references "foreground_window"
    When the target is resolved
    Then it resolves to the foreground window at execution time


Feature: Application-specific action precedence
  As a user
  I want application-specific actions to override global actions
  So that gestures can behave differently in different applications

  Scenario: Use application-specific process action
    Given a gesture has a global action
    And the same gesture has a process action in the matching application profile
    When the gesture is performed in that application
    Then the application-specific process action executes
    And the global action does not execute

  Scenario: Use application-specific window action
    Given a gesture has a global action
    And the same gesture has a window action in the matching application profile
    When the gesture is performed in that application
    Then the application-specific window action executes
    And the global action does not execute

  Scenario: Fall back to a global generic action
    Given no matching application profile defines an action for the recognized gesture
    And a global action exists for that gesture
    When the gesture is performed
    Then the global action executes

  Scenario: Execute no action when no mapping exists
    Given no application-specific action exists for the recognized gesture
    And no global action exists for the recognized gesture
    When the gesture is performed
    Then no action executes


Feature: Action type configuration
  As a user
  I want to choose the action performed by a gesture
  So that gestures can be configured without editing files directly

  Scenario Outline: Select an action type
    Given the user is editing a gesture action
    When the user selects "<action_type>"
    Then the configuration interface displays settings relevant to "<action_type>"

    Examples:
      | action_type       |
      | Keyboard Shortcut |
      | Launch Program    |
      | Open URL          |
      | Mouse             |
      | Window            |
      | Media             |
      | Volume            |
      | Virtual Desktop   |


Feature: Keyboard action configuration
  As a user
  I want to configure keyboard shortcuts
  So that gestures can send key combinations

  Scenario: Configure a keyboard shortcut
    Given the user is configuring a Keyboard Shortcut action
    When the user enters a valid shortcut
    And saves the action
    Then the shortcut is assigned to the gesture


Feature: Process action configuration
  As a user
  I want to configure executable launch actions
  So that gestures can start applications

  Scenario: Configure an executable path
    Given the user is configuring a Launch Program action
    When the user selects an executable
    Then the executable path is stored in the action definition

  Scenario: Configure process arguments
    Given the user is configuring a Launch Program action
    When the user enters command-line arguments
    Then the arguments are stored in the action definition

  Scenario: Configure a working directory
    Given the user is configuring a Launch Program action
    When the user selects a working directory
    Then the working directory is stored in the action definition

  Scenario: Reject process action without executable
    Given the user is configuring a Launch Program action
    And no executable path is configured
    When the user attempts to save the action
    Then the action is rejected as invalid


Feature: URL action configuration
  As a user
  I want to configure URL or URI actions
  So that gestures can open registered destinations

  Scenario: Configure a URL
    Given the user is configuring an Open URL action
    When the user enters a valid URL or URI
    And saves the action
    Then the URL or URI is assigned to the gesture

  Scenario: Reject URL action without a URI
    Given the user is configuring an Open URL action
    And the URI field is empty
    When the user attempts to save the action
    Then the action is rejected as invalid


Feature: Mouse action configuration
  As a user
  I want to configure mouse actions
  So that gestures can generate pointer input

  Scenario: Select a mouse operation
    Given the user is configuring a Mouse action
    When the user selects a supported mouse operation
    Then that operation is stored in the action definition

  Scenario: Select a mouse button
    Given the selected mouse operation requires a button
    When the user selects a supported mouse button
    Then that button is stored in the action definition

  Scenario: Select a contextual position
    Given the selected mouse operation uses a position
    When the user selects a contextual position target
    Then the selected target is stored in the action definition

  Scenario: Configure absolute coordinates
    Given the selected mouse position type is Absolute
    When the user enters X and Y coordinates
    Then the coordinates are stored in the action definition


Feature: Window action configuration
  As a user
  I want to configure window actions
  So that gestures can manipulate application windows

  Scenario: Select a window operation
    Given the user is configuring a Window action
    When the user selects a supported operation
    Then the operation is stored in the action definition

  Scenario: Select a window target
    Given the user is configuring a Window action
    When the user selects a supported window target
    Then the target is stored in the action definition

  Scenario: Configure move coordinates
    Given the selected window operation is Move
    When the user enters X and Y coordinates
    Then the coordinates are stored in the action definition

  Scenario: Configure resize dimensions
    Given the selected window operation is Resize
    When the user enters width and height
    Then the dimensions are stored in the action definition

  Scenario: Configure move-resize parameters
    Given the selected window operation is Move and Resize
    When the user enters X and Y coordinates
    And enters width and height
    Then all four values are stored in the action definition


Feature: Media action configuration
  As a user
  I want to configure media commands
  So that gestures can control playback

  Scenario Outline: Configure a media operation
    Given the user is configuring a Media action
    When the user selects "<operation>"
    And saves the action
    Then "<operation>" is assigned to the gesture

    Examples:
      | operation      |
      | Play/Pause     |
      | Next Track     |
      | Previous Track |
      | Stop           |


Feature: Volume action configuration
  As a user
  I want to configure volume commands
  So that gestures can control system audio

  Scenario Outline: Configure a volume operation
    Given the user is configuring a Volume action
    When the user selects "<operation>"
    And saves the action
    Then "<operation>" is assigned to the gesture

    Examples:
      | operation   |
      | Increase    |
      | Decrease    |
      | Mute Toggle |

  Scenario: Configure volume amount
    Given the selected volume operation supports an amount
    When the user enters a valid amount
    Then the amount is stored in the action definition


Feature: Virtual desktop action configuration
  As a user
  I want to configure virtual desktop actions
  So that gestures can control Windows workspaces

  Scenario: Configure Next Desktop
    Given Next Desktop is supported
    When the user selects Next Desktop
    And saves the action
    Then Next Desktop is assigned to the gesture

  Scenario: Configure Previous Desktop
    Given Previous Desktop is supported
    When the user selects Previous Desktop
    And saves the action
    Then Previous Desktop is assigned to the gesture

  Scenario: Configure Create Desktop
    Given Create Desktop is supported
    When the user selects Create Desktop
    And saves the action
    Then Create Desktop is assigned to the gesture

  Scenario: Configure Close Desktop
    Given Close Desktop is supported
    When the user selects Close Desktop
    And saves the action
    Then Close Desktop is assigned to the gesture

  Scenario: Hide unsupported virtual desktop operations
    Given a virtual desktop operation is not supported
    When the user configures a Virtual Desktop action
    Then the unsupported operation is not presented as available


Feature: Action definition validation
  As a user
  I want invalid action definitions to be rejected
  So that unusable gesture mappings are not saved

  Scenario: Reject unknown action type
    Given an action definition specifies an unknown action type
    When the action is validated
    Then the action is invalid

  Scenario: Reject unknown action operation
    Given a valid action type specifies an unsupported operation
    When the action is validated
    Then the action is invalid

  Scenario: Reject process action without executable
    Given a Process action contains no executable
    When the action is validated
    Then the action is invalid

  Scenario: Reject URL action without URI
    Given a URL action contains no URL or URI
    When the action is validated
    Then the action is invalid

  Scenario: Reject absolute mouse action without coordinates
    Given a Mouse action uses an Absolute position
    And required coordinates are missing
    When the action is validated
    Then the action is invalid

  Scenario: Reject window resize without dimensions
    Given a Window Resize action is missing width or height
    When the action is validated
    Then the action is invalid

  Scenario: Save a valid action
    Given an action definition contains all required parameters
    And all selected values are supported
    When the user saves the action
    Then the action definition is persisted


Feature: Runtime target validation
  As a user
  I want actions to verify runtime-dependent targets
  So that stale configuration does not cause incorrect behavior

  Scenario: Target window disappears before execution
    Given a window action was resolved
    And its target window closes before execution
    When the action executes
    Then the action reports failure
    And no substitute window is manipulated

  Scenario: Executable disappears before execution
    Given a valid process action was previously configured
    And the executable no longer exists
    When the action executes
    Then the action reports failure

  Scenario: Requested runtime capability is unavailable
    Given an action requires a Windows capability that is unavailable
    When the action executes
    Then the action reports that the operation cannot be completed


Feature: Input cleanup after failed actions
  As a user
  I want synthetic input to be released after failures
  So that the mouse or keyboard does not remain logically pressed

  Scenario: Keyboard action fails after modifier down
    Given a keyboard action has injected a modifier-down event
    And the action subsequently fails
    When failure cleanup occurs
    Then the application attempts to release the injected modifier

  Scenario: Mouse action fails after button down
    Given a mouse action has injected a button-down event
    And the action subsequently fails
    When failure cleanup occurs
    Then the application attempts to release the injected mouse button


Feature: Existing Phase 1 action compatibility
  As an existing user
  I want my Phase 1 keyboard mappings to continue working
  So that upgrading does not require recreating them

  Scenario: Load an existing keyboard mapping
    Given a valid Phase 1 keyboard action configuration exists
    When Phase 2 loads the configuration
    Then the existing keyboard mapping remains usable

  Scenario: Execute an existing Phase 1 keyboard mapping
    Given an existing Phase 1 gesture is mapped to a keyboard shortcut
    When the gesture is recognized
    Then the configured keyboard shortcut executes

  Scenario: Preserve existing application-specific precedence
    Given an existing Phase 1 application profile overrides a global keyboard mapping
    When the gesture is performed in that application
    Then the application-specific mapping still takes precedence


Feature: Persist Phase 2 action mappings
  As a user
  I want all new action types to survive application restarts
  So that my Phase 2 configuration remains intact

  Scenario Outline: Persist an action type
    Given a gesture is assigned a valid "<action_type>" action
    When the configuration is saved
    And the application restarts
    Then the gesture remains assigned to the same "<action_type>" action
    And its parameters are restored

    Examples:
      | action_type       |
      | Keyboard Shortcut |
      | Launch Program    |
      | Open URL          |
      | Mouse             |
      | Window            |
      | Media             |
      | Volume            |
      | Virtual Desktop   |


Feature: Action failure recovery
  As a user
  I want failed actions to leave the gesture system usable
  So that one bad mapping does not disable the application

  Scenario: Return to idle after failed action
    Given a recognized gesture resolves to an action
    And that action fails
    When action processing finishes
    Then the gesture engine returns to the Idle state

  Scenario: Execute another gesture after failure
    Given a previous gesture action failed
    And the gesture engine has returned to Idle
    When the user performs another valid gesture
    Then the new gesture is recognized
    And its configured action may execute normally

  Scenario: Preserve gesture input after failure
    Given an action execution fails
    Then gesture input remains enabled unless the user explicitly disables it
