// Copyright 2026 Jose Emilio Camargo Chavez
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "index/repo_scanner.hpp"

#include "parsing/chunk_extractor.hpp"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

bool should_ignore(const fs::path& p, const Config& cfg) {
    for (const auto& part : p) {
        const std::string name = part.string();
        for (const auto& ignored : cfg.ignore_dirs)
            if (name == ignored) return true;
    }
    return false;
}

std::vector<fs::path> scan_repo(const fs::path& root, const Config& cfg) {
    std::vector<fs::path> files;
    for (auto it = fs::recursive_directory_iterator(root);
         it != fs::recursive_directory_iterator(); ++it) {
        const fs::path p = it->path();
        if (should_ignore(p, cfg)) {
            if (it->is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file()) continue;
        if (detect_language(p).empty()) continue;
        std::error_code ec;
        const auto size = fs::file_size(p, ec);
        if (ec || size > 1024 * 1024) continue;
        files.push_back(p);
    }
    return files;
}
