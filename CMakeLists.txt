cmake_minimum_required(VERSION 3.25)

project(strokes_plus_plus VERSION 0.8.0 LANGUAGES CXX)

option(STROKES_BUILD_TESTS "Build the unit tests" ON)

add_library(strokes_core
    src/actions/action_definition.cpp
    src/actions/action_executor.cpp
    src/actions/action_factory.cpp
    src/actions/action_resolver.cpp
    src/actions/keyboard_action.cpp
    src/actions/keyboard_service.cpp
    src/actions/keyboard_shortcut.cpp
    src/actions/symbolic_resolver.cpp
    src/context/profile_matcher.cpp
    src/config/json.cpp
    src/config/configuration_codec.cpp
    src/config/configuration_store.cpp
    src/engine/gesture_engine.cpp
    src/input/mouse_input_router.cpp
    src/input/gesture_state_machine.cpp
    src/logging/structured_logger.cpp
    src/gestures/normalizer.cpp
    src/gestures/recognizer.cpp
    src/gestures/gesture_repository.cpp
    src/context/profile_repository.cpp
)

target_include_directories(strokes_core PUBLIC src)
target_compile_features(strokes_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(strokes_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(strokes_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_library(strokes_windows
        src/actions/windows_keyboard_input.cpp
        src/actions/windows_audio_service.cpp
        src/actions/windows_media_service.cpp
        src/actions/windows_mouse_service.cpp
        src/actions/windows_process_service.cpp
        src/actions/windows_shell_service.cpp
        src/actions/windows_virtual_desktop_service.cpp
        src/actions/windows_window_service.cpp
        src/context/windows_application_context.cpp
        src/config/windows_app_data.cpp
        src/input/windows_mouse_click.cpp
        src/input/windows_keyboard_hook.cpp
        src/input/windows_mouse_hook.cpp
        src/input/windows_modifier_state.cpp
        src/overlay/windows_gesture_overlay.cpp
        src/tray/windows_tray_icon.cpp
        src/ui/windows_settings_window.cpp
        src/ui/windows_action_editor.cpp
        src/ui/windows_gesture_trainer.cpp
        src/ui/settings_controls.cpp
        src/ui/gesture_editor.cpp
        src/ui/profile_editor.cpp
    )
    target_include_directories(strokes_windows PUBLIC src)
    target_link_libraries(strokes_windows PUBLIC strokes_core user32 shell32 ole32 uuid comdlg32)
    target_compile_features(strokes_windows PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(strokes_windows PRIVATE /W4 /permissive- /EHsc)
    endif()

    add_executable(GestureEngine WIN32
        src/app/main.cpp
        resources/strokes_plus_plus.rc
    )
    target_link_libraries(GestureEngine PRIVATE strokes_windows psapi)
    target_compile_features(GestureEngine PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(GestureEngine PRIVATE /W4 /permissive- /EHsc)
    endif()
endif()

if(STROKES_BUILD_TESTS)
    enable_testing()
    add_executable(strokes_core_tests
        tests/action_definition_tests.cpp
        tests/gesture_tests.cpp
        tests/repository_tests.cpp
        tests/gesture_engine_tests.cpp
        tests/keyboard_action_tests.cpp
        tests/json_tests.cpp
        tests/configuration_tests.cpp
        tests/configuration_store_tests.cpp
        tests/input_queue_tests.cpp
        tests/mouse_input_router_tests.cpp
        tests/logging_tests.cpp
        tests/profile_tests.cpp
        tests/state_machine_tests.cpp
    )
    target_link_libraries(strokes_core_tests PRIVATE strokes_core)
    target_compile_features(strokes_core_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(strokes_core_tests PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(strokes_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME strokes_core_tests COMMAND strokes_core_tests)
    add_executable(strokes_performance_tests
        tests/performance_main.cpp
        tests/performance_tests.cpp
    )
    target_link_libraries(strokes_performance_tests PRIVATE strokes_core)
    target_compile_features(strokes_performance_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(strokes_performance_tests PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(strokes_performance_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME strokes_performance_tests COMMAND strokes_performance_tests)
    set_tests_properties(strokes_performance_tests PROPERTIES LABELS performance)
    if(WIN32)
        add_executable(strokes_windows_smoke_tests
            tests/windows_smoke_tests.cpp
            resources/strokes_plus_plus.rc
        )
        target_link_libraries(strokes_windows_smoke_tests PRIVATE strokes_windows)
        target_compile_features(strokes_windows_smoke_tests PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(strokes_windows_smoke_tests PRIVATE /W4 /permissive- /EHsc)
        endif()
        add_test(NAME strokes_windows_smoke_tests COMMAND strokes_windows_smoke_tests)
        add_test(NAME gesture_engine_idle_test COMMAND GestureEngine --idle-test)
        set_tests_properties(gesture_engine_idle_test PROPERTIES TIMEOUT 10)
    endif()
endif()
