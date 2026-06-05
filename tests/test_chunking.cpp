#include "app/application.hpp"
#include "app/config.hpp"
#include "edit/patch_workflow.hpp"
#include "index/index_store.hpp"
#include "parsing/chunk_extractor.hpp"
#include "session/chat_session.hpp"
#include "support/utils.hpp"
#include "vcs/git_diff.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
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

static fs::path make_temp_repo() {
    const auto unique = "ultracode-index-test-" + std::to_string(std::rand());
    const fs::path dest = fs::temp_directory_path() / unique;
    fs::copy(fixture_dir(), dest, fs::copy_options::recursive);
    return dest;
}

static void test_incremental_indexing() {
    const fs::path repo = make_temp_repo();
    const CommandRequest init_request{"init", {}};
    const CommandRequest index_request{"index", {}};

    assert(run_command(repo, init_request) == 0);
    assert(run_command(repo, index_request) == 0);

    auto state = load_file_index_state(repo);
    auto summary = load_index_run_summary(repo);
    assert(summary.has_value());
    assert(state.size() == 5);
    assert(summary->new_files == 5);
    assert(summary->reused_files == 0);

    assert(run_command(repo, index_request) == 0);
    summary = load_index_run_summary(repo);
    assert(summary.has_value());
    assert(summary->reused_files == 5);
    assert(summary->changed_files == 0);

    const fs::path python_file = repo / "hello.py";
    write_text(python_file, slurp(python_file) + "\n\ndef added_after_index():\n    return True\n");
    assert(run_command(repo, index_request) == 0);
    summary = load_index_run_summary(repo);
    assert(summary.has_value());
    assert(summary->changed_files == 1);
    assert(summary->reused_files == 4);

    fs::remove(repo / "README.md");
    assert(run_command(repo, index_request) == 0);
    summary = load_index_run_summary(repo);
    assert(summary.has_value());
    assert(summary->deleted_files == 1);

    state = load_file_index_state(repo);
    bool found_readme = false;
    for (const auto& record : state) {
        if (record.path == "README.md") {
            found_readme = true;
        }
    }
    assert(!found_readme);

    fs::remove_all(repo);
    std::cout << "PASS: incremental indexing\n";
}

static void test_vector_store_roundtrip() {
    const fs::path repo = make_temp_repo();
    fs::create_directories(repo / ".ultracode");

    std::map<std::string, std::vector<float>> written = {
        {"chunk-a", {1.0f, 2.0f, 3.0f}},
        {"chunk-b", {-0.5f, 0.25f}}
    };
    assert(write_vector_store(repo, written));
    const auto loaded = load_vector_store(repo);
    assert(loaded.size() == written.size());
    assert(loaded.at("chunk-a").size() == 3);
    assert(loaded.at("chunk-a")[1] == 2.0f);
    assert(loaded.at("chunk-b").size() == 2);
    assert(loaded.at("chunk-b")[0] == -0.5f);

    fs::remove_all(repo);
    std::cout << "PASS: vector store roundtrip\n";
}

static void test_chat_session_commands() {
    const fs::path repo = make_temp_repo();
    const CommandRequest init_request{"init", {}};
    assert(run_command(repo, init_request) == 0);

    Config cfg = load_config(repo);
    std::istringstream input("/context\n/model tiny-test-model\n/model\n/clear\n/exit\n");
    std::ostringstream output;
    assert(run_interactive_chat(repo, cfg, input, output) == 0);

    const std::string rendered = output.str();
    assert(rendered.find("Entering chat mode") != std::string::npos);
    assert(rendered.find("Active context") != std::string::npos);
    assert(rendered.find("Session model set to: tiny-test-model") != std::string::npos);
    assert(rendered.find("Current session model: tiny-test-model") != std::string::npos);
    assert(rendered.find("Session cleared.") != std::string::npos);
    assert(rendered.find("Leaving chat mode.") != std::string::npos);

    fs::remove_all(repo);
    std::cout << "PASS: chat session commands\n";
}

static void init_git_repo(const fs::path& repo) {
    run_command_capture("git -C " + shell_quote(repo.string()) + " init 2>/dev/null");
    run_command_capture("git -C " + shell_quote(repo.string()) + " config user.email test@example.com");
    run_command_capture("git -C " + shell_quote(repo.string()) + " config user.name test");
    run_command_capture("git -C " + shell_quote(repo.string()) + " add .");
    run_command_capture("git -C " + shell_quote(repo.string()) + " commit -m init 2>/dev/null");
}

static void test_git_diff_context() {
    const fs::path repo = make_temp_repo();
    init_git_repo(repo);
    write_text(repo / "hello.py", slurp(repo / "hello.py") + "\n# local change\n");

    const auto diff_context = load_repo_diff_context(repo);
    assert(!diff_context.diff_text.empty());
    assert(diff_context.changed_paths.count("hello.py") == 1);

    fs::remove_all(repo);
    std::cout << "PASS: git diff context\n";
}

static void test_patch_apply_reject_workflow() {
    const fs::path repo = fs::temp_directory_path() / ("ultracode-patch-test-" + std::to_string(std::rand()));
    fs::create_directories(repo);
    write_text(repo / "note.txt", "hello\n");
    init_git_repo(repo);

    const std::string diff_text =
        "--- a/note.txt\n"
        "+++ b/note.txt\n"
        "@@ -1 +1 @@\n"
        "-hello\n"
        "+hi\n";
    const auto targets = extract_patch_target_paths(diff_text);
    assert(targets.size() == 1 && targets.front() == "note.txt");

    auto proposal = save_patch_proposal(repo, "change greeting", diff_text, targets);
    std::string error;
    assert(apply_patch_proposal(repo, proposal.id, &error));
    assert(slurp(repo / "note.txt") == "hi\n");
    auto applied = load_patch_proposal(repo, proposal.id);
    assert(applied && applied->status == "applied");

    auto rejected = save_patch_proposal(repo, "reject me", diff_text, targets);
    assert(reject_patch_proposal(repo, rejected.id, &error));
    auto rejected_meta = load_patch_proposal(repo, rejected.id);
    assert(rejected_meta && rejected_meta->status == "rejected");

    auto invalid = save_patch_proposal(repo, "invalid", "not a diff\n", {});
    assert(!apply_patch_proposal(repo, invalid.id, &error));

    write_text(repo / "note.txt", "drifted\n");
    auto drift = save_patch_proposal(repo, "drift", diff_text, targets);
    assert(!apply_patch_proposal(repo, drift.id, &error));

    fs::remove_all(repo);
    std::cout << "PASS: patch apply/reject workflow\n";
}

int main() {
    test_detect_language();
    test_cpp_chunking();
    test_python_chunking();
    test_go_chunking();
    test_ts_chunking();
    test_markdown_chunking();
    test_incremental_indexing();
    test_vector_store_roundtrip();
    test_chat_session_commands();
    test_git_diff_context();
    test_patch_apply_reject_workflow();
    std::cout << "All tests passed.\n";
    return 0;
}
