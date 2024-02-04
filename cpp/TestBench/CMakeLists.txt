project(TestBench)

add_executable(TestBench main.cpp)
target_link_libraries(TestBench ${JPEG_LIBRARY} ${LZ4_LIBRARY} ${ZSTD_LIBRARY})
