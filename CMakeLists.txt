cmake_minimum_required(VERSION 3.20)

project(ultracode_mvp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Tree-sitter (optional) ────────────────────────────────────────────────────
option(ULTRACODE_USE_TREESITTER "Enable Tree-sitter based parsing" OFF)

if(ULTRACODE_USE_TREESITTER)
    include(FetchContent)

    FetchContent_Declare(tree-sitter
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
        GIT_TAG        v0.22.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(tree-sitter-c
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-c.git
        GIT_TAG        v0.21.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(tree-sitter-cpp
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
        GIT_TAG        v0.22.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(tree-sitter-go
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-go.git
        GIT_TAG        v0.21.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(tree-sitter-python
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-python.git
        GIT_TAG        v0.21.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_Declare(tree-sitter-typescript
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-typescript.git
        GIT_TAG        v0.21.2
        GIT_SHALLOW    TRUE
    )

    FetchContent_MakeAvailable(
        tree-sitter
        tree-sitter-c
        tree-sitter-cpp
        tree-sitter-go
        tree-sitter-python
        tree-sitter-typescript
    )

    # Runtime library
    add_library(ts_runtime STATIC ${tree-sitter_SOURCE_DIR}/lib/src/lib.c)
    target_include_directories(ts_runtime PUBLIC ${tree-sitter_SOURCE_DIR}/lib/include)
    set_target_properties(ts_runtime PROPERTIES C_STANDARD 99)

    # C grammar
    add_library(ts_grammar_c STATIC ${tree-sitter-c_SOURCE_DIR}/src/parser.c)
    target_include_directories(ts_grammar_c PRIVATE ${tree-sitter-c_SOURCE_DIR}/src)
    set_target_properties(ts_grammar_c PROPERTIES C_STANDARD 99)

    # C++ grammar (has a C scanner that needs C11 for static_assert)
    add_library(ts_grammar_cpp STATIC
        ${tree-sitter-cpp_SOURCE_DIR}/src/parser.c
        ${tree-sitter-cpp_SOURCE_DIR}/src/scanner.c
    )
    target_include_directories(ts_grammar_cpp PRIVATE ${tree-sitter-cpp_SOURCE_DIR}/src)
    set_target_properties(ts_grammar_cpp PROPERTIES C_STANDARD 11)

    # Go grammar
    add_library(ts_grammar_go STATIC ${tree-sitter-go_SOURCE_DIR}/src/parser.c)
    target_include_directories(ts_grammar_go PRIVATE ${tree-sitter-go_SOURCE_DIR}/src)
    set_target_properties(ts_grammar_go PROPERTIES C_STANDARD 99)

    # Python grammar (has a C scanner)
    add_library(ts_grammar_python STATIC
        ${tree-sitter-python_SOURCE_DIR}/src/parser.c
        ${tree-sitter-python_SOURCE_DIR}/src/scanner.c
    )
    target_include_directories(ts_grammar_python PRIVATE ${tree-sitter-python_SOURCE_DIR}/src)
    set_target_properties(ts_grammar_python PROPERTIES C_STANDARD 99)

    # TypeScript grammar (has a scanner)
    add_library(ts_grammar_typescript STATIC
        ${tree-sitter-typescript_SOURCE_DIR}/typescript/src/parser.c
        ${tree-sitter-typescript_SOURCE_DIR}/typescript/src/scanner.c
    )
    target_include_directories(ts_grammar_typescript
        PRIVATE ${tree-sitter-typescript_SOURCE_DIR}/typescript/src
    )
    set_target_properties(ts_grammar_typescript PROPERTIES C_STANDARD 99)

    set(ULTRACODE_TS_SOURCES src/parsing/ts_extractor.cpp)
    set(ULTRACODE_TS_LIBS
        ts_runtime
        ts_grammar_c
        ts_grammar_cpp
        ts_grammar_go
        ts_grammar_python
        ts_grammar_typescript
    )
    set(ULTRACODE_TS_INCLUDE_DIRS ${tree-sitter_SOURCE_DIR}/lib/include)
endif()

# ── Shared library sources ────────────────────────────────────────────────────
set(ULTRACODE_LIB_SOURCES
    src/app/application.cpp
    src/app/config.cpp
    src/cli/command_router.cpp
    src/index/index_store.cpp
    src/index/repo_scanner.cpp
    src/llm/ollama_client.cpp
    src/parsing/chunk_extractor.cpp
    src/retrieval/retriever.cpp
    src/session/chat_session.cpp
    ${ULTRACODE_TS_SOURCES}
)

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(ultracode src/main.cpp ${ULTRACODE_LIB_SOURCES})
target_include_directories(ultracode PRIVATE src)
target_compile_options(ultracode PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
if(ULTRACODE_USE_TREESITTER)
    target_compile_definitions(ultracode PRIVATE ULTRACODE_USE_TREESITTER)
    target_include_directories(ultracode PRIVATE ${ULTRACODE_TS_INCLUDE_DIRS})
    target_link_libraries(ultracode PRIVATE ${ULTRACODE_TS_LIBS})
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

add_executable(test_chunking tests/test_chunking.cpp ${ULTRACODE_LIB_SOURCES})
target_include_directories(test_chunking PRIVATE src)
target_compile_options(test_chunking PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
if(ULTRACODE_USE_TREESITTER)
    target_compile_definitions(test_chunking PRIVATE ULTRACODE_USE_TREESITTER)
    target_include_directories(test_chunking PRIVATE ${ULTRACODE_TS_INCLUDE_DIRS})
    target_link_libraries(test_chunking PRIVATE ${ULTRACODE_TS_LIBS})
endif()
add_test(NAME test_chunking COMMAND $<TARGET_FILE:test_chunking>)
