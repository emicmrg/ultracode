#pragma once

#include "app/config.hpp"

#include <filesystem>
#include <vector>

// Returns true if the given path should be excluded from indexing.
bool should_ignore(const std::filesystem::path& p, const Config& cfg);

// Returns all indexable files under root, respecting ignore_dirs and size limits.
std::vector<std::filesystem::path> scan_repo(const std::filesystem::path& root,
                                              const Config& cfg);
