#pragma once

#include <filesystem>
#include <set>
#include <string>

struct RepoDiffContext {
    std::string diff_text;
    std::set<std::string> changed_paths;
};

RepoDiffContext load_repo_diff_context(const std::filesystem::path& root,
                                       int max_chars = 8000);
