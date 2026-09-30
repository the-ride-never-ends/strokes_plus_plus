cmake_minimum_required(VERSION 3.25)

project(strokes_plus_plus VERSION 1.0.0 LANGUAGES C CXX)

option(STROKES_BUILD_TESTS "Build the unit tests" ON)

include(FetchContent)
FetchContent_Declare(lua
    URL https://www.lua.org/ftp/lua-5.4.9.tar.gz
    URL_HASH SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)
file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${lua_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/src/lua.c" "${lua_SOURCE_DIR}/src/luac.c")
# Lua is built as C++ so that LUAI_THROW raises a C++ exception instead of calling
# longjmp. Errors raised by the bindings in src/actions/lua_runtime.cpp unwind C++
# frames that own strings, paths and streams, and longjmp would not destroy them.
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(lua54 STATIC ${LUA_SOURCES})
target_include_directories(lua54 PUBLIC "${lua_SOURCE_DIR}/src")
target_compile_features(lua54 PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(lua54 PRIVATE /EHsc /FS)
endif()

add_library(strokes_core
    src/actions/action_definition.cpp
    src/actions/action_executor.cpp
    src/actions/action_factory.cpp
    src/actions/action_resolver.cpp
    src/actions/keyboard_action.cpp
    src/actions/keyboard_service.cpp
    src/actions/keyboard_shortcut.cpp
    src/actions/lua_runtime.cpp
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
target_link_libraries(strokes_core PUBLIC lua54)
target_compile_features(strokes_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(strokes_core PRIVATE /W4 /permissive- /EHsc /FS)
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
        src/actions/windows_user_feedback_service.cpp
        src/actions/windows_window_service.cpp
        src/context/windows_application_context.cpp
        src/context/windows_shell_surface.cpp
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
        target_compile_options(strokes_windows PRIVATE /W4 /permissive- /EHsc /FS)
    endif()

    add_executable(StrokesPlusPlus WIN32
        src/app/main.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/generated/strokes_plus_plus.rc
    )
    target_link_libraries(StrokesPlusPlus PRIVATE strokes_windows psapi)
    target_compile_features(StrokesPlusPlus PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(StrokesPlusPlus PRIVATE /W4 /permissive- /EHsc /FS)
    endif()

    install(TARGETS StrokesPlusPlus RUNTIME DESTINATION . COMPONENT Application)
    install(FILES LICENSE README.md RELEASING.md THIRD_PARTY_NOTICES.txt
            DESTINATION . COMPONENT Application)
    install(FILES resources/strokes_plus_plus_logo.svg DESTINATION resources
            COMPONENT Application)

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
        target_compile_options(strokes_core_tests PRIVATE /W4 /permissive- /EHsc /FS)
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
        target_compile_options(strokes_performance_tests PRIVATE /W4 /permissive- /EHsc /FS)
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
            target_compile_options(strokes_windows_smoke_tests PRIVATE /W4 /permissive- /EHsc /FS)
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
