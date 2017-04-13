cmake_minimum_required(VERSION 2.8.4)

project(libopenglrecorder)
include(GNUInstallDirs)
include(CMakeDependentOption)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_WITH_VPX "Build libopenglrecorder with libvpx encoder" ON)
option(BUILD_RECORDER_WITH_SOUND "Build libopenglrecorder with sound recording support" ON)
CMAKE_DEPENDENT_OPTION(BUILD_PULSE_WO_DL "If pulseaudio in your distro / system is optional, turn this off to load pulse with libdl"
    ON "BUILD_RECORDER_WITH_SOUND;UNIX" OFF)

if (UNIX OR MINGW)
    if (CMAKE_BUILD_TYPE MATCHES Debug)
        add_definitions(-std=gnu++0x -O0)
    else()
        add_definitions(-std=gnu++0x -O3)
    endif()
endif()

find_library(TURBOJPEG_LIBRARIES NAMES turbojpeg libturbojpeg PATHS "${PROJECT_SOURCE_DIR}/dependencies/lib")
find_path(TURBOJPEG_INCLUDEDIR NAMES turbojpeg.h PATHS "${PROJECT_SOURCE_DIR}/dependencies/include")
mark_as_advanced(TURBOJPEG_LIBRARIES TURBOJPEG_INCLUDEDIR)
include_directories(${TURBOJPEG_INCLUDEDIR})

if (UNIX)
    include(FindPkgConfig)
endif()

if (BUILD_RECORDER_WITH_SOUND)
    add_definitions(-DENABLE_REC_SOUND)
    if (BUILD_PULSE_WO_DL)
        add_definitions(-DENABLE_PULSE_WO_DL)
    endif()
    if (UNIX)
        pkg_check_modules(PULSEAUDIO libpulse)
        include_directories(${PULSEAUDIO_INCLUDEDIR})
        mark_as_advanced(PULSEAUDIO_LIBRARIES PULSEAUDIO_INCLUDEDIR)
    endif()
    if (UNIX)
        pkg_check_modules(VORBIS vorbis)
        pkg_check_modules(VORBISENC vorbisenc)
        include_directories(${VORBIS_INCLUDEDIR} ${VORBISENC_INCLUDEDIR})
    else()
        find_path(VORBIS_INCLUDEDIR NAMES vorbis/vorbisfile.h PATHS "${PROJECT_SOURCE_DIR}/dependencies/include")
        find_library(VORBIS_LIBRARIES NAMES vorbis libvorbis PATHS "${PROJECT_SOURCE_DIR}/dependencies/lib")
        find_path(VORBISENC_INCLUDEDIR NAMES vorbis/vorbisenc.h PATHS "${PROJECT_SOURCE_DIR}/dependencies/include")
        find_library(VORBISENC_LIBRARIES NAMES vorbisenc libvorbisenc PATHS "${PROJECT_SOURCE_DIR}/dependencies/lib")
    endif()
    mark_as_advanced(VORBIS_LIBRARIES VORBIS_INCLUDEDIR VORBISENC_LIBRARIES VORBISENC_INCLUDEDIR)
endif()

if (BUILD_WITH_VPX)
    add_definitions(-DENABLE_VPX)
    if (UNIX)
        pkg_check_modules(VPX vpx)
    else()
        find_path(VPX_INCLUDEDIR NAMES vpx/vpx_codec.h PATHS "${PROJECT_SOURCE_DIR}/dependencies/include")
        find_library(VPX_LIBRARIES NAMES libvpx PATHS "${PROJECT_SOURCE_DIR}/dependencies/lib")
    endif()
    include_directories(${VPX_INCLUDEDIR})
    mark_as_advanced(VPX_LIBRARIES VPX_INCLUDEDIR)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libwebm)

set(SOURCES
    audio/pulseaudio_recorder.cpp
    audio/vorbis_encoder.cpp
    audio/wasapi_recorder.cpp
    core/capture_library.cpp
    core/mkv_writer.cpp
    core/recorder.cpp
    libwebm/mkvmuxer/mkvmuxer.cc
    libwebm/mkvmuxer/mkvmuxerutil.cc
    libwebm/mkvmuxer/mkvwriter.cc
    video/mjpeg_writer.cpp
    video/vpx_encoder.cpp
)

if(MSVC)
    list(APPEND SOURCES openglrecorder.def)
endif()

add_library(openglrecorder ${SOURCES})

if (BUILD_RECORDER_WITH_SOUND)
    if (UNIX AND BUILD_PULSE_WO_DL)
        target_link_libraries(openglrecorder ${PULSEAUDIO_LIBRARIES})
    elseif (UNIX)
        target_link_libraries(openglrecorder dl)
    endif()
    target_link_libraries(openglrecorder ${VORBISENC_LIBRARIES} ${VORBIS_LIBRARIES})
endif()

if (BUILD_WITH_VPX)
    target_link_libraries(openglrecorder ${VPX_LIBRARIES})
endif()

target_link_libraries(openglrecorder ${TURBOJPEG_LIBRARIES})
if (UNIX)
    target_link_libraries(openglrecorder pthread)
endif()

if (UNIX)
    set_target_properties(openglrecorder PROPERTIES LINK_FLAGS "-Wl,--no-undefined")
endif()
set_target_properties(openglrecorder PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

set(OGR_HEADERS openglrecorder.h)

if (UNIX)
    install(FILES ${OGR_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(TARGETS openglrecorder LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
