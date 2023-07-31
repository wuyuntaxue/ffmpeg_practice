set(DEMO_NAME "qsv-encoder-h264")

aux_source_directory(${PROJECT_SOURCE_DIR}/${DEMO_NAME}/ SRC_FILES)

add_executable(${DEMO_NAME} ${SRC_FILES})

#链接库
target_link_libraries(${DEMO_NAME} PUBLIC -lavcodec -lavutil -lavformat -lavfilter -lswresample -lswscale)
