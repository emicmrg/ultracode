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

#include "parsing/chunk_extractor.hpp"

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
