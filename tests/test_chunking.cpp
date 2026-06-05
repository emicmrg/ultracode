#include "../src/chunk_extractor.hpp"
#include "../src/utils.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path fixture_dir() {
    return fs::path(__FILE__).parent_path() / "fixtures" / "sample_repo";
}

static void test_cpp_chunking() {
    const fs::path root = fixture_dir();
    const auto chunks   = extract_chunks(root, root / "hello.cpp");
    assert(!chunks.empty() && "C++ chunking should produce chunks");
    bool found_greeter = false, found_add = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("Greeter") != std::string::npos) found_greeter = true;
        if (c.symbol.find("add")     != std::string::npos) found_add     = true;
    }
    assert(found_greeter && "Should find Greeter class");
    assert(found_add     && "Should find add function");
    std::cout << "PASS: C++ chunking (" << chunks.size() << " chunks)\n";
}

static void test_python_chunking() {
    const fs::path root = fixture_dir();
    const auto chunks   = extract_chunks(root, root / "hello.py");
    assert(!chunks.empty() && "Python chunking should produce chunks");
    bool found_animal = false, found_greet = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("Animal") != std::string::npos) found_animal = true;
        if (c.symbol.find("greet")  != std::string::npos) found_greet  = true;
    }
    assert(found_animal && "Should find Animal class");
    assert(found_greet  && "Should find greet function");
    std::cout << "PASS: Python chunking (" << chunks.size() << " chunks)\n";
}

static void test_go_chunking() {
    const fs::path root = fixture_dir();
    const auto chunks   = extract_chunks(root, root / "main.go");
    assert(!chunks.empty() && "Go chunking should produce chunks");
    bool found_dog = false, found_add = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("Dog") != std::string::npos) found_dog = true;
        if (c.symbol.find("Add") != std::string::npos) found_add = true;
    }
    assert(found_dog && "Should find Dog type");
    assert(found_add && "Should find Add function");
    std::cout << "PASS: Go chunking (" << chunks.size() << " chunks)\n";
}

static void test_ts_chunking() {
    const fs::path root = fixture_dir();
    const auto chunks   = extract_chunks(root, root / "app.ts");
    assert(!chunks.empty() && "TypeScript chunking should produce chunks");
    bool found_circle = false, found_greet = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("Circle") != std::string::npos) found_circle = true;
        if (c.symbol.find("greet")  != std::string::npos) found_greet  = true;
    }
    assert(found_circle && "Should find Circle class");
    assert(found_greet  && "Should find greet function");
    std::cout << "PASS: TypeScript chunking (" << chunks.size() << " chunks)\n";
}

static void test_markdown_chunking() {
    const fs::path root = fixture_dir();
    const auto chunks   = extract_chunks(root, root / "README.md");
    assert(!chunks.empty() && "Markdown chunking should produce chunks");
    bool found_overview = false, found_usage = false;
    for (const auto& c : chunks) {
        if (c.symbol.find("Overview") != std::string::npos) found_overview = true;
        if (c.symbol.find("Usage")    != std::string::npos) found_usage    = true;
    }
    assert(found_overview && "Should find Overview section");
    assert(found_usage    && "Should find Usage section");
    std::cout << "PASS: Markdown chunking (" << chunks.size() << " chunks)\n";
}

static void test_detect_language() {
    assert(detect_language(fs::path("foo.cpp"))  == "cpp");
    assert(detect_language(fs::path("foo.py"))   == "python");
    assert(detect_language(fs::path("foo.go"))   == "go");
    assert(detect_language(fs::path("foo.ts"))   == "typescript");
    assert(detect_language(fs::path("foo.md"))   == "markdown");
    assert(detect_language(fs::path("Makefile")) == "");
    std::cout << "PASS: detect_language\n";
}

int main() {
    test_detect_language();
    test_cpp_chunking();
    test_python_chunking();
    test_go_chunking();
    test_ts_chunking();
    test_markdown_chunking();
    std::cout << "All tests passed.\n";
    return 0;
}
