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

#include <string>

struct Chunk {
    std::string id;
    std::string path;
    std::string language;
    std::string symbol;
    int start_line = 1;
    int end_line   = 1;
    std::string hash;
};

struct RankedChunk {
    Chunk chunk;
    double score        = 0.0;
    double vector_score = 0.0;
    double lexical_score = 0.0;
    std::string content;
};
