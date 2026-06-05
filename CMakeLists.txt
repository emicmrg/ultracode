cmake_minimum_required(VERSION 3.20)

project(ultracode_mvp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared library sources (everything except main.cpp)
set(ULTRACODE_LIB_SOURCES
    src/config.cpp
    src/chunk_extractor.cpp
    src/ollama_client.cpp
    src/repo_scanner.cpp
    src/index_store.cpp
    src/retriever.cpp
)

add_executable(ultracode src/main.cpp ${ULTRACODE_LIB_SOURCES})

target_compile_options(ultracode PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -pedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(test_chunking tests/test_chunking.cpp ${ULTRACODE_LIB_SOURCES})
target_compile_options(test_chunking PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -pedantic>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
add_test(NAME test_chunking COMMAND test_chunking)
