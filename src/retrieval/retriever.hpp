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

// Scores every chunk in the index, populating the hybrid, vector and lexical
// scores. Returned unsorted so the caller can rank by whichever dimension it
// needs. `compare` uses this so the diagnostic reflects the exact scoring the
// retriever applies, rather than a duplicated copy that can drift from it.
std::vector<RankedChunk> score_all_chunks(const std::filesystem::path& root,
                                           const Config& cfg,
                                           const std::string& query,
                                           const std::set<std::string>& preferred_paths = {},
                                           const std::string& required_path = "");
