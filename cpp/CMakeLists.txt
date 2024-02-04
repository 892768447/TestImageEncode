cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DFMT_HEADER_ONLY)

if(CMAKE_HOST_APPLE)
  set(LZ4_INCLUDE_DIR "/opt/homebrew/Cellar/lz4/1.9.4/include")
  set(LZ4_LIBRARY_DIR "/opt/homebrew/Cellar/lz4/1.9.4/lib")
  set(ZSTD_INCLUDE_DIR "/opt/homebrew/Cellar/zstd/1.5.5/include")
  set(ZSTD_LIBRARY_DIR "/opt/homebrew/Cellar/zstd/1.5.5/lib")
  set(JPEG_INCLUDE_DIR "/opt/homebrew/Cellar/jpeg-turbo/3.0.1/include")
  set(JPEG_LIBRARY_DIR "/opt/homebrew/Cellar/jpeg-turbo/3.0.1/lib")
else()
  set(LZ4_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/lz4/include")
  set(LZ4_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/lz4/lib")
  set(ZSTD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/zstd/include")
  set(ZSTD_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/zstd/lib")
  set(JPEG_INCLUDE_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/libjpeg-turbo/include")
  set(JPEG_LIBRARY_DIR
      "${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/libjpeg-turbo/lib")
endif()

# set library
if(CMAKE_HOST_WIN32)
  set(LZ4_LIBRARY "liblz4_static")
  set(ZSTD_LIBRARY "libzstd_static")
  set(JPEG_LIBRARY "turbojpeg-static")
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO /NODEFAULTLIB:liblz4_static.lib")
else()
  set(LZ4_LIBRARY "lz4")
  set(ZSTD_LIBRARY "zstd")
  set(JPEG_LIBRARY "turbojpeg")
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../cpplib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/qoixx/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../cpplib/fmt/include")
include_directories(${LZ4_INCLUDE_DIR} ${JPEG_INCLUDE_DIR} ${ZSTD_INCLUDE_DIR})
link_directories(${LZ4_LIBRARY_DIR} ${JPEG_LIBRARY_DIR} ${ZSTD_LIBRARY_DIR})

add_subdirectory(TestBench)
# add_subdirectory(TestTurboEncode) add_subdirectory(TestQoiEncode)
