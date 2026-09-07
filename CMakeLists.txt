cmake_minimum_required(VERSION 3.21)
project(nvr-monitor VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT WIN32)
    add_compile_definitions(_POSIX_C_SOURCE=200809L)
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SQLite3 REQUIRED)
if(NOT TARGET SQLite3::SQLite3)
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swscale)

add_library(nvr_core
    src/camera.c
    src/camera_manager.c
    src/config.c
    src/database.c
    src/decoder.c
    src/frame_queue.c
    src/layout.c
    src/logger.c
    src/paths.c
    src/renderer.c
    src/rtsp.c
    src/ui.c
)
target_include_directories(nvr_core PUBLIC include)
target_link_libraries(nvr_core PUBLIC SDL2::SDL2 SQLite3::SQLite3 Threads::Threads FFmpeg::FFmpeg)
target_compile_definitions(nvr_core PRIVATE
    NVR_SOURCE_ICON_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/nvr-monitor-window.bmp"
    NVR_INSTALLED_ICON_PATH="${CMAKE_INSTALL_FULL_DATADIR}/nvr-monitor/assets/icons/nvr-monitor-window.bmp")
if(MSVC)
    target_compile_options(nvr_core PRIVATE /W4)
else()
    target_compile_options(nvr_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(nvr-monitor src/main.c src/app.c)
target_link_libraries(nvr-monitor PRIVATE nvr_core)
if(WIN32)
    target_sources(nvr-monitor PRIVATE windows/nvr-monitor.rc)
    target_include_directories(nvr-monitor PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/windows")
    if(TARGET SDL2::SDL2main)
        target_link_libraries(nvr-monitor PRIVATE SDL2::SDL2main)
        target_compile_definitions(nvr-monitor PRIVATE NVR_USE_SDL_MAIN)
        set_target_properties(nvr-monitor PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME app_help COMMAND nvr-monitor --help)
    add_executable(test_layout tests/test_layout.c src/layout.c)
    target_include_directories(test_layout PRIVATE include)
    add_test(NAME layout COMMAND test_layout)

    add_executable(test_rtsp_uri tests/test_rtsp_uri.c src/rtsp.c src/config.c src/logger.c)
    target_compile_definitions(test_rtsp_uri PRIVATE NVR_RTSP_URI_ONLY)
    target_include_directories(test_rtsp_uri PRIVATE include)
    target_link_libraries(test_rtsp_uri PRIVATE Threads::Threads)
    add_test(NAME rtsp_uri COMMAND test_rtsp_uri)

    add_executable(test_config tests/test_config.c src/config.c src/logger.c)
    target_include_directories(test_config PRIVATE include)
    target_link_libraries(test_config PRIVATE Threads::Threads)
    add_test(NAME config COMMAND test_config)

    add_executable(test_database tests/test_database.c src/database.c src/config.c src/logger.c)
    target_include_directories(test_database PRIVATE include)
    target_link_libraries(test_database PRIVATE SQLite3::SQLite3 Threads::Threads)
    add_test(NAME database COMMAND test_database)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(test_decoder tests/test_decoder.c)
        target_link_libraries(test_decoder PRIVATE nvr_core)
        target_link_options(test_decoder PRIVATE -Wl,--wrap=av_hwdevice_ctx_create)
        target_compile_options(test_decoder PRIVATE -UNDEBUG)
        add_test(NAME decoder COMMAND test_decoder)

        add_executable(test_app_idle tests/test_app_idle.c src/app.c)
        target_link_libraries(test_app_idle PRIVATE nvr_core)
        target_link_options(test_app_idle PRIVATE
            -Wl,--wrap=SDL_RenderPresent -Wl,--wrap=SDL_GetWindowFlags)
        target_compile_options(test_app_idle PRIVATE -UNDEBUG)
        add_test(NAME app_idle COMMAND test_app_idle)
        set_tests_properties(app_idle PROPERTIES TIMEOUT 10)
    endif()

    foreach(test_target IN ITEMS test_layout test_rtsp_uri test_config test_database)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -UNDEBUG)
        endif()
    endforeach()
endif()

if(WIN32)
    install(TARGETS nvr-monitor
        RUNTIME_DEPENDENCY_SET nvr_runtime_dependencies
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    # Keep dependency filters before artifact options: PERMISSIONS consumes
    # trailing arguments in this install signature on supported CMake versions.
    install(RUNTIME_DEPENDENCY_SET nvr_runtime_dependencies
        DIRECTORIES "$ENV{MINGW_PREFIX}/bin"
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES ".*[\\/]system32[\\/].*" "kernel32.dll" "user32.dll"
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
else()
    install(TARGETS nvr-monitor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

install(FILES config/cameras.example.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/nvr-monitor/config
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
install(DIRECTORY assets/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/nvr-monitor/assets
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

if(UNIX AND NOT APPLE)
    install(FILES assets/icons/nvr-monitor-256.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
        RENAME nvr-monitor.png
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    install(FILES assets/icons/nvr-monitor-128.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
        RENAME nvr-monitor.png
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    install(FILES desktop/nvr-monitor.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
endif()

configure_file(cmake/uninstall.cmake.in cmake_uninstall.cmake @ONLY)
add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

set(CPACK_PACKAGE_NAME "nvr-monitor")
set(CPACK_PACKAGE_VENDOR "Hangell")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Monitor desktop para câmeras RTSP")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "NVR Monitor")
set(CPACK_PACKAGE_EXECUTABLES "nvr-monitor" "NVR Monitor")
if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "NVR Monitor")
    set(CPACK_NSIS_PACKAGE_NAME "NVR Monitor")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/windows/nvr-monitor.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/windows/nvr-monitor.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\nvr-monitor.exe")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "bin\\\\nvr-monitor.exe")
    set(CPACK_CREATE_DESKTOP_LINKS "nvr-monitor")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
else()
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Hangell")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postinst"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/postrm")
endif()
include(CPack)
