Feature: Application startup
  As a user
  I want the mouse gesture application to initialize automatically
  So that gesture recognition is available after launch

  Scenario: Start with an existing configuration
    Given valid configuration files exist
    When the application starts
    Then the application loads the global configuration
    And the application loads gesture definitions
    And the application loads application profiles
    And the application initializes gesture recognition
    And the application initializes global mouse input handling
    And the application initializes the gesture overlay
    And the application creates a system tray icon
    And the gesture engine enters the idle state

  Scenario: Start without configuration files
    Given one or more required configuration files do not exist
    When the application starts
    Then the application creates default configuration data
    And the application continues starting normally

  Scenario: Start with gestures enabled
    Given the saved configuration specifies that gestures are enabled
    When the application starts
    Then gesture recognition is enabled

  Scenario: Start with gestures disabled
    Given the saved configuration specifies that gestures are disabled
    When the application starts
    Then gesture recognition is disabled


Feature: Global mouse input capture
  As a user
  I want gestures to work regardless of the foreground application
  So that I can use the same gesture system throughout Windows

  Scenario: Receive mouse input from a foreground application
    Given gestures are enabled
    And a normal desktop application is foreground
    When the user operates the configured gesture button
    Then the gesture engine receives the relevant mouse input

  Scenario: Pass unrelated mouse input through normally
    Given gestures are enabled
    And no gesture is being performed
    When the user generates mouse input unrelated to gesture activation
    Then the input is delivered normally to the target application


Feature: Gesture activation button
  As a user
  I want to choose which mouse button starts gestures
  So that the gesture system fits my mouse and preferences

  Scenario Outline: Configure a supported activation button
    When the user selects "<button>" as the gesture activation button
    And the configuration is saved
    Then "<button>" becomes the gesture activation button

    Examples:
      | button             |
      | Right Mouse Button |
      | Middle Mouse Button|
      | XButton1           |
      | XButton2           |

  Scenario: Use the default activation button
    Given no custom gesture activation button has been configured
    When the application starts
    Then the Right Mouse Button is used as the gesture activation button


Feature: Preserve normal activation button behavior
  As a user
  I want the gesture button to retain its normal click behavior
  So that using gestures does not remove ordinary mouse functionality

  Scenario: Perform a normal right click
    Given the Right Mouse Button is the gesture activation button
    And gestures are enabled
    When the user presses the Right Mouse Button
    And the pointer does not move beyond the gesture movement threshold
    And the user releases the Right Mouse Button
    Then the foreground application receives a normal right click
    And no gesture is recognized
    And no gesture action is executed

  Scenario: Perform a normal configured-button click
    Given a mouse button is configured as the gesture activation button
    And gestures are enabled
    When the user presses the activation button
    And the pointer does not move beyond the gesture movement threshold
    And the user releases the activation button
    Then the target application receives the normal behavior of that button
    And no gesture action is executed


Feature: Gesture movement threshold
  As a user
  I want small mouse movements to remain clicks
  So that accidental motion does not trigger gestures

  Scenario: Movement remains below threshold
    Given gestures are enabled
    And the user has pressed the activation button
    When the pointer moves less than the configured movement threshold
    Then the engine remains in the pending-button state
    And gesture capture does not begin

  Scenario: Movement exceeds threshold
    Given gestures are enabled
    And the user has pressed the activation button
    When the pointer moves farther than the configured movement threshold
    Then gesture capture begins
    And normal activation-button behavior is suppressed for that interaction

  Scenario: Change the movement threshold
    Given the user changes the gesture movement threshold
    When the configuration is saved
    Then subsequent gesture activation uses the new threshold


Feature: Gesture state management
  As the gesture engine
  I want gesture interactions to follow explicit states
  So that clicks, gestures, cancellations, and actions behave consistently

  Scenario: Begin from idle
    Given the gesture engine is idle
    When the user presses the activation button
    Then the engine enters the ButtonPending state

  Scenario: Convert pending input into a gesture
    Given the engine is in the ButtonPending state
    When pointer movement exceeds the gesture movement threshold
    Then the engine enters the Capturing state

  Scenario: Complete a captured gesture
    Given the engine is in the Capturing state
    When the user releases the activation button
    Then the engine enters the Recognizing state

  Scenario: Execute a recognized gesture
    Given the engine is in the Recognizing state
    And the stroke matches a configured gesture above the recognition threshold
    When an action is resolved for the gesture
    Then the engine enters the Executing state
    And the resolved action is executed
    And the engine returns to the Idle state

  Scenario: Finish an unrecognized gesture
    Given the engine is in the Recognizing state
    And the stroke does not match any gesture above the recognition threshold
    Then no gesture action is executed
    And the engine returns to the Idle state

  Scenario: Cancel a gesture
    Given a gesture interaction is active
    When the gesture is cancelled
    Then the engine enters the Cancelled state
    And no gesture action is executed
    And the engine returns to the Idle state


Feature: Gesture session context
  As the gesture engine
  I want each gesture to retain its interaction context
  So that recognition and action execution use the correct information

  Scenario: Create a gesture session
    Given gestures are enabled
    When the user presses the activation button
    Then a new gesture session is created
    And the session records the activation button
    And the session records the gesture start position
    And the session records the gesture start time
    And the session records the current modifier-key state
    And the session records the foreground window
    And the session records the foreground process

  Scenario: Preserve the original target application
    Given a gesture session has started
    And the foreground application changes before the gesture finishes
    When the gesture is resolved
    Then application matching uses the application context captured when the gesture began


Feature: Stroke point collection
  As the gesture engine
  I want to capture the path drawn by the pointer
  So that the path can be recognized as a gesture

  Scenario: Record points while capturing
    Given the engine is in the Capturing state
    When the pointer moves
    Then qualifying pointer coordinates are added to the current stroke

  Scenario: Do not record stroke points while idle
    Given the engine is in the Idle state
    When the pointer moves
    Then the movement is not added to a gesture stroke

  Scenario: Stop recording after gesture completion
    Given the engine was capturing a gesture
    When the activation button is released
    Then no additional pointer movement is added to that completed stroke


Feature: Stroke point filtering
  As the gesture engine
  I want insignificant pointer movement to be ignored
  So that gesture strokes are not filled with redundant points

  Scenario: Ignore movement below the point-distance threshold
    Given a stroke already contains a previously accepted point
    When the pointer moves less than the minimum configured point distance
    Then the new point is not added to the stroke

  Scenario: Record movement beyond the point-distance threshold
    Given a stroke already contains a previously accepted point
    When the pointer moves at least the minimum configured point distance
    Then the new point is added to the stroke


Feature: Stroke point limit
  As the gesture engine
  I want captured strokes to have a bounded number of points
  So that unusually long gestures remain manageable

  Scenario: Capture a stroke below the maximum point count
    Given the stroke contains fewer points than the configured maximum
    When another qualifying point is captured
    Then the point is accepted

  Scenario: Reach the maximum point count
    Given the stroke has reached the configured maximum point count
    When additional pointer movement occurs
    Then the gesture engine keeps the stroke within the configured maximum
    And the gesture interaction remains functional


Feature: Single-stroke gesture recognition
  As a user
  I want arbitrary drawn shapes to be recognized
  So that gestures are not limited to simple cardinal directions

  Scenario: Recognize a trained gesture
    Given a gesture has at least one saved template
    When the user draws a sufficiently similar single stroke
    Then the recognizer returns that gesture
    And the recognizer returns a similarity score

  Scenario: Recognize an arbitrary shape
    Given a user-defined arbitrary single-stroke shape has been trained
    When the user draws a sufficiently similar shape
    Then the user-defined gesture can be recognized

  Scenario: Reject an unknown stroke
    Given gesture templates exist
    When the user draws a stroke that does not sufficiently match any enabled gesture
    Then the recognition result is NoMatch


Feature: Gesture normalization
  As the gesture engine
  I want equivalent strokes to be normalized before comparison
  So that differences in position and size do not prevent recognition

  Scenario: Recognize the same gesture at a different screen position
    Given a gesture template exists
    When the user draws the same shape at a different location on the screen
    Then the gesture can still be recognized as the same gesture

  Scenario: Recognize the same gesture at a different size
    Given a gesture template exists
    When the user draws the same shape at a moderately different size
    Then the gesture can still be recognized as the same gesture

  Scenario: Preserve gesture orientation
    Given separate rightward and upward gestures exist
    When the user draws a rightward gesture
    Then it is not treated as equivalent to the upward gesture solely because of rotation


Feature: Gesture definitions
  As a user
  I want to manage named gesture definitions
  So that gesture shapes can be reused in action mappings

  Scenario: Create a gesture
    When the user creates a gesture with a unique name
    Then a new gesture definition is created
    And the gesture is assigned a unique identifier

  Scenario: Rename a gesture
    Given a gesture exists
    When the user changes its name
    Then the gesture retains its identity
    And the new name is saved

  Scenario: Delete a gesture
    Given a gesture exists
    When the user deletes the gesture
    Then the gesture definition is removed
    And it is no longer available for recognition

  Scenario: Disable a gesture
    Given a gesture exists
    When the user disables the gesture
    Then its templates are excluded from recognition

  Scenario: Enable a gesture
    Given a disabled gesture exists
    When the user enables the gesture
    Then its templates become available for recognition


Feature: Gesture templates
  As a user
  I want multiple examples of the same gesture to be stored
  So that natural drawing variation can be recognized

  Scenario: Save the first template
    Given a gesture definition exists
    And it has no templates
    When the user records a valid training stroke
    Then the stroke is saved as a template for that gesture

  Scenario: Save an additional template
    Given a gesture already contains a template
    When the user records another valid training stroke
    Then the additional stroke is saved as another template for the same gesture

  Scenario: Match any template belonging to a gesture
    Given a gesture contains multiple templates
    When an input stroke closely matches one of those templates
    Then the recognizer may return that gesture


Feature: Recognition threshold
  As a user
  I want to control how closely strokes must match
  So that I can balance recognition tolerance against false matches

  Scenario: Accept a match above the threshold
    Given a gesture match has a score above the configured recognition threshold
    Then the gesture is considered recognized

  Scenario: Reject a match below the threshold
    Given the best gesture match has a score below the configured recognition threshold
    Then the recognition result is NoMatch
    And no gesture action is executed

  Scenario: Change the recognition threshold
    Given the user changes the recognition threshold
    When the configuration is saved
    Then subsequent recognition uses the new threshold


Feature: Application context detection
  As a user
  I want gestures to respond to the application in which they started
  So that the same gesture can perform different actions in different applications

  Scenario: Capture foreground application information
    Given a gesture begins while an application is foreground
    Then the gesture context records the foreground window handle
    And the gesture context records the process identifier
    And the gesture context records the executable name
    And the gesture context records the window title
    And the gesture context records the window class


Feature: Application profiles
  As a user
  I want application-specific gesture profiles
  So that gesture behavior can differ between applications

  Scenario: Create an application profile
    When the user creates a new application profile
    Then the profile is assigned a unique identifier
    And the profile can contain application matching criteria
    And the profile can contain gesture action mappings

  Scenario: Disable an application profile
    Given an application profile exists
    When the user disables the profile
    Then the profile is excluded from application matching

  Scenario: Enable an application profile
    Given a disabled application profile exists
    When the user enables the profile
    Then the profile becomes eligible for application matching


Feature: Application profile matching
  As a user
  I want profiles to match application properties
  So that gestures can target specific application contexts

  Scenario: Match an application by process name
    Given a profile is configured to match process "chrome.exe"
    And the gesture target process is "chrome.exe"
    When profiles are resolved
    Then the profile matches

  Scenario: Reject a different process name
    Given a profile is configured to match process "chrome.exe"
    And the gesture target process is "notepad.exe"
    When profiles are resolved
    Then the profile does not match based on that criterion

  Scenario: Match an application by exact window title
    Given a profile has an exact window-title criterion
    When the target window title exactly matches the configured value
    Then the title criterion matches

  Scenario: Match an application by contained window-title text
    Given a profile has a window-title Contains criterion
    When the target window title contains the configured text
    Then the title criterion matches

  Scenario: Match an application by window class
    Given a profile contains a window-class criterion
    When the target window class matches the configured class
    Then the class criterion matches

  Scenario: Match an application using a regular expression
    Given a profile contains a regular-expression criterion
    When the target application property matches the expression
    Then that criterion matches

  Scenario: Require configured profile criteria
    Given a profile contains multiple required matching criteria
    When one or more required criteria do not match
    Then the profile does not match


Feature: Gesture action resolution
  As a user
  I want application-specific actions to override global actions
  So that gestures can have sensible defaults and specialized behavior

  Scenario: Resolve an application-specific action
    Given the recognized gesture has an action in the matching application profile
    And the same gesture also has a global action
    When the gesture action is resolved
    Then the application-specific action is selected

  Scenario: Fall back to a global action
    Given no matching application profile defines an action for the recognized gesture
    And a global action exists for that gesture
    When the gesture action is resolved
    Then the global action is selected

  Scenario: Resolve no action
    Given no matching application profile defines an action for the recognized gesture
    And no global action exists for the gesture
    When the gesture action is resolved
    Then no action is executed


Feature: Keyboard shortcut actions
  As a user
  I want gestures to trigger keyboard shortcuts
  So that gestures can control existing applications

  Scenario Outline: Execute a configured keyboard shortcut
    Given a recognized gesture is mapped to "<shortcut>"
    When the gesture action executes
    Then the application sends the "<shortcut>" keyboard input

    Examples:
      | shortcut       |
      | CTRL+W         |
      | CTRL+T         |
      | ALT+LEFT       |
      | WIN+D          |
      | CTRL+SHIFT+TAB |

  Scenario: Release injected modifier keys
    Given an action contains one or more modifier keys
    When the keyboard action finishes
    Then all modifier keys injected by the action are released

  Scenario: Preserve operation after action failure
    Given a keyboard action cannot be executed
    When execution fails
    Then the action reports failure
    And the gesture engine remains operational
    And subsequent gestures can still be performed


Feature: Gesture overlay
  As a user
  I want to see the stroke I am drawing
  So that I have visual feedback during a gesture

  Scenario: Show the overlay when gesture capture begins
    Given gesture overlay rendering is enabled
    When the engine enters the Capturing state
    Then a gesture overlay becomes visible

  Scenario: Draw the captured stroke
    Given the gesture overlay is visible
    When qualifying stroke points are captured
    Then the overlay displays a line following the captured trajectory

  Scenario: Do not steal focus
    Given a gesture overlay is displayed
    Then the overlay does not become the active application
    And the foreground application retains keyboard focus

  Scenario: Allow pointer interaction with underlying applications
    Given the gesture overlay is displayed
    Then the overlay does not become the normal mouse target

  Scenario: Hide the overlay after successful completion
    Given the gesture overlay is visible
    When gesture processing completes
    Then the overlay disappears

  Scenario: Hide the overlay after cancellation
    Given the gesture overlay is visible
    When the gesture is cancelled
    Then the overlay disappears

  Scenario: Hide the overlay after failed recognition
    Given the gesture overlay is visible
    When the stroke does not match a gesture
    Then the overlay disappears

  Scenario: Disable gesture drawing
    Given gesture overlay rendering is disabled
    When the user performs a gesture
    Then no stroke overlay is displayed
    And gesture recognition continues to function

  Scenario: Configure overlay line width
    Given the user changes the overlay line width
    When a later gesture is drawn
    Then the overlay uses the configured line width

  Scenario: Configure overlay opacity
    Given the user changes the overlay opacity
    When a later gesture is drawn
    Then the overlay uses the configured opacity


Feature: System tray controls
  As a user
  I want to control the gesture engine from the Windows system tray
  So that common application functions are readily accessible

  Scenario: Display the tray icon
    When the application is running
    Then a system tray icon is available

  Scenario: Disable gestures from the tray
    Given gestures are enabled
    When the user selects "Disable Gestures" from the tray menu
    Then gesture processing is disabled

  Scenario: Enable gestures from the tray
    Given gestures are disabled
    When the user selects "Enable Gestures" from the tray menu
    Then gesture processing is enabled

  Scenario: Open settings from the tray
    When the user selects "Settings" from the tray menu
    Then the settings interface opens

  Scenario: Exit from the tray
    When the user selects "Exit" from the tray menu
    Then the application shuts down

  Scenario: Indicate enabled state
    Given gestures are enabled
    Then the tray interface indicates that gestures are enabled

  Scenario: Indicate disabled state
    Given gestures are disabled
    Then the tray interface indicates that gestures are disabled


Feature: Gesture engine enable and disable
  As a user
  I want to temporarily disable gestures
  So that I can use my mouse without gesture interception when desired

  Scenario: Use the mouse while gestures are disabled
    Given gestures are disabled
    When the user presses and moves the configured gesture button
    Then the mouse behaves normally
    And no gesture is captured
    And no gesture overlay appears
    And no gesture action executes

  Scenario: Resume gesture processing
    Given gestures are disabled
    When the user enables gestures
    Then subsequent activation-button interactions can begin gestures


Feature: Configuration persistence
  As a user
  I want my settings to persist
  So that I do not have to recreate them after restarting the application

  Scenario: Save global settings
    Given the user changes a global setting
    When the setting is saved
    And the application is restarted
    Then the saved value is restored

  Scenario: Save gesture definitions
    Given the user creates or modifies a gesture
    When the gesture configuration is saved
    And the application is restarted
    Then the gesture definition is restored
    And its training templates are restored

  Scenario: Save application profiles
    Given the user creates or modifies an application profile
    When the profile configuration is saved
    And the application is restarted
    Then the application profile is restored

  Scenario: Save gesture action mappings
    Given the user assigns an action to a gesture
    When the configuration is saved
    And the application is restarted
    Then the action remains assigned to that gesture


Feature: Gesture configuration interface
  As a user
  I want to configure gestures without directly editing configuration files
  So that normal setup can be performed through the application

  Scenario: View configured gestures
    When the user opens gesture configuration
    Then the interface displays the configured gestures

  Scenario: Add a gesture
    When the user chooses to add a gesture
    Then the user can provide a gesture name
    And the user can train the gesture

  Scenario: Rename a gesture
    Given a gesture exists
    When the user renames it through the settings interface
    Then the changed name is persisted

  Scenario: Delete a gesture
    Given a gesture exists
    When the user deletes it through the settings interface
    Then the gesture is removed from configuration

  Scenario: Assign a keyboard action
    Given a gesture exists
    When the user assigns a keyboard shortcut to the gesture
    Then the mapping can be saved

  Scenario: Configure the activation button
    When the user chooses a supported mouse button in settings
    And saves the setting
    Then subsequent gesture activation uses the selected button

  Scenario: Configure recognition sensitivity
    When the user changes recognition sensitivity
    And saves the setting
    Then subsequent gestures use the changed recognition threshold


Feature: Application profile configuration
  As a user
  I want to configure application-specific mappings
  So that gestures can behave differently in different programs

  Scenario: Create an application profile
    When the user chooses to create an application profile
    Then the user can provide a profile name
    And the user can configure application matching criteria

  Scenario: Select a process for a profile
    Given the user is editing an application profile
    When the user selects a target process
    Then the selected process is stored as a matching criterion

  Scenario: Assign an application-specific gesture action
    Given an application profile exists
    And a gesture exists
    When the user assigns an action for that gesture within the profile
    Then the application-specific mapping is saved

  Scenario: Configure a global gesture action
    Given a gesture exists
    When the user assigns an action in the global action configuration
    Then the global mapping is saved


Feature: Gesture training
  As a user
  I want to train gestures by drawing them
  So that the recognizer learns my preferred gesture shapes

  Scenario: Train a new gesture
    Given the user has created a gesture definition
    When the user starts gesture training
    And draws a valid stroke
    And saves the stroke
    Then the stroke becomes a template for that gesture

  Scenario: Add another training sample
    Given a gesture already has a training template
    When the user records and saves another valid stroke
    Then the new sample is added to that gesture

  Scenario: Display a training stroke
    Given gesture training is active
    When the user draws a stroke
    Then the training interface displays the captured stroke

  Scenario: Prevent use of an untrained gesture
    Given a gesture has no valid templates
    Then the gesture is not eligible for recognition


Feature: Gesture cancellation
  As a user
  I want to cancel a gesture before it executes
  So that mistakes do not trigger actions

  Scenario: Cancel a gesture with Escape
    Given a gesture is being captured
    When the user presses Escape
    Then the current gesture is cancelled
    And no action executes
    And the captured stroke is discarded
    And the gesture overlay disappears
    And the engine returns to the Idle state


Feature: Multi-monitor gestures
  As a user
  I want gestures to work across multiple monitors
  So that the gesture engine works throughout my desktop

  Scenario: Draw a gesture entirely on a secondary monitor
    Given the user has multiple monitors
    When the user performs a gesture on a secondary monitor
    Then the complete gesture is captured
    And it can be recognized normally

  Scenario: Draw a gesture across monitor boundaries
    Given the user has multiple monitors
    When a gesture begins on one monitor
    And continues onto another monitor
    Then the complete trajectory is captured as one gesture
    And it can be recognized normally

  Scenario: Handle negative desktop coordinates
    Given a monitor occupies negative Windows virtual-screen coordinates
    When the user performs a gesture on that monitor
    Then the gesture is captured correctly
    And recognition does not depend on the absolute screen coordinates
