cmake_minimum_required(VERSION 3.24)

project(SimpleInput VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIMPLEINPUT_BUILD_TESTS "Build core tests" ON)
option(SIMPLEINPUT_BUILD_APP "Build Win32 desktop app" ON)

if(WIN32)
    add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
    message(FATAL_ERROR "SimpleInput currently supports Windows only.")
endif()

add_library(simpleinput_core
    src/core/AutomationEngine.cpp
    src/core/Runners.cpp
    src/core/SettingsStore.cpp
)
target_include_directories(simpleinput_core PUBLIC include)

if(SIMPLEINPUT_BUILD_TESTS)
    add_executable(simpleinput_core_tests
        tests/test_core.cpp
    )
    target_link_libraries(simpleinput_core_tests PRIVATE simpleinput_core)

    add_executable(simpleinput_settings_tests
        tests/test_settings_store.cpp
    )
    target_link_libraries(simpleinput_settings_tests PRIVATE simpleinput_core)

    if(MINGW)
        target_link_options(simpleinput_core_tests PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(simpleinput_settings_tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif()

    enable_testing()
    add_test(NAME simpleinput_core COMMAND simpleinput_core_tests)
    add_test(NAME simpleinput_settings COMMAND simpleinput_settings_tests)
endif()

if(SIMPLEINPUT_BUILD_APP)
    add_executable(SimpleInput WIN32
        include/simpleinput/NativeInput.h
        include/simpleinput/SettingsStore.h
        src/main.cpp
        src/platform/windows/NativeInput.cpp
        src/platform/windows/MainWindow.h
        src/platform/windows/MainWindow.cpp
    )

    target_include_directories(SimpleInput PRIVATE include src)
    if(MINGW)
        target_link_options(SimpleInput PRIVATE -municode -static -static-libgcc -static-libstdc++ -Wl,--subsystem,windows -s)
    endif()
    target_link_libraries(SimpleInput PRIVATE
        simpleinput_core
        user32
        gdi32
        winmm
    )

    install(TARGETS SimpleInput RUNTIME DESTINATION .)
    install(FILES config.json DESTINATION .)

    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "SimpleInput")
    set(CPACK_PACKAGE_VENDOR "SimpleInput")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_FILE_NAME "SimpleInput-${PROJECT_VERSION}-windows")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    include(CPack)
endif()
