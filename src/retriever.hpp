#pragma once

#include "chunk.hpp"
#include "config.hpp"

#include <filesystem>
#include <string>
#include <vector>

// Hybrid vector + lexical retrieval.
std::vector<RankedChunk> retrieve(const std::filesystem::path& root,
                                   const Config& cfg,
                                   const std::string& query,
                                   int top_k);
