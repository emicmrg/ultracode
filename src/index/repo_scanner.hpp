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

#pragma once

#include "app/config.hpp"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

// Returns true if the given path should be excluded from indexing.
bool should_ignore(const std::filesystem::path& p, const Config& cfg);

// Files dropped during the scan. Unsupported languages used to be skipped
// silently, which meant a whole backend could be missing from the index with
// no indication: a Tauri project had every .rs file dropped before Rust
// support existed, and nothing reported it.
struct ScanSkips {
    std::map<std::string, int> unsupported_by_extension;
    int unsupported = 0;
    int too_large = 0;
};

// Returns all indexable files under root, respecting ignore_dirs and size
// limits. Pass `skips` to find out what was left out and why.
std::vector<std::filesystem::path> scan_repo(const std::filesystem::path& root,
                                              const Config& cfg,
                                              ScanSkips* skips = nullptr);
