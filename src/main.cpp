#include "chunk_extractor.hpp"
#include "config.hpp"
#include "index_store.hpp"
#include "ollama_client.hpp"
#include "repo_scanner.hpp"
#include "retriever.hpp"
#include "utils.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int cmd_init(const fs::path& root) {
    ensure_workspace(root);
    std::cout << "Initialized .ultracode/\n";
    return 0;
}

static int cmd_index(const fs::path& root) {
    ensure_workspace(root);
    const Config cfg = load_config(root);
    const OllamaClient ollama(cfg);
    std::vector<Chunk> all_chunks;
    int ollama_vectors = 0, fallback_vectors = 0;

    const auto files = scan_repo(root, cfg);
    for (const auto& file : files) {
        for (auto& c : extract_chunks(root, file)) {
            std::string content = join_lines(split_lines(slurp(root / c.path)),
                                             c.start_line, c.end_line);
            write_text(chunk_path(root, c.id), content);
            const fs::path vp = vector_path(root, c.id);
            if (!fs::exists(vp)) {
                std::vector<float> v = ollama.embed(content);
                if (!v.empty()) ++ollama_vectors;
                else { v = hashed_embedding(content, cfg.fallback_embedding_dim); ++fallback_vectors; }
                save_vector(vp, v);
            }
            all_chunks.push_back(c);
        }
    }
    write_manifest(root, all_chunks);
    std::cout << "Indexed " << files.size() << " files and " << all_chunks.size() << " chunks.\n";
    std::cout << "New vectors: " << ollama_vectors << " from Ollama, " << fallback_vectors << " local fallback.\n";
    return 0;
}

static int cmd_stats(const fs::path& root) {
    const auto chunks = load_manifest(root);
    std::map<std::string, int> by_lang;
    for (const auto& c : chunks) by_lang[c.language]++;
    std::cout << "Chunks: " << chunks.size() << '\n';
    for (const auto& [lang, count] : by_lang)
        std::cout << "  " << lang << ": " << count << '\n';
    return 0;
}

static int cmd_search(const fs::path& root, const std::string& query) {
    const Config cfg = load_config(root);
    const auto ranked = retrieve(root, cfg, query, cfg.top_k);
    for (size_t i = 0; i < ranked.size(); ++i) {
        const auto& r = ranked[i];
        std::cout << i + 1 << ". " << r.chunk.path << ":"
                  << r.chunk.start_line << "-" << r.chunk.end_line
                  << "  " << r.chunk.symbol
                  << "  score=" << std::fixed << std::setprecision(3) << r.score
                  << " vec=" << r.vector_score << " lex=" << r.lexical_score << '\n';
    }
    return 0;
}

static int cmd_explain(const fs::path& root, const std::string& file) {
    const Config cfg = load_config(root);
    std::string content = slurp(root / file);
    if (content.empty()) { std::cerr << "Could not read file: " << file << '\n'; return 1; }
    if (static_cast<int>(content.size()) > cfg.max_context_chars)
        content = content.substr(0, static_cast<size_t>(cfg.max_context_chars));
    const OllamaClient ollama(cfg);
    const std::string system = "You are a local coding assistant. Be concise and precise. Explain code with file paths and practical next steps.";
    const std::string user   = "Explain this file:\n\n<file path=\"" + file + "\">\n" + content + "\n</file>";
    std::cout << ollama.chat(system, user) << '\n';
    return 0;
}

static int cmd_ask(const fs::path& root, const std::string& query) {
    const Config cfg  = load_config(root);
    const auto ranked = retrieve(root, cfg, query, cfg.top_k);
    std::ostringstream ctx;
    int used = 0;
    for (const auto& r : ranked) {
        std::ostringstream block;
        block << "<chunk path=\"" << r.chunk.path << "\" lines=\""
              << r.chunk.start_line << "-" << r.chunk.end_line
              << "\" symbol=\"" << r.chunk.symbol << "\">\n"
              << r.content << "\n</chunk>\n\n";
        const std::string b = block.str();
        if (used + static_cast<int>(b.size()) > cfg.max_context_chars) break;
        ctx << b;
        used += static_cast<int>(b.size());
    }
    const std::string system = "You are a local-first coding assistant. Answer using only the provided code context. If context is insufficient, say what is missing. Always cite file paths and line ranges.";
    const std::string user   = "Question:\n" + query + "\n\nRelevant code context:\n" + ctx.str();
    const OllamaClient ollama(cfg);
    std::cout << ollama.chat(system, user) << "\n\nSources:\n";
    for (const auto& r : ranked)
        std::cout << "- " << r.chunk.path << ":" << r.chunk.start_line << "-"
                  << r.chunk.end_line << " " << r.chunk.symbol << '\n';
    return 0;
}

static std::string collect_args(int argc, char** argv, int start) {
    std::ostringstream ss;
    for (int i = start; i < argc; ++i) { if (i > start) ss << ' '; ss << argv[i]; }
    return ss.str();
}

static void print_help() {
    std::cout << "ultracode - ultralight local coding assistant MVP\n\n"
              << "Usage:\n"
              << "  ultracode init\n"
              << "  ultracode index\n"
              << "  ultracode stats\n"
              << "  ultracode search <query>\n"
              << "  ultracode ask <question>\n"
              << "  ultracode explain <file>\n";
}

int main(int argc, char** argv) {
    const fs::path root = fs::current_path();
    if (argc < 2) { print_help(); return 0; }
    const std::string cmd = argv[1];
    try {
        if (cmd == "init")    return cmd_init(root);
        if (cmd == "index")   return cmd_index(root);
        if (cmd == "stats")   return cmd_stats(root);
        if (cmd == "search") {
            if (argc < 3) { std::cerr << "Missing query.\n"; return 1; }
            return cmd_search(root, collect_args(argc, argv, 2));
        }
        if (cmd == "ask") {
            if (argc < 3) { std::cerr << "Missing question.\n"; return 1; }
            return cmd_ask(root, collect_args(argc, argv, 2));
        }
        if (cmd == "explain") {
            if (argc < 3) { std::cerr << "Missing file.\n"; return 1; }
            return cmd_explain(root, argv[2]);
        }
        print_help(); return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n'; return 1;
    }
}
