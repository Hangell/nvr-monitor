find_package(PkgConfig QUIET)

set(_ffmpeg_modules)
foreach(_component IN LISTS FFmpeg_FIND_COMPONENTS)
    list(APPEND _ffmpeg_modules "lib${_component}")
endforeach()
if(NOT _ffmpeg_modules)
    set(_ffmpeg_modules libavformat libavcodec libavutil libswscale)
endif()

if(PkgConfig_FOUND)
    pkg_check_modules(PC_FFMPEG QUIET IMPORTED_TARGET ${_ffmpeg_modules})
endif()

if(PC_FFMPEG_FOUND)
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    target_link_libraries(FFmpeg::FFmpeg INTERFACE PkgConfig::PC_FFMPEG)
    set(FFmpeg_FOUND TRUE)
else()
    set(FFmpeg_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg REQUIRED_VARS FFmpeg_FOUND)
