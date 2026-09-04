cmake_minimum_required(VERSION 3.20)
project(nvr-monitor VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_definitions(_POSIX_C_SOURCE=200809L)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
find_package(SDL2 REQUIRED)
find_package(FFmpeg REQUIRED COMPONENTS avformat avcodec avutil swscale)

add_library(nvr_core
    src/camera.c
    src/camera_manager.c
    src/config.c
    src/decoder.c
    src/frame_queue.c
    src/layout.c
    src/logger.c
    src/renderer.c
    src/rtsp.c
    src/ui.c
)
target_include_directories(nvr_core PUBLIC include)
target_link_libraries(nvr_core PUBLIC SDL2::SDL2 Threads::Threads FFmpeg::FFmpeg)
target_compile_options(nvr_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(nvr-monitor src/main.c src/app.c)
target_link_libraries(nvr-monitor PRIVATE nvr_core)

include(CTest)
if(BUILD_TESTING)
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
endif()

install(TARGETS nvr-monitor
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION share/nvr-monitor/config
)

install(DIRECTORY assets/
    DESTINATION share/nvr-monitor/assets
)

install(FILES assets/icons/nvr-monitor-256.png
    DESTINATION share/icons/hicolor/256x256/apps
    RENAME nvr-monitor.png
)

install(FILES assets/icons/nvr-monitor-128.png
    DESTINATION share/icons/hicolor/128x128/apps
    RENAME nvr-monitor.png
)

install(FILES desktop/nvr-monitor.desktop
    DESTINATION share/applications
)