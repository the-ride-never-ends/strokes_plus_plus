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
    configure_file(
        resources/strokes_plus_plus.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/strokes_plus_plus.rc
        @ONLY
    )
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
        src/ui/gesture_inventory.cpp
        src/ui/profile_editor.cpp
    )
    target_include_directories(strokes_windows PUBLIC src)
    target_link_libraries(strokes_windows PUBLIC strokes_core user32 shell32 ole32 uuid comdlg32
                                                   comctl32)
    target_compile_features(strokes_windows PUBLIC cxx_std_20)
    if(MSVC)
        target_compile_options(strokes_windows PRIVATE /W4 /permissive- /EHsc)
    endif()

    add_executable(StrokesPlusPlus WIN32
        src/app/main.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/generated/strokes_plus_plus.rc
    )
    target_link_libraries(StrokesPlusPlus PRIVATE strokes_windows psapi)
    target_compile_features(StrokesPlusPlus PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(StrokesPlusPlus PRIVATE /W4 /permissive- /EHsc)
    endif()

    install(TARGETS StrokesPlusPlus RUNTIME DESTINATION . COMPONENT Application)
    install(FILES LICENSE README.md DESTINATION . COMPONENT Application)

    if(MSVC)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT Application)
        include(InstallRequiredSystemLibraries)
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
            ${CMAKE_CURRENT_BINARY_DIR}/generated/strokes_plus_plus.rc
        )
        target_link_libraries(strokes_windows_smoke_tests PRIVATE strokes_windows)
        target_compile_features(strokes_windows_smoke_tests PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(strokes_windows_smoke_tests PRIVATE /W4 /permissive- /EHsc)
        endif()
        add_test(NAME strokes_windows_smoke_tests COMMAND strokes_windows_smoke_tests)
        add_test(NAME strokes_plus_plus_idle_test COMMAND StrokesPlusPlus --idle-test)
        set_tests_properties(strokes_plus_plus_idle_test PROPERTIES TIMEOUT 10)
    endif()
endif()

if(WIN32)
    set(CPACK_PACKAGE_NAME "StrokesPlusPlus")
    set(CPACK_PACKAGE_VENDOR "Strokes++ contributors")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Windows mouse gesture application")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "StrokesPlusPlus")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "StrokesPlusPlus-${PROJECT_VERSION}-win64")
    set(CPACK_PACKAGE_CHECKSUM SHA256)
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_GENERATOR "NSIS64;ZIP")

    set(CPACK_NSIS_DISPLAY_NAME "Strokes++")
    set(CPACK_NSIS_PACKAGE_NAME "Strokes++")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/strokes_plus_plus_logo.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/strokes_plus_plus_logo.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "StrokesPlusPlus.exe")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_PACKAGE_EXECUTABLES "StrokesPlusPlus" "Strokes++")
    set(CPACK_CREATE_DESKTOP_LINKS "StrokesPlusPlus")

    include(CPack)
endif()
