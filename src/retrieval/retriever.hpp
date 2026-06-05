#pragma once

#include "app/config.hpp"
#include "parsing/chunk.hpp"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

// Hybrid vector + lexical retrieval.
std::vector<RankedChunk> retrieve(const std::filesystem::path& root,
                                   const Config& cfg,
                                   const std::string& query,
                                   int top_k,
                                   const std::set<std::string>& preferred_paths = {},
                                   const std::string& required_path = "");
