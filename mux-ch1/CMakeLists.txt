set(DEMO_NAME "mux-ch1")

aux_source_directory(${PROJECT_SOURCE_DIR}/${DEMO_NAME}/ SRC_FILES)

add_executable(${DEMO_NAME} ${SRC_FILES})

#链接库
target_link_libraries(${DEMO_NAME} PUBLIC -lavutil -lavformat -lavcodec)