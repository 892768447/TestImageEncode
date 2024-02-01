project(TestQoiEncode)

add_executable(TestQoiEncode main.cpp)
target_link_libraries(TestQoiEncode ${LZ4_LIBRARY})
