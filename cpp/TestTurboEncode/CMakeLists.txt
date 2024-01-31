project(TestTurboEncode)

add_executable(TestTurboEncode main.cpp)
target_link_libraries(TestTurboEncode ${JPEG_LIBRARY} ${LZ4_LIBRARY})
