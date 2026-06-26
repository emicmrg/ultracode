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
#include <vector>

// Returns true if the given path should be excluded from indexing.
bool should_ignore(const std::filesystem::path& p, const Config& cfg);

// Returns all indexable files under root, respecting ignore_dirs and size limits.
std::vector<std::filesystem::path> scan_repo(const std::filesystem::path& root,
                                              const Config& cfg);
