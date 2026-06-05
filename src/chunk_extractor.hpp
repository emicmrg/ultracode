#pragma once

#include "chunk.hpp"

#include <filesystem>
#include <string>
#include <vector>

// Returns the language tag for a given file extension, or "" if unsupported.
std::string detect_language(const std::filesystem::path& path);

// Extracts all chunks from a single file.
std::vector<Chunk> extract_chunks(const std::filesystem::path& root,
                                   const std::filesystem::path& file);
