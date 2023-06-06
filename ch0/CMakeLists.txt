set(DEMO_NAME "ffmpeg_ch0")

add_executable(${DEMO_NAME} ${PROJECT_SOURCE_DIR}/ch0/start.cpp)

#链接库
target_link_libraries(${DEMO_NAME} PUBLIC -lavutil -lavformat -lavcodec)
