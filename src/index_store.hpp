#pragma once

#include "chunk.hpp"

#include <filesystem>
#include <string>
#include <vector>

// Paths for persisted chunk content and its embedding vector.
std::filesystem::path chunk_path(const std::filesystem::path& root,
                                  const std::string& id);
std::filesystem::path vector_path(const std::filesystem::path& root,
                                   const std::string& id);

bool save_vector(const std::filesystem::path& path,
                 const std::vector<float>& v);
std::vector<float> load_vector(const std::filesystem::path& path);

bool write_manifest(const std::filesystem::path& root,
                    const std::vector<Chunk>& chunks);
std::vector<Chunk> load_manifest(const std::filesystem::path& root);

// Creates .ultracode/{chunks,vectors}/ and writes a default config.json if absent.
void ensure_workspace(const std::filesystem::path& root);
