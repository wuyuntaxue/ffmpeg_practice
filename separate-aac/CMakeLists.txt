set(DEMO_NAME "separate-aac")

add_executable(${DEMO_NAME} ${PROJECT_SOURCE_DIR}/${DEMO_NAME}/start.cpp)

#链接库
target_link_libraries(${DEMO_NAME} PUBLIC -lavutil -lavformat -lavcodec)
