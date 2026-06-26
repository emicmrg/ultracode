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

#include "parsing/chunk.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct FileIndexRecord {
    std::string path;
    std::string content_hash;
    std::vector<std::string> chunk_ids;
};

struct IndexRunSummary {
    int scanned_files = 0;
    int new_files = 0;
    int changed_files = 0;
    int reused_files = 0;
    int deleted_files = 0;
    int total_chunks = 0;
    int reused_chunks = 0;
    int ollama_vectors = 0;
    int fallback_vectors = 0;
};

// Paths for persisted chunk content and its embedding vector.
std::filesystem::path chunk_path(const std::filesystem::path& root,
                                  const std::string& id);
std::filesystem::path vector_path(const std::filesystem::path& root,
                                   const std::string& id);
std::filesystem::path vector_store_path(const std::filesystem::path& root);

bool save_vector(const std::filesystem::path& path,
                 const std::vector<float>& v);
std::vector<float> load_vector(const std::filesystem::path& path);
bool write_vector_store(const std::filesystem::path& root,
                        const std::map<std::string, std::vector<float>>& vectors);
std::map<std::string, std::vector<float>> load_vector_store(const std::filesystem::path& root);

bool write_manifest(const std::filesystem::path& root,
                    const std::vector<Chunk>& chunks);
std::vector<Chunk> load_manifest(const std::filesystem::path& root);

bool write_file_index_state(const std::filesystem::path& root,
                            const std::vector<FileIndexRecord>& records);
std::vector<FileIndexRecord> load_file_index_state(const std::filesystem::path& root);

bool write_index_run_summary(const std::filesystem::path& root,
                             const IndexRunSummary& summary);
std::optional<IndexRunSummary> load_index_run_summary(const std::filesystem::path& root);

void remove_chunk_artifacts(const std::filesystem::path& root,
                            const std::vector<std::string>& chunk_ids);

// Creates .ultracode/{chunks,vectors}/ and writes a default config.json if absent.
void ensure_workspace(const std::filesystem::path& root);
