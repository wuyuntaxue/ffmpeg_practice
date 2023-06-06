cmake_minimum_required(VERSION 3.10)

project(ffmpeg_practice)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg-n6.0/include)
link_directories(${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg-n6.0/lib)

add_subdirectory(${PROJECT_SOURCE_DIR}/ch0)
