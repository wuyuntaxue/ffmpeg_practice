cmake_minimum_required(VERSION 3.10)

project(ffmpeg_practice)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg-n6.0/include)
link_directories(${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg-n6.0/lib)
# link_directories(${PROJECT_SOURCE_DIR}/3rdparty/ffmpeg-n6.0/x86_64)

add_subdirectory(${PROJECT_SOURCE_DIR}/ch0)

add_subdirectory(${PROJECT_SOURCE_DIR}/separate-aac)

add_subdirectory(${PROJECT_SOURCE_DIR}/parser-h264)

add_subdirectory(${PROJECT_SOURCE_DIR}/encoder-h264)

add_subdirectory(${PROJECT_SOURCE_DIR}/vaapi-encoder-h264)

add_subdirectory(${PROJECT_SOURCE_DIR}/qsv-encoder-h264)

add_subdirectory(${PROJECT_SOURCE_DIR}/mux-ch0)

add_subdirectory(${PROJECT_SOURCE_DIR}/mux-ch1)

# add_subdirectory(${PROJECT_SOURCE_DIR}/filter-ch0)