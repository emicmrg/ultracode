#include "app/application.hpp"

#include "app/config.hpp"
#include "app/diagnose.hpp"
#include "edit/patch_workflow.hpp"
#include "index/index_store.hpp"
#include "index/repo_scanner.hpp"
#include "llm/ollama_client.hpp"
#include "parsing/chunk_extractor.hpp"
#include "retrieval/retriever.hpp"
#include "session/chat_session.hpp"
#include "support/utils.hpp"
#include "vcs/git_diff.hpp"

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct ChunkEmbeddingRequest {
    Chunk chunk;
    std::string content;
};

struct PatchDebugInfo {
    fs::path dir;
    std::string retrieval_text;
    int context_chars_total = 0;
    int target_file_chars = 0;
    int git_diff_chars = 0;
    int model_output_chars = 0;
    int sanitized_patch_chars = 0;
};

fs::path debug_root_dir(const fs::path& root) {
    return root / ".ultracode" / "debug";
}

PatchDebugInfo create_patch_debug_session(const fs::path& root) {
    fs::create_directories(debug_root_dir(root));
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    const fs::path dir = debug_root_dir(root) / ("patch-" + std::to_string(now));
    fs::create_directories(dir);
    PatchDebugInfo debug;
    debug.dir = dir;
    return debug;
}

void flush_patch_debug(const PatchDebugInfo& debug,
                       const std::string& prompt,
                       const std::string& raw_model_output,
                       const std::string& sanitized_patch,
                       const std::string& validation_text) {
    if (debug.dir.empty()) {
        return;
    }
    write_text(debug.dir / "prompt.txt", prompt);
    write_text(debug.dir / "model_output.txt", raw_model_output);
    write_text(debug.dir / "sanitized_patch.diff", sanitized_patch);
    write_text(debug.dir / "retrieval.txt", debug.retrieval_text);

    std::ostringstream summary;
    summary << "context_chars_total=" << debug.context_chars_total << '\n'
            << "target_file_chars=" << debug.target_file_chars << '\n'
            << "git_diff_chars=" << debug.git_diff_chars << '\n'
            << "model_output_chars=" << debug.model_output_chars << '\n'
            << "sanitized_patch_chars=" << debug.sanitized_patch_chars << '\n'
            << validation_text << '\n';
    write_text(debug.dir / "validation.txt", summary.str());
}

std::string normalize_repo_relative_path(const fs::path& root, const std::string& file_target) {
    if (file_target.empty()) {
        return {};
    }
    fs::path candidate(file_target);
    if (candidate.is_absolute()) {
        std::error_code ec;
        const fs::path relative = fs::relative(candidate, root, ec);
        if (!ec && !relative.empty()) {
            return relative.generic_string();
        }
        return {};
    }
    return candidate.generic_string();
}

bool validate_file_target(const fs::path& root,
                          const std::string& file_target,
                          std::string* normalized_path = nullptr) {
    const std::string rel = normalize_repo_relative_path(root, file_target);
    if (rel.empty()) {
        return false;
    }
    const fs::path full_path = root / rel;
    if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
        return false;
    }
    if (normalized_path) {
        *normalized_path = rel;
    }
    return true;
}

std::string render_file_block(const fs::path& root,
                              const std::string& file_target,
                              int max_context_chars) {
    if (file_target.empty()) {
        return {};
    }
    std::string content = slurp(root / file_target);
    if (content.empty()) {
        return {};
    }
    if (static_cast<int>(content.size()) > max_context_chars) {
        content = content.substr(0, static_cast<size_t>(max_context_chars));
    }
    return "<file path=\"" + file_target + "\">\n" + content + "\n</file>\n\n";
}

std::vector<RankedChunk> retrieve_for_request(const fs::path& root,
                                              const Config& cfg,
                                              const std::string& query,
                                              const std::set<std::string>& preferred_paths,
                                              const std::string& file_target) {
    return retrieve(root, cfg, query, cfg.top_k, preferred_paths, file_target);
}

int cmd_init(const fs::path& root) {
    ensure_workspace(root);
    std::cout << "Initialized .ultracode/\n";
    return 0;
}

int cmd_index(const fs::path& root) {
    ensure_workspace(root);
    const Config cfg = load_config(root);
    const OllamaClient ollama(cfg);
    const auto previous_records = load_file_index_state(root);
    const auto previous_chunks = load_manifest(root);
    const auto previous_vectors = load_vector_store(root);
    std::map<std::string, FileIndexRecord> previous_by_path;
    std::map<std::string, Chunk> previous_chunks_by_id;
    for (const auto& record : previous_records) {
        previous_by_path[record.path] = record;
    }
    for (const auto& chunk : previous_chunks) {
        previous_chunks_by_id[chunk.id] = chunk;
    }

    std::vector<Chunk> all_chunks;
    std::vector<FileIndexRecord> next_records;
    std::map<std::string, std::vector<float>> next_vectors;
    std::set<std::string> seen_paths;
    std::vector<ChunkEmbeddingRequest> pending_embeddings;
    IndexRunSummary summary;

    const auto files = scan_repo(root, cfg);
    summary.scanned_files = static_cast<int>(files.size());
    for (const auto& file : files) {
        const std::string rel = fs::relative(file, root).generic_string();
        seen_paths.insert(rel);
        const std::string file_content = slurp(file);
        const std::string file_hash = fnv1a_hex(file_content);

        auto previous_it = previous_by_path.find(rel);
        if (previous_it != previous_by_path.end() &&
            previous_it->second.content_hash == file_hash) {
            bool can_reuse_vectors = true;
            for (const auto& chunk_id : previous_it->second.chunk_ids) {
                if (!previous_vectors.count(chunk_id)) {
                    can_reuse_vectors = false;
                    break;
                }
            }
            if (!can_reuse_vectors) {
                previous_it = previous_by_path.end();
            }
        }

        if (previous_it != previous_by_path.end() &&
            previous_it->second.content_hash == file_hash) {
            ++summary.reused_files;
            summary.reused_chunks += static_cast<int>(previous_it->second.chunk_ids.size());
            for (const auto& chunk_id : previous_it->second.chunk_ids) {
                auto chunk_it = previous_chunks_by_id.find(chunk_id);
                if (chunk_it != previous_chunks_by_id.end()) {
                    all_chunks.push_back(chunk_it->second);
                }
                auto vector_it = previous_vectors.find(chunk_id);
                if (vector_it != previous_vectors.end()) {
                    next_vectors[chunk_id] = vector_it->second;
                }
            }
            next_records.push_back(previous_it->second);
            continue;
        }

        const bool is_new_file = previous_by_path.find(rel) == previous_by_path.end();
        if (is_new_file) {
            ++summary.new_files;
        } else {
            ++summary.changed_files;
        }

        auto chunks = extract_chunks(root, file);
        std::vector<std::string> new_chunk_ids;
        std::set<std::string> new_chunk_set;
        for (auto& c : chunks) {
            std::string content = join_lines(split_lines(file_content),
                                             c.start_line, c.end_line);
            write_text(chunk_path(root, c.id), content);
            const fs::path vp = vector_path(root, c.id);
            if (!fs::exists(vp)) {
                pending_embeddings.push_back(ChunkEmbeddingRequest{c, content});
            }
            new_chunk_ids.push_back(c.id);
            new_chunk_set.insert(c.id);
            all_chunks.push_back(c);
        }

        if (previous_it != previous_by_path.end()) {
            std::vector<std::string> orphaned_ids;
            for (const auto& old_id : previous_it->second.chunk_ids) {
                if (!new_chunk_set.count(old_id)) {
                    orphaned_ids.push_back(old_id);
                }
            }
            remove_chunk_artifacts(root, orphaned_ids);
        }

        next_records.push_back(FileIndexRecord{rel, file_hash, new_chunk_ids});
    }

    for (const auto& record : previous_records) {
        if (!seen_paths.count(record.path)) {
            ++summary.deleted_files;
            remove_chunk_artifacts(root, record.chunk_ids);
        }
    }

    const int batch_size = std::max(1, cfg.embedding_batch_size);
    for (size_t start = 0; start < pending_embeddings.size(); start += static_cast<size_t>(batch_size)) {
        const size_t end = std::min(pending_embeddings.size(), start + static_cast<size_t>(batch_size));
        std::vector<std::string> inputs;
        inputs.reserve(end - start);
        for (size_t i = start; i < end; ++i) {
            inputs.push_back(pending_embeddings[i].content);
        }

        auto embeddings = ollama.embed_batch(inputs);
        for (size_t i = start; i < end; ++i) {
            const size_t local_index = i - start;
            std::vector<float> values;
            if (local_index < embeddings.size()) {
                values = embeddings[local_index];
            }
            if (!values.empty()) {
                ++summary.ollama_vectors;
            } else {
                values = hashed_embedding(pending_embeddings[i].content, cfg.fallback_embedding_dim);
                ++summary.fallback_vectors;
            }
            next_vectors[pending_embeddings[i].chunk.id] = values;
        }
    }

    write_manifest(root, all_chunks);
    write_file_index_state(root, next_records);
    write_vector_store(root, next_vectors);
    summary.total_chunks = static_cast<int>(all_chunks.size());
    write_index_run_summary(root, summary);

    std::cout << "Indexed " << files.size() << " files and " << all_chunks.size() << " chunks.\n";
    std::cout << "Files: " << summary.new_files << " new, "
              << summary.changed_files << " changed, "
              << summary.reused_files << " reused, "
              << summary.deleted_files << " deleted.\n";

    const int total_vecs = summary.ollama_vectors + summary.fallback_vectors;
    if (total_vecs > 0) {
        const int pct_ollama = (summary.ollama_vectors * 100) / total_vecs;
        const int pct_fallback = (summary.fallback_vectors * 100) / total_vecs;
        std::cout << "Vectors: " << summary.ollama_vectors << " from Ollama (" << pct_ollama << "%), "
                  << summary.fallback_vectors << " local fallback (" << pct_fallback << "%).\n";
        if (pct_fallback > 50) {
            std::cout << "\033[33m  Warning: " << pct_fallback
                      << "% of vectors use local fallback. Ensure Ollama is running and '" << cfg.embedding_model
                      << "' model is pulled for better semantic search.\033[0m\n";
        }
    } else {
        std::cout << "Vectors: " << summary.ollama_vectors << " from Ollama, "
                  << summary.fallback_vectors << " local fallback.\n";
    }
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
    if (const auto summary = load_index_run_summary(root)) {
        std::cout << "Last index run:\n"
                  << "  scanned: " << summary->scanned_files << '\n'
                  << "  new: " << summary->new_files << '\n'
                  << "  changed: " << summary->changed_files << '\n'
                  << "  reused: " << summary->reused_files << '\n'
                  << "  deleted: " << summary->deleted_files << '\n'
                  << "  reused chunks: " << summary->reused_chunks << '\n';
    }
    return 0;
}

int cmd_search(const fs::path& root,
               const std::string& query,
               const std::string& file_target) {
    const Config cfg = load_config(root);
    const auto diff_context = load_repo_diff_context(root, cfg.max_context_chars / 3);
    std::string normalized_file_target;
    if (!file_target.empty() && !validate_file_target(root, file_target, &normalized_file_target)) {
        std::cerr << "Could not read file target: " << file_target << '\n';
        return 1;
    }
    const auto ranked = retrieve_for_request(root, cfg, query, diff_context.changed_paths, normalized_file_target);
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

int cmd_ask(const fs::path& root,
            const std::string& query,
            const std::string& file_target) {
    const Config cfg = load_config(root);
    const auto diff_context = load_repo_diff_context(root, cfg.max_context_chars / 3);
    std::string normalized_file_target;
    if (!file_target.empty() && !validate_file_target(root, file_target, &normalized_file_target)) {
        std::cerr << "Could not read file target: " << file_target << '\n';
        return 1;
    }
    const auto ranked = retrieve_for_request(root, cfg, query, diff_context.changed_paths, normalized_file_target);
    std::ostringstream ctx;
    int used = 0;
    const std::string file_block = render_file_block(root, normalized_file_target, cfg.max_context_chars / 2);
    if (!file_block.empty()) {
        ctx << file_block;
        used += static_cast<int>(file_block.size());
    }
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
        "Question:\n" + query + "\n\nRelevant code context:\n" + ctx.str() +
        (diff_context.diff_text.empty() ? "" : "\nWorking tree diff:\n" + diff_context.diff_text);
    const OllamaClient ollama(cfg);
    std::cout << ollama.chat(system, user) << "\n\nSources:\n";
    for (const auto& r : ranked) {
        std::cout << "- " << r.chunk.path << ":" << r.chunk.start_line
                  << "-" << r.chunk.end_line << " " << r.chunk.symbol << '\n';
    }
    return 0;
}

int cmd_patch(const fs::path& root,
              const std::string& instruction,
              const std::string& file_target,
              bool debug_enabled) {
    ensure_workspace(root);
    const Config cfg = load_config(root);
    const auto diff_context = load_repo_diff_context(root, cfg.max_context_chars / 3);
    PatchDebugInfo debug = debug_enabled ? create_patch_debug_session(root) : PatchDebugInfo{};
    std::string normalized_file_target;
    if (!file_target.empty() && !validate_file_target(root, file_target, &normalized_file_target)) {
        std::cerr << "Could not read file target: " << file_target << '\n';
        return 1;
    }
    const auto ranked = retrieve_for_request(root, cfg, instruction, diff_context.changed_paths, normalized_file_target);

    std::ostringstream ctx;
    int used = 0;
    const std::string file_block = render_file_block(root, normalized_file_target, cfg.max_context_chars / 2);
    if (!file_block.empty()) {
        ctx << file_block;
        used += static_cast<int>(file_block.size());
        debug.target_file_chars = static_cast<int>(file_block.size());
    }
    std::ostringstream retrieval_log;
    for (const auto& r : ranked) {
        retrieval_log << r.chunk.path << ":" << r.chunk.start_line << "-" << r.chunk.end_line
                      << " score=" << std::fixed << std::setprecision(3) << r.score
                      << " vec=" << r.vector_score
                      << " lex=" << r.lexical_score << '\n';
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
    debug.retrieval_text = retrieval_log.str();
    debug.context_chars_total = used;
    debug.git_diff_chars = static_cast<int>(diff_context.diff_text.size());

    const std::string system =
        "You are a local coding assistant. Return only a complete unified diff patch. "
        "Do not include markdown fences, prose, bullets, explanations, or code outside the patch. "
        "Every file change must include diff --git, ---, +++, and @@ hunk headers.";
    std::string user =
        "Requested change:\n" + instruction +
        "\n\nRelevant code context:\n" + ctx.str() +
        "\n\nPatch requirements:\n"
        "- Modify only files that are necessary for the request.\n"
        "- Preserve the existing coding style and schema.\n"
        "- If adding repeated entries, add all requested entries, not a partial subset.\n"
        "- Return only a valid unified diff.\n";
    if (!normalized_file_target.empty()) {
        user += "- The primary file to modify is " + normalized_file_target + ".\n"
                "- Prefer changing only that file unless the request strictly requires more.\n";
    }
    if (!diff_context.diff_text.empty()) {
        user += "\nWorking tree diff:\n" + diff_context.diff_text;
    }
    const std::string full_prompt = "SYSTEM:\n" + system + "\n\nUSER:\n" + user;
    const OllamaClient ollama(cfg);
    const std::string raw_model_output = ollama.chat(system, user);
    const std::string diff_text = sanitize_patch_text(raw_model_output);
    debug.model_output_chars = static_cast<int>(raw_model_output.size());
    debug.sanitized_patch_chars = static_cast<int>(diff_text.size());
    std::string patch_error;
    if (!validate_patch_text(root, diff_text, &patch_error)) {
        flush_patch_debug(
            debug,
            full_prompt,
            raw_model_output,
            diff_text,
            "result=invalid\nerror=" + patch_error);
        std::cerr << "Model returned an invalid patch: " << patch_error
                  << ". Patch proposal was not saved.";
        if (!debug.dir.empty()) {
            std::cerr << " Debug artifacts: " << debug.dir;
        }
        std::cerr << '\n';
        return 1;
    }
    const auto target_paths = extract_patch_target_paths(diff_text);
    const PatchProposal proposal = save_patch_proposal(root, instruction, diff_text, target_paths);
    std::ostringstream target_paths_text;
    for (size_t i = 0; i < target_paths.size(); ++i) {
        if (i) {
            target_paths_text << '\n';
        }
        target_paths_text << target_paths[i];
    }
    flush_patch_debug(
        debug,
        full_prompt,
        raw_model_output,
        diff_text,
        "result=valid\npatch_id=" + proposal.id + "\ntarget_paths=" + target_paths_text.str());

    std::cout << "Saved patch proposal " << proposal.id
              << " targeting " << proposal.target_paths.size() << " file(s).\n";
    for (const auto& path : proposal.target_paths) {
        std::cout << "- " << path << '\n';
    }
    if (!debug.dir.empty()) {
        std::cout << "Debug artifacts: " << debug.dir << '\n';
    }
    return 0;
}

int cmd_apply(const fs::path& root, const std::string& patch_id) {
    std::string error;
    if (!apply_patch_proposal(root, patch_id, &error)) {
        std::cerr << "Failed to apply patch " << patch_id << ": " << error << '\n';
        return 1;
    }
    std::cout << "Applied patch " << patch_id << ".\n";
    return 0;
}

int cmd_reject(const fs::path& root, const std::string& patch_id) {
    std::string error;
    if (!reject_patch_proposal(root, patch_id, &error)) {
        std::cerr << "Failed to reject patch " << patch_id << ": " << error << '\n';
        return 1;
    }
    std::cout << "Rejected patch " << patch_id << ".\n";
    return 0;
}

int cmd_diagnose(const fs::path&, const Config& cfg) {
    const auto result = run_diagnose({}, cfg);
    print_diagnose(result);
    return 0;
}

int cmd_compare(const fs::path& root,
                const std::string& query) {
    const Config cfg = load_config(root);
    const auto diff_context = load_repo_diff_context(root, cfg.max_context_chars / 3);

    const OllamaClient ollama(cfg);
    std::vector<float> qv = ollama.embed(query);
    if (qv.empty()) qv = hashed_embedding(query, cfg.fallback_embedding_dim);

    const auto chunks = load_manifest(root);
    const auto vectors = load_vector_store(root);

    struct ScoredEntry {
        RankedChunk ranked;
        double vec_only;
        double lex_only;
    };

    std::vector<ScoredEntry> entries;
    for (const auto& c : chunks) {
        const std::string content = slurp(chunk_path(root, c.id));
        const auto it = vectors.find(c.id);
        std::vector<float> cv = it != vectors.end() ? it->second : std::vector<float>{};
        double vs = 0.0;
        if (!qv.empty() && !cv.empty()) {
            for (size_t i = 0; i < std::min(qv.size(), cv.size()); ++i) {
                vs += static_cast<double>(qv[i]) * cv[i];
            }
        }
        entries.push_back(ScoredEntry{
            RankedChunk{c, 0.0, vs, 0.0, content}, vs, 0.0});
    }

    for (auto& e : entries) {
        e.lex_only = 0.0;
        const auto q = tokenize(query);
        if (!q.empty()) {
            const std::string hay = lower(e.ranked.chunk.path + " " + e.ranked.chunk.symbol + " " + e.ranked.content);
            double hits = 0.0;
            for (const auto& token : q)
                if (hay.find(token) != std::string::npos) hits += 1.0;
            e.lex_only = hits / static_cast<double>(q.size());
        }
    }

    std::cout << "=== Vector-only ranking ===\n";
    {
        auto by_vec = entries;
        std::sort(by_vec.begin(), by_vec.end(),
            [](const ScoredEntry& a, const ScoredEntry& b) { return a.vec_only > b.vec_only; });
        int shown = 0;
        for (const auto& e : by_vec) {
            if (shown >= 5) break;
            std::cout << shown + 1 << ". " << e.ranked.chunk.path << ":"
                      << e.ranked.chunk.start_line << "-" << e.ranked.chunk.end_line
                      << "  " << e.ranked.chunk.symbol
                      << "  vec=" << std::fixed << std::setprecision(4) << e.vec_only << '\n';
            ++shown;
        }
    }

    std::cout << "\n=== Lexical-only ranking ===\n";
    {
        auto by_lex = entries;
        std::sort(by_lex.begin(), by_lex.end(),
            [](const ScoredEntry& a, const ScoredEntry& b) { return a.lex_only > b.lex_only; });
        int shown = 0;
        for (const auto& e : by_lex) {
            if (shown >= 5) break;
            std::cout << shown + 1 << ". " << e.ranked.chunk.path << ":"
                      << e.ranked.chunk.start_line << "-" << e.ranked.chunk.end_line
                      << "  " << e.ranked.chunk.symbol
                      << "  lex=" << std::fixed << std::setprecision(4) << e.lex_only << '\n';
            ++shown;
        }
    }

    std::cout << "\n=== Hybrid ranking (search command) ===\n";
    {
        const auto ranked = retrieve(root, cfg, query, cfg.top_k, diff_context.changed_paths);
        int shown = 0;
        for (const auto& r : ranked) {
            if (shown >= 5) break;
            std::cout << shown + 1 << ". " << r.chunk.path << ":"
                      << r.chunk.start_line << "-" << r.chunk.end_line
                      << "  " << r.chunk.symbol
                      << "  score=" << std::fixed << std::setprecision(3) << r.score
                      << " vec=" << r.vector_score
                      << " lex=" << r.lexical_score << '\n';
            ++shown;
        }
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
        return cmd_search(root, request.args.front(), request.file_target);
    }
    if (request.name == "ask") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing question.\n";
            return 1;
        }
        return cmd_ask(root, request.args.front(), request.file_target);
    }
    if (request.name == "chat") {
        return run_interactive_chat(root, load_config(root), std::cin, std::cout);
    }
    if (request.name == "patch") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing patch instruction.\n";
            return 1;
        }
        return cmd_patch(root, request.args.front(), request.file_target, request.debug);
    }
    if (request.name == "apply") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing patch id.\n";
            return 1;
        }
        return cmd_apply(root, request.args.front());
    }
    if (request.name == "reject") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing patch id.\n";
            return 1;
        }
        return cmd_reject(root, request.args.front());
    }
    if (request.name == "explain") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing file.\n";
            return 1;
        }
        return cmd_explain(root, request.args.front());
    }
    if (request.name == "diagnose") {
        return cmd_diagnose(root, load_config(root));
    }
    if (request.name == "compare") {
        if (request.args.empty() || request.args.front().empty()) {
            std::cerr << "Missing query.\n";
            return 1;
        }
        return cmd_compare(root, request.args.front());
    }

    std::cerr << "Unknown command: " << request.name << '\n';
    return 1;
}
