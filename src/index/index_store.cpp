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

#include "index/index_store.hpp"

#include "app/config.hpp"
#include "parsing/chunk.hpp"
#include "support/utils.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>

namespace fs = std::filesystem;

fs::path chunk_path(const fs::path& root, const std::string& id) {
    return root / ".ultracode" / "chunks" / (id + ".txt");
}

fs::path vector_path(const fs::path& root, const std::string& id) {
    return root / ".ultracode" / "vectors" / (id + ".vec");
}

fs::path vector_store_path(const fs::path& root) {
    return root / ".ultracode" / "vectors.bin";
}

bool save_vector(const fs::path& path, const std::vector<float>& v) {
    std::ostringstream ss;
    ss << std::setprecision(9);
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) ss << ' ';
        ss << v[i];
    }
    ss << '\n';
    return write_text(path, ss.str());
}

std::vector<float> load_vector(const fs::path& path) {
    std::stringstream ss(slurp(path));
    std::vector<float> v;
    float x;
    while (ss >> x) v.push_back(x);
    return v;
}

bool write_vector_store(const fs::path& root,
                        const std::map<std::string, std::vector<float>>& vectors) {
    std::ofstream out(vector_store_path(root), std::ios::binary);
    if (!out) {
        return false;
    }

    const char magic[4] = {'U', 'C', 'V', '1'};
    const uint32_t version = 1;
    const uint32_t count = static_cast<uint32_t>(vectors.size());
    out.write(magic, sizeof(magic));
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (const auto& [chunk_id, values] : vectors) {
        const uint32_t id_len = static_cast<uint32_t>(chunk_id.size());
        const uint32_t dim = static_cast<uint32_t>(values.size());
        out.write(reinterpret_cast<const char*>(&id_len), sizeof(id_len));
        out.write(chunk_id.data(), static_cast<std::streamsize>(chunk_id.size()));
        out.write(reinterpret_cast<const char*>(&dim), sizeof(dim));
        if (!values.empty()) {
            out.write(reinterpret_cast<const char*>(values.data()),
                      static_cast<std::streamsize>(values.size() * sizeof(float)));
        }
    }
    return static_cast<bool>(out);
}

std::map<std::string, std::vector<float>> load_vector_store(const fs::path& root) {
    std::map<std::string, std::vector<float>> vectors;
    std::ifstream in(vector_store_path(root), std::ios::binary);
    if (!in) {
        return vectors;
    }

    char magic[4] = {};
    uint32_t version = 0;
    uint32_t count = 0;
    in.read(magic, sizeof(magic));
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in || std::string(magic, sizeof(magic)) != "UCV1" || version != 1) {
        return {};
    }

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id_len = 0;
        uint32_t dim = 0;
        in.read(reinterpret_cast<char*>(&id_len), sizeof(id_len));
        if (!in) return {};
        std::string chunk_id(id_len, '\0');
        in.read(chunk_id.data(), static_cast<std::streamsize>(id_len));
        in.read(reinterpret_cast<char*>(&dim), sizeof(dim));
        if (!in) return {};
        std::vector<float> values(dim);
        if (dim > 0) {
            in.read(reinterpret_cast<char*>(values.data()),
                    static_cast<std::streamsize>(dim * sizeof(float)));
        }
        if (!in) return {};
        vectors[chunk_id] = std::move(values);
    }
    return vectors;
}

static std::string escape_tsv(std::string s) {
    std::replace(s.begin(), s.end(), '\t', ' ');
    std::replace(s.begin(), s.end(), '\n', ' ');
    return s;
}

static std::vector<std::string> split(const std::string& text, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, delim)) {
        parts.push_back(item);
    }
    return parts;
}

static std::string join_strings(const std::vector<std::string>& values, char delim) {
    std::ostringstream ss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) {
            ss << delim;
        }
        ss << values[i];
    }
    return ss.str();
}

bool write_manifest(const fs::path& root, const std::vector<Chunk>& chunks) {
    std::ostringstream ss;
    ss << "id\tpath\tlanguage\tsymbol\tstart_line\tend_line\thash\n";
    for (const auto& c : chunks) {
        ss << c.id << '\t' << escape_tsv(c.path) << '\t' << c.language << '\t'
           << escape_tsv(c.symbol) << '\t' << c.start_line << '\t' << c.end_line
           << '\t' << c.hash << '\n';
    }
    return write_text(root / ".ultracode" / "chunks.tsv", ss.str());
}

std::vector<Chunk> load_manifest(const fs::path& root) {
    std::vector<Chunk> chunks;
    std::stringstream ss(slurp(root / ".ultracode" / "chunks.tsv"));
    std::string line;
    std::getline(ss, line); // skip header
    while (std::getline(ss, line)) {
        std::vector<std::string> fields;
        std::stringstream ls(line);
        std::string field;
        while (std::getline(ls, field, '\t')) fields.push_back(field);
        if (fields.size() < 7) continue;
        Chunk c;
        c.id         = fields[0];
        c.path       = fields[1];
        c.language   = fields[2];
        c.symbol     = fields[3];
        c.start_line = std::stoi(fields[4]);
        c.end_line   = std::stoi(fields[5]);
        c.hash       = fields[6];
        chunks.push_back(c);
    }
    return chunks;
}

bool write_file_index_state(const fs::path& root,
                            const std::vector<FileIndexRecord>& records) {
    std::ostringstream ss;
    ss << "path\tcontent_hash\tchunk_ids\n";
    for (const auto& record : records) {
        ss << escape_tsv(record.path) << '\t'
           << record.content_hash << '\t'
           << join_strings(record.chunk_ids, ',') << '\n';
    }
    return write_text(root / ".ultracode" / "files.tsv", ss.str());
}

std::vector<FileIndexRecord> load_file_index_state(const fs::path& root) {
    std::vector<FileIndexRecord> records;
    std::stringstream ss(slurp(root / ".ultracode" / "files.tsv"));
    std::string line;
    std::getline(ss, line);
    while (std::getline(ss, line)) {
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> fields;
        std::stringstream ls(line);
        std::string field;
        while (std::getline(ls, field, '\t')) {
            fields.push_back(field);
        }
        if (fields.size() < 3) {
            continue;
        }
        FileIndexRecord record;
        record.path = fields[0];
        record.content_hash = fields[1];
        if (!fields[2].empty()) {
            record.chunk_ids = split(fields[2], ',');
        }
        records.push_back(record);
    }
    return records;
}

bool write_index_run_summary(const fs::path& root,
                             const IndexRunSummary& summary) {
    std::ostringstream ss;
    ss << "scanned_files\tnew_files\tchanged_files\treused_files\tdeleted_files\t"
       << "total_chunks\treused_chunks\tollama_vectors\tfallback_vectors\n";
    ss << summary.scanned_files << '\t'
       << summary.new_files << '\t'
       << summary.changed_files << '\t'
       << summary.reused_files << '\t'
       << summary.deleted_files << '\t'
       << summary.total_chunks << '\t'
       << summary.reused_chunks << '\t'
       << summary.ollama_vectors << '\t'
       << summary.fallback_vectors << '\n';
    return write_text(root / ".ultracode" / "index_stats.tsv", ss.str());
}

std::optional<IndexRunSummary> load_index_run_summary(const fs::path& root) {
    std::stringstream ss(slurp(root / ".ultracode" / "index_stats.tsv"));
    std::string header;
    std::string line;
    if (!std::getline(ss, header) || !std::getline(ss, line)) {
        return std::nullopt;
    }

    std::vector<std::string> fields;
    std::stringstream ls(line);
    std::string field;
    while (std::getline(ls, field, '\t')) {
        fields.push_back(field);
    }
    if (fields.size() < 9) {
        return std::nullopt;
    }

    IndexRunSummary summary;
    summary.scanned_files = std::stoi(fields[0]);
    summary.new_files = std::stoi(fields[1]);
    summary.changed_files = std::stoi(fields[2]);
    summary.reused_files = std::stoi(fields[3]);
    summary.deleted_files = std::stoi(fields[4]);
    summary.total_chunks = std::stoi(fields[5]);
    summary.reused_chunks = std::stoi(fields[6]);
    summary.ollama_vectors = std::stoi(fields[7]);
    summary.fallback_vectors = std::stoi(fields[8]);
    return summary;
}

void remove_chunk_artifacts(const fs::path& root,
                            const std::vector<std::string>& chunk_ids) {
    for (const auto& id : chunk_ids) {
        std::error_code ec;
        fs::remove(chunk_path(root, id), ec);
        fs::remove(vector_path(root, id), ec);
    }
}

void ensure_workspace(const fs::path& root) {
    fs::create_directories(root / ".ultracode" / "chunks");
    fs::create_directories(root / ".ultracode" / "vectors");
    const fs::path cfg = root / ".ultracode" / "config.json";
    if (!fs::exists(cfg)) write_text(cfg, default_config_json());
}
