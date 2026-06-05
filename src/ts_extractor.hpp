#pragma once

#include "chunk_extractor.hpp"

// Tree-sitter based extractor.
// Supports: cpp (C and C++), go, python, typescript / javascript.
// Falls back gracefully to an empty result when the parser is unavailable,
// allowing the caller to try HeuristicExtractor instead.
class TreeSitterExtractor final : public ChunkExtractor {
public:
    bool supports(const std::string& language) const override;
    std::vector<Chunk> extract(const std::string& content,
                                const std::string& path,
                                const std::string& language) const override;
};
