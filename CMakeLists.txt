cmake_minimum_required(VERSION 3.6)
project(bitrate_measure)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -D__LIBAVRESAMPLE__ -std=c99")

set(SOURCE_FILES main.c)
add_executable(bitrate_measure ${SOURCE_FILES})

find_package( FFMPEG       REQUIRED )
if (FFMPEG_FOUND)
    message("Found FFMPEG/LibAV libraries")
    include_directories(${FFMPEG_INCLUDE_DIR})
    target_link_libraries (bitrate_measure ${FFMPEG_LIBRARIES})
else (FFMPEG_FOUND)
    message("Can't find libavcodec, libavformat or libavutil. Add them!")
endif (FFMPEG_FOUND)

target_link_libraries (bitrate_measure "-lm")
target_link_libraries (bitrate_measure "-lavresample")

