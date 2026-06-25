cmake_minimum_required(VERSION 3.20)
project(devforge_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(devforge_compression STATIC
    compression/zlib_wrap.cpp
    compression/lz4_wrap.cpp
)

add_library(devforge_crypto STATIC
    crypto/hasher.cpp
    crypto/cipher.cpp
)

add_library(devforge_format STATIC
    format/json.cpp
    format/msgpack.cpp
)

target_include_directories(devforge_compression PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(devforge_crypto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(devforge_format PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(devforge_crypto PUBLIC devforge_compression)
target_link_libraries(devforge_format PUBLIC devforge_crypto devforge_compression)
