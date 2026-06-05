#pragma once

#include "parsing/chunk.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Returns the language tag for a given file extension, or "" if unsupported.
std::string detect_language(const std::filesystem::path& path);

// ── ChunkExtractor interface ─────────────────────────────────────────────────

// Abstract base for all chunk extractors.
// Each extractor declares which languages it supports and converts raw source
// text into a list of Chunk objects.
class ChunkExtractor {
public:
    virtual ~ChunkExtractor() = default;

    // Returns true if this extractor can handle the given language tag.
    virtual bool supports(const std::string& language) const = 0;

    // Extracts chunks from raw file content.
    // path     – repository-relative path (stored in each Chunk)
    // language – language tag returned by detect_language()
    virtual std::vector<Chunk> extract(const std::string& content,
                                        const std::string& path,
                                        const std::string& language) const = 0;
};

// ── HeuristicExtractor ───────────────────────────────────────────────────────

// Regex/indentation-based extractor used as the universal fallback.
class HeuristicExtractor final : public ChunkExtractor {
public:
    bool supports(const std::string& language) const override;
    std::vector<Chunk> extract(const std::string& content,
                                const std::string& path,
                                const std::string& language) const override;
};

// ── Public entry point ───────────────────────────────────────────────────────

// Tries registered extractors in priority order and returns the first
// non-empty result.  Falls back to HeuristicExtractor if no other extractor
// handles the language.
std::vector<Chunk> extract_chunks(const std::filesystem::path& root,
                                   const std::filesystem::path& file);
