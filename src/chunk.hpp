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
