#include "index_store.hpp"
#include "config.hpp"
#include "utils.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

fs::path chunk_path(const fs::path& root, const std::string& id) {
    return root / ".ultracode" / "chunks" / (id + ".txt");
}

fs::path vector_path(const fs::path& root, const std::string& id) {
    return root / ".ultracode" / "vectors" / (id + ".vec");
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

static std::string escape_tsv(std::string s) {
    std::replace(s.begin(), s.end(), '\t', ' ');
    std::replace(s.begin(), s.end(), '\n', ' ');
    return s;
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

void ensure_workspace(const fs::path& root) {
    fs::create_directories(root / ".ultracode" / "chunks");
    fs::create_directories(root / ".ultracode" / "vectors");
    const fs::path cfg = root / ".ultracode" / "config.json";
    if (!fs::exists(cfg)) write_text(cfg, default_config_json());
}
