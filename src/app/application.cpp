#include "app/application.hpp"

#include "app/config.hpp"
#include "index/index_store.hpp"
#include "index/repo_scanner.hpp"
#include "llm/ollama_client.hpp"
#include "parsing/chunk_extractor.hpp"
#include "retrieval/retriever.hpp"
#include "support/utils.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int cmd_init(const fs::path& root) {
    ensure_workspace(root);
    std::cout << "Initialized .ultracode/\n";
    return 0;
}

int cmd_index(const fs::path& root) {
    ensure_workspace(root);
    const Config cfg = load_config(root);
    const OllamaClient ollama(cfg);
    std::vector<Chunk> all_chunks;
    int ollama_vectors = 0;
    int fallback_vectors = 0;

    const auto files = scan_repo(root, cfg);
    for (const auto& file : files) {
        for (auto& c : extract_chunks(root, file)) {
            std::string content = join_lines(split_lines(slurp(root / c.path)),
                                             c.start_line, c.end_line);
            write_text(chunk_path(root, c.id), content);
            const fs::path vp = vector_path(root, c.id);
            if (!fs::exists(vp)) {
                std::vector<float> v = ollama.embed(content);
                if (!v.empty()) {
                    ++ollama_vectors;
                } else {
                    v = hashed_embedding(content, cfg.fallback_embedding_dim);
                    ++fallback_vectors;
                }
                save_vector(vp, v);
            }
            all_chunks.push_back(c);
        }
    }

    write_manifest(root, all_chunks);
    std::cout << "Indexed " << files.size() << " files and " << all_chunks.size() << " chunks.\n";
    std::cout << "New vectors: " << ollama_vectors << " from Ollama, "
              << fallback_vectors << " local fallback.\n";
    return 0;
}

int cmd_stats(const fs::path& root) {
    const auto chunks = load_manifest(root);
    std::map<std::string, int> by_lang;
    for (const auto& c : chunks) {
        by_lang[c.language]++;
    }

    std::cout << "Chunks: " << chunks.size() << '\n';
    for (const auto& [lang, count] : by_lang) {
        std::cout << "  " << lang << ": " << count << '\n';
    }
    return 0;
}

int cmd_search(const fs::path& root, const std::string& query) {
    const Config cfg = load_config(root);
    const auto ranked = retrieve(root, cfg, query, cfg.top_k);
    for (size_t i = 0; i < ranked.size(); ++i) {
        const auto& r = ranked[i];
        std::cout << i + 1 << ". " << r.chunk.path << ":"
                  << r.chunk.start_line << "-" << r.chunk.end_line
                  << "  " << r.chunk.symbol
                  << "  score=" << std::fixed << std::setprecision(3) << r.score
                  << " vec=" << r.vector_score
                  << " lex=" << r.lexical_score << '\n';
    }
    return 0;
}

int cmd_explain(const fs::path& root, const std::string& file) {
    const Config cfg = load_config(root);
    std::string content = slurp(root / file);
    if (content.empty()) {
        std::cerr << "Could not read file: " << file << '\n';
        return 1;
    }

    if (static_cast<int>(content.size()) > cfg.max_context_chars) {
        content = content.substr(0, static_cast<size_t>(cfg.max_context_chars));
    }

    const OllamaClient ollama(cfg);
    const std::string system =
        "You are a local coding assistant. Be concise and precise. "
        "Explain code with file paths and practical next steps.";
    const std::string user =
        "Explain this file:\n\n<file path=\"" + file + "\">\n" + content + "\n</file>";
    std::cout << ollama.chat(system, user) << '\n';
    return 0;
}

int cmd_ask(const fs::path& root, const std::string& query) {
    const Config cfg = load_config(root);
    const auto ranked = retrieve(root, cfg, query, cfg.top_k);
    std::ostringstream ctx;
    int used = 0;
    for (const auto& r : ranked) {
        std::ostringstream block;
        block << "<chunk path=\"" << r.chunk.path << "\" lines=\""
              << r.chunk.start_line << "-" << r.chunk.end_line
              << "\" symbol=\"" << r.chunk.symbol << "\">\n"
              << r.content << "\n</chunk>\n\n";
        const std::string rendered = block.str();
        if (used + static_cast<int>(rendered.size()) > cfg.max_context_chars) {
            break;
        }
        ctx << rendered;
        used += static_cast<int>(rendered.size());
    }

    const std::string system =
        "You are a local-first coding assistant. Answer using only the provided "
        "code context. If context is insufficient, say what is missing. Always cite "
        "file paths and line ranges.";
    const std::string user =
        "Question:\n" + query + "\n\nRelevant code context:\n" + ctx.str();
    const OllamaClient ollama(cfg);
    std::cout << ollama.chat(system, user) << "\n\nSources:\n";
    for (const auto& r : ranked) {
        std::cout << "- " << r.chunk.path << ":" << r.chunk.start_line
                  << "-" << r.chunk.end_line << " " << r.chunk.symbol << '\n';
    }
    return 0;
}

}  // namespace

int run_command(const fs::path& root, const CommandRequest& request) {
    if (request.name == "init") {
        return cmd_init(root);
    }
    if (request.name == "index") {
        return cmd_index(root);
    }
    if (request.name == "stats") {
        return cmd_stats(root);
    }
    if (request.name == "search") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing query.\n";
            return 1;
        }
        return cmd_search(root, request.args.front());
    }
    if (request.name == "ask") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing question.\n";
            return 1;
        }
        return cmd_ask(root, request.args.front());
    }
    if (request.name == "explain") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing file.\n";
            return 1;
        }
        return cmd_explain(root, request.args.front());
    }

    std::cerr << "Unknown command: " << request.name << '\n';
    return 1;
}
