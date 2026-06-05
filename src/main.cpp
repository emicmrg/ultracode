#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

struct Config {
    std::string ollama_url = "http://127.0.0.1:11434";
    std::string embedding_model = "nomic-embed-text";
    std::string chat_model = "qwen2.5-coder:3b";
    int top_k = 8;
    int max_context_chars = 24000;
    int fallback_embedding_dim = 256;
    std::vector<std::string> ignore_dirs = {
        ".git", ".ultracode", "build", "dist", "target", "node_modules",
        ".next", ".venv", "venv", "__pycache__", ".idea", ".vscode"
    };
};

struct Chunk {
    std::string id;
    std::string path;
    std::string language;
    std::string symbol;
    int start_line = 1;
    int end_line = 1;
    std::string hash;
};

struct RankedChunk {
    Chunk chunk;
    double score = 0.0;
    double vector_score = 0.0;
    double lexical_score = 0.0;
    std::string content;
};

static std::string slurp(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool write_text(const fs::path& path, const std::string& content) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << content;
    return true;
}

static std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::string fnv1a_hex(const std::string& input) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : input) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

static std::string join_lines(const std::vector<std::string>& lines, int start_one_based, int end_one_based) {
    std::ostringstream ss;
    const int start = std::max(1, start_one_based);
    const int end = std::min(static_cast<int>(lines.size()), end_one_based);
    for (int i = start; i <= end; ++i) ss << lines[static_cast<size_t>(i - 1)] << '\n';
    return ss.str();
}

static std::string json_escape(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                else out << c;
        }
    }
    return out.str();
}

static std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static std::string run_command_capture(const std::string& cmd) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return {};
    std::array<char, 4096> buffer{};
    std::string result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) result += buffer.data();
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

static std::string unescape_json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) {
            out += s[i];
            continue;
        }
        char n = s[++i];
        switch (n) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case '\\': out += '\\'; break;
            case '"': out += '"'; break;
            default: out += n; break;
        }
    }
    return out;
}

static std::optional<std::string> extract_json_string_after_key(const std::string& json, const std::string& key) {
    const size_t key_pos = json.find(key);
    if (key_pos == std::string::npos) return std::nullopt;
    const size_t colon = json.find(':', key_pos + key.size());
    if (colon == std::string::npos) return std::nullopt;
    const size_t first_quote = json.find('"', colon + 1);
    if (first_quote == std::string::npos) return std::nullopt;

    std::string value;
    bool escaped = false;
    for (size_t i = first_quote + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (escaped) {
            value += '\\';
            value += c;
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') return unescape_json_string(value);
        value += c;
    }
    return std::nullopt;
}

static std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '_') cur += static_cast<char>(std::tolower(c));
        else if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

static std::vector<float> hashed_embedding(const std::string& text, int dim) {
    std::vector<float> v(static_cast<size_t>(dim), 0.0f);
    for (const auto& token : tokenize(text)) {
        const std::string h = fnv1a_hex(token);
        const uint64_t n = std::stoull(h.substr(0, 16), nullptr, 16);
        const size_t idx = static_cast<size_t>(n % static_cast<uint64_t>(dim));
        const float sign = ((n >> 63) & 1u) ? -1.0f : 1.0f;
        v[idx] += sign;
    }
    double norm = 0.0;
    for (float x : v) norm += static_cast<double>(x) * x;
    norm = std::sqrt(norm);
    if (norm > 0.0) for (float& x : v) x = static_cast<float>(x / norm);
    return v;
}

static std::vector<float> parse_first_embedding(const std::string& json) {
    const size_t key = json.find("\"embeddings\"");
    if (key == std::string::npos) return {};
    const size_t outer = json.find('[', key);
    if (outer == std::string::npos) return {};
    const size_t inner = json.find('[', outer + 1);
    if (inner == std::string::npos) return {};
    const size_t end = json.find(']', inner + 1);
    if (end == std::string::npos) return {};

    std::string body = json.substr(inner + 1, end - inner - 1);
    std::vector<float> values;
    std::stringstream ss(body);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (item.empty()) continue;
        try { values.push_back(std::stof(item)); }
        catch (...) { return {}; }
    }
    return values;
}

class OllamaClient {
public:
    explicit OllamaClient(Config config) : config_(std::move(config)) {}

    std::vector<float> embed(const std::string& input) const {
        const std::string payload = "{\"model\":\"" + json_escape(config_.embedding_model) +
            "\",\"input\":\"" + json_escape(input) + "\",\"keep_alive\":\"1m\"}";
        const std::string cmd = "curl -sS --max-time 180 -H 'Content-Type: application/json' " +
            shell_quote(config_.ollama_url + "/api/embed") + " -d " + shell_quote(payload) + " 2>/dev/null";
        return parse_first_embedding(run_command_capture(cmd));
    }

    std::string chat(const std::string& system, const std::string& user) const {
        const std::string payload =
            "{\"model\":\"" + json_escape(config_.chat_model) +
            "\",\"stream\":false,\"keep_alive\":\"10m\",\"messages\":[" +
            "{\"role\":\"system\",\"content\":\"" + json_escape(system) + "\"}," +
            "{\"role\":\"user\",\"content\":\"" + json_escape(user) + "\"}]}";
        const std::string cmd = "curl -sS --max-time 300 -H 'Content-Type: application/json' " +
            shell_quote(config_.ollama_url + "/api/chat") + " -d " + shell_quote(payload) + " 2>/dev/null";
        const std::string response = run_command_capture(cmd);
        auto content = extract_json_string_after_key(response, "\"content\"");
        return content.value_or(response);
    }

private:
    Config config_;
};

static std::string default_config_json() {
    return R"JSON({
  "ollama_url": "http://127.0.0.1:11434",
  "embedding_model": "nomic-embed-text",
  "chat_model": "qwen2.5-coder:3b",
  "top_k": 8,
  "max_context_chars": 24000,
  "fallback_embedding_dim": 256,
  "notes": "This MVP uses Ollama /api/embed and /api/chat. If Ollama is unavailable, indexing falls back to local hashed embeddings."
}
)JSON";
}

static Config load_config(const fs::path& root) {
    Config cfg;
    const std::string text = slurp(root / ".ultracode" / "config.json");
    auto string_value = [&](const std::string& key) -> std::optional<std::string> {
        std::regex r("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
        std::smatch m;
        if (std::regex_search(text, m, r)) return m[1].str();
        return std::nullopt;
    };
    auto int_value = [&](const std::string& key) -> std::optional<int> {
        std::regex r("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
        std::smatch m;
        if (std::regex_search(text, m, r)) return std::stoi(m[1].str());
        return std::nullopt;
    };
    if (auto v = string_value("ollama_url")) cfg.ollama_url = *v;
    if (auto v = string_value("embedding_model")) cfg.embedding_model = *v;
    if (auto v = string_value("chat_model")) cfg.chat_model = *v;
    if (auto v = int_value("top_k")) cfg.top_k = *v;
    if (auto v = int_value("max_context_chars")) cfg.max_context_chars = *v;
    if (auto v = int_value("fallback_embedding_dim")) cfg.fallback_embedding_dim = *v;
    return cfg;
}

static std::string detect_language(const fs::path& path) {
    const std::string ext = lower(path.extension().string());
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c" || ext == ".hpp" || ext == ".h" || ext == ".hh") return "cpp";
    if (ext == ".go") return "go";
    if (ext == ".py") return "python";
    if (ext == ".js" || ext == ".jsx") return "javascript";
    if (ext == ".ts" || ext == ".tsx") return "typescript";
    if (ext == ".md" || ext == ".markdown") return "markdown";
    if (ext == ".json" || ext == ".yaml" || ext == ".yml" || ext == ".toml" || ext == ".mod" || ext == ".sum") return "config";
    return "";
}

static bool should_ignore(const fs::path& p, const Config& cfg) {
    for (const auto& part : p) {
        const std::string name = part.string();
        for (const auto& ignored : cfg.ignore_dirs) if (name == ignored) return true;
    }
    return false;
}

static std::vector<Chunk> chunk_by_lines(const std::vector<std::string>& lines, const std::string& path, const std::string& lang, int max_lines = 120) {
    std::vector<Chunk> chunks;
    for (int start = 1; start <= static_cast<int>(lines.size()); start += max_lines) {
        const int end = std::min(static_cast<int>(lines.size()), start + max_lines - 1);
        const std::string content = join_lines(lines, start, end);
        if (trim(content).empty()) continue;
        Chunk c;
        c.path = path;
        c.language = lang;
        c.symbol = "lines_" + std::to_string(start) + "_" + std::to_string(end);
        c.start_line = start;
        c.end_line = end;
        c.hash = fnv1a_hex(path + content);
        c.id = c.hash;
        chunks.push_back(c);
    }
    return chunks;
}

static int find_brace_block_end(const std::vector<std::string>& lines, int start_line, int max_lines = 400) {
    int depth = 0;
    bool seen_open = false;
    int end = start_line;
    for (int j = start_line; j <= static_cast<int>(lines.size()); ++j) {
        for (char c : lines[static_cast<size_t>(j - 1)]) {
            if (c == '{') { ++depth; seen_open = true; }
            else if (c == '}') --depth;
        }
        end = j;
        if (seen_open && depth <= 0) break;
        if (j - start_line > max_lines) break;
    }
    return end;
}

static std::vector<Chunk> extract_cpp_like(const std::vector<std::string>& lines, const std::string& path, const std::string& lang) {
    std::vector<Chunk> chunks;
    std::regex class_re(R"(^\s*(class|struct|enum)\s+([A-Za-z_][A-Za-z0-9_]*))");
    std::regex func_re(R"(^\s*([A-Za-z_~][A-Za-z0-9_:<>~*&\s]+)\s+([A-Za-z_~][A-Za-z0-9_:~]*)\s*\([^;]*\)\s*(const\s*)?(\{|$))");

    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        const std::string& line = lines[static_cast<size_t>(i - 1)];
        std::smatch m;
        std::string symbol;
        if (std::regex_search(line, m, class_re)) symbol = m[1].str() + " " + m[2].str();
        else if (std::regex_search(line, m, func_re)) symbol = m[2].str() + "()";
        else continue;

        const int start = i;
        const int end = find_brace_block_end(lines, start);
        const std::string content = join_lines(lines, start, end);
        Chunk c;
        c.path = path;
        c.language = lang;
        c.symbol = symbol;
        c.start_line = start;
        c.end_line = end;
        c.hash = fnv1a_hex(path + content);
        c.id = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, lang);
    return chunks;
}

static std::vector<Chunk> extract_go(const std::vector<std::string>& lines, const std::string& path) {
    std::vector<Chunk> chunks;
    std::regex func_re(R"(^\s*func\s+(?:\([^)]*\)\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\))");
    std::regex type_re(R"(^\s*type\s+([A-Za-z_][A-Za-z0-9_]*)\s+(struct|interface)\s*(\{|$))");

    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        const std::string& line = lines[static_cast<size_t>(i - 1)];
        std::smatch m;
        std::string symbol;
        if (std::regex_search(line, m, func_re)) symbol = "func " + m[1].str() + "()";
        else if (std::regex_search(line, m, type_re)) symbol = "type " + m[1].str() + " " + m[2].str();
        else continue;

        const int start = i;
        const int end = find_brace_block_end(lines, start);
        const std::string content = join_lines(lines, start, end);
        Chunk c;
        c.path = path;
        c.language = "go";
        c.symbol = symbol;
        c.start_line = start;
        c.end_line = end;
        c.hash = fnv1a_hex(path + content);
        c.id = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, "go");
    return chunks;
}

static int indentation(const std::string& line) {
    int n = 0;
    for (char c : line) {
        if (c == ' ') ++n;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

static std::vector<Chunk> extract_python(const std::vector<std::string>& lines, const std::string& path) {
    std::vector<Chunk> chunks;
    std::regex def_re(R"(^\s*(class|def)\s+([A-Za-z_][A-Za-z0-9_]*))");
    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        std::smatch m;
        if (!std::regex_search(lines[static_cast<size_t>(i - 1)], m, def_re)) continue;
        const int start = i;
        const int base_indent = indentation(lines[static_cast<size_t>(i - 1)]);
        int end = static_cast<int>(lines.size());
        for (int j = i + 1; j <= static_cast<int>(lines.size()); ++j) {
            const std::string& line = lines[static_cast<size_t>(j - 1)];
            if (trim(line).empty()) continue;
            if (indentation(line) <= base_indent && std::regex_search(line, def_re)) { end = j - 1; break; }
            if (j - start > 400) { end = j; break; }
        }
        const std::string content = join_lines(lines, start, end);
        Chunk c;
        c.path = path;
        c.language = "python";
        c.symbol = m[1].str() + " " + m[2].str();
        c.start_line = start;
        c.end_line = end;
        c.hash = fnv1a_hex(path + content);
        c.id = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, "python");
    return chunks;
}

static std::vector<Chunk> extract_markdown(const std::vector<std::string>& lines, const std::string& path) {
    std::vector<Chunk> chunks;
    std::regex heading_re(R"(^(#{1,6})\s+(.+)$)");
    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        std::smatch m;
        if (!std::regex_search(lines[static_cast<size_t>(i - 1)], m, heading_re)) continue;
        const int level = static_cast<int>(m[1].str().size());
        int end = static_cast<int>(lines.size());
        for (int j = i + 1; j <= static_cast<int>(lines.size()); ++j) {
            std::smatch hm;
            if (std::regex_search(lines[static_cast<size_t>(j - 1)], hm, heading_re) && static_cast<int>(hm[1].str().size()) <= level) {
                end = j - 1;
                break;
            }
        }
        const std::string content = join_lines(lines, i, end);
        Chunk c;
        c.path = path;
        c.language = "markdown";
        c.symbol = m[2].str();
        c.start_line = i;
        c.end_line = end;
        c.hash = fnv1a_hex(path + content);
        c.id = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, "markdown");
    return chunks;
}

static std::vector<Chunk> extract_chunks(const fs::path& root, const fs::path& file) {
    const std::string rel = fs::relative(file, root).generic_string();
    const std::string lang = detect_language(file);
    const std::string text = slurp(file);
    const auto lines = split_lines(text);
    if (lang == "cpp" || lang == "javascript" || lang == "typescript") return extract_cpp_like(lines, rel, lang);
    if (lang == "go") return extract_go(lines, rel);
    if (lang == "python") return extract_python(lines, rel);
    if (lang == "markdown") return extract_markdown(lines, rel);
    return chunk_by_lines(lines, rel, lang.empty() ? "text" : lang);
}

static fs::path chunk_path(const fs::path& root, const std::string& id) { return root / ".ultracode" / "chunks" / (id + ".txt"); }
static fs::path vector_path(const fs::path& root, const std::string& id) { return root / ".ultracode" / "vectors" / (id + ".vec"); }

static bool save_vector(const fs::path& path, const std::vector<float>& v) {
    std::ostringstream ss;
    ss << std::setprecision(9);
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) ss << ' ';
        ss << v[i];
    }
    ss << '\n';
    return write_text(path, ss.str());
}

static std::vector<float> load_vector(const fs::path& path) {
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

static bool write_manifest(const fs::path& root, const std::vector<Chunk>& chunks) {
    std::ostringstream ss;
    ss << "id\tpath\tlanguage\tsymbol\tstart_line\tend_line\thash\n";
    for (const auto& c : chunks) {
        ss << c.id << '\t' << escape_tsv(c.path) << '\t' << c.language << '\t' << escape_tsv(c.symbol) << '\t'
           << c.start_line << '\t' << c.end_line << '\t' << c.hash << '\n';
    }
    return write_text(root / ".ultracode" / "chunks.tsv", ss.str());
}

static std::vector<Chunk> load_manifest(const fs::path& root) {
    std::vector<Chunk> chunks;
    std::stringstream ss(slurp(root / ".ultracode" / "chunks.tsv"));
    std::string line;
    std::getline(ss, line);
    while (std::getline(ss, line)) {
        std::vector<std::string> fields;
        std::stringstream ls(line);
        std::string field;
        while (std::getline(ls, field, '\t')) fields.push_back(field);
        if (fields.size() < 7) continue;
        Chunk c;
        c.id = fields[0];
        c.path = fields[1];
        c.language = fields[2];
        c.symbol = fields[3];
        c.start_line = std::stoi(fields[4]);
        c.end_line = std::stoi(fields[5]);
        c.hash = fields[6];
        chunks.push_back(c);
    }
    return chunks;
}

static double dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) sum += static_cast<double>(a[i]) * b[i];
    return sum;
}

static double lexical_score(const std::string& query, const Chunk& c, const std::string& content) {
    const auto q = tokenize(query);
    if (q.empty()) return 0.0;
    const std::string hay = lower(c.path + " " + c.symbol + " " + content);
    double hits = 0.0;
    for (const auto& token : q) if (hay.find(token) != std::string::npos) hits += 1.0;
    return hits / static_cast<double>(q.size());
}

static std::vector<RankedChunk> retrieve(const fs::path& root, const Config& cfg, const std::string& query, int top_k) {
    const auto chunks = load_manifest(root);
    const OllamaClient ollama(cfg);
    std::vector<float> qv = ollama.embed(query);
    if (qv.empty()) qv = hashed_embedding(query, cfg.fallback_embedding_dim);

    std::vector<RankedChunk> ranked;
    for (const auto& c : chunks) {
        const std::string content = slurp(chunk_path(root, c.id));
        const auto cv = load_vector(vector_path(root, c.id));
        const double vs = dot_product(qv, cv);
        const double ls = lexical_score(query, c, content);
        const double score = 0.62 * vs + 0.38 * ls;
        ranked.push_back({c, score, vs, ls, content});
    }
    std::sort(ranked.begin(), ranked.end(), [](const RankedChunk& a, const RankedChunk& b) { return a.score > b.score; });
    if (static_cast<int>(ranked.size()) > top_k) ranked.resize(static_cast<size_t>(top_k));
    return ranked;
}

static void ensure_workspace(const fs::path& root) {
    fs::create_directories(root / ".ultracode" / "chunks");
    fs::create_directories(root / ".ultracode" / "vectors");
    const fs::path cfg = root / ".ultracode" / "config.json";
    if (!fs::exists(cfg)) write_text(cfg, default_config_json());
}

static int cmd_init(const fs::path& root) {
    ensure_workspace(root);
    std::cout << "Initialized .ultracode/\n";
    return 0;
}

static int cmd_index(const fs::path& root) {
    ensure_workspace(root);
    const Config cfg = load_config(root);
    const OllamaClient ollama(cfg);
    std::vector<Chunk> all_chunks;
    int files = 0;
    int ollama_vectors = 0;
    int fallback_vectors = 0;

    for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it) {
        const fs::path p = it->path();
        if (should_ignore(p, cfg)) {
            if (it->is_directory()) it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file()) continue;
        if (detect_language(p).empty()) continue;
        std::error_code ec;
        const auto size = fs::file_size(p, ec);
        if (ec || size > 1024 * 1024) continue;
        ++files;
        auto chunks = extract_chunks(root, p);
        for (auto& c : chunks) {
            std::string content = slurp(root / c.path);
            content = join_lines(split_lines(content), c.start_line, c.end_line);
            const fs::path cp = chunk_path(root, c.id);
            const fs::path vp = vector_path(root, c.id);
            write_text(cp, content);
            if (!fs::exists(vp)) {
                std::vector<float> v = ollama.embed(content);
                if (!v.empty()) ++ollama_vectors;
                else {
                    v = hashed_embedding(content, cfg.fallback_embedding_dim);
                    ++fallback_vectors;
                }
                save_vector(vp, v);
            }
            all_chunks.push_back(c);
        }
    }
    write_manifest(root, all_chunks);
    std::cout << "Indexed " << files << " files and " << all_chunks.size() << " chunks.\n";
    std::cout << "New vectors: " << ollama_vectors << " from Ollama, " << fallback_vectors << " local fallback.\n";
    return 0;
}

static int cmd_stats(const fs::path& root) {
    const auto chunks = load_manifest(root);
    std::map<std::string, int> by_lang;
    for (const auto& c : chunks) by_lang[c.language]++;
    std::cout << "Chunks: " << chunks.size() << '\n';
    for (const auto& [lang, count] : by_lang) std::cout << "  " << lang << ": " << count << '\n';
    return 0;
}

static int cmd_search(const fs::path& root, const std::string& query) {
    const Config cfg = load_config(root);
    const auto ranked = retrieve(root, cfg, query, cfg.top_k);
    for (size_t i = 0; i < ranked.size(); ++i) {
        const auto& r = ranked[i];
        std::cout << i + 1 << ". " << r.chunk.path << ":" << r.chunk.start_line << "-" << r.chunk.end_line
                  << "  " << r.chunk.symbol << "  score=" << std::fixed << std::setprecision(3) << r.score
                  << " vec=" << r.vector_score << " lex=" << r.lexical_score << '\n';
    }
    return 0;
}

static int cmd_explain(const fs::path& root, const std::string& file) {
    const Config cfg = load_config(root);
    std::string content = slurp(root / file);
    if (content.empty()) {
        std::cerr << "Could not read file: " << file << '\n';
        return 1;
    }
    if (static_cast<int>(content.size()) > cfg.max_context_chars) content = content.substr(0, static_cast<size_t>(cfg.max_context_chars));
    const OllamaClient ollama(cfg);
    const std::string system = "You are a local coding assistant. Be concise and precise. Explain code with file paths and practical next steps.";
    const std::string user = "Explain this file:\n\n<file path=\"" + file + "\">\n" + content + "\n</file>";
    std::cout << ollama.chat(system, user) << '\n';
    return 0;
}

static int cmd_ask(const fs::path& root, const std::string& query) {
    const Config cfg = load_config(root);
    const auto ranked = retrieve(root, cfg, query, cfg.top_k);
    std::ostringstream ctx;
    int used = 0;
    for (const auto& r : ranked) {
        std::ostringstream block;
        block << "<chunk path=\"" << r.chunk.path << "\" lines=\"" << r.chunk.start_line << "-" << r.chunk.end_line
              << "\" symbol=\"" << r.chunk.symbol << "\">\n" << r.content << "\n</chunk>\n\n";
        const std::string b = block.str();
        if (used + static_cast<int>(b.size()) > cfg.max_context_chars) break;
        ctx << b;
        used += static_cast<int>(b.size());
    }
    const std::string system = "You are a local-first coding assistant. Answer using only the provided code context. If context is insufficient, say what is missing. Always cite file paths and line ranges.";
    const std::string user = "Question:\n" + query + "\n\nRelevant code context:\n" + ctx.str();
    const OllamaClient ollama(cfg);
    std::cout << ollama.chat(system, user) << "\n\nSources:\n";
    for (const auto& r : ranked) std::cout << "- " << r.chunk.path << ":" << r.chunk.start_line << "-" << r.chunk.end_line << " " << r.chunk.symbol << '\n';
    return 0;
}

static std::string collect_args(int argc, char** argv, int start) {
    std::ostringstream ss;
    for (int i = start; i < argc; ++i) {
        if (i > start) ss << ' ';
        ss << argv[i];
    }
    return ss.str();
}

static void print_help() {
    std::cout << "ultracode - ultralight local coding assistant MVP\n\n"
              << "Usage:\n"
              << "  ultracode init\n"
              << "  ultracode index\n"
              << "  ultracode stats\n"
              << "  ultracode search <query>\n"
              << "  ultracode ask <question>\n"
              << "  ultracode explain <file>\n";
}

int main(int argc, char** argv) {
    const fs::path root = fs::current_path();
    if (argc < 2) {
        print_help();
        return 0;
    }

    const std::string cmd = argv[1];
    try {
        if (cmd == "init") return cmd_init(root);
        if (cmd == "index") return cmd_index(root);
        if (cmd == "stats") return cmd_stats(root);
        if (cmd == "search") {
            if (argc < 3) { std::cerr << "Missing query.\n"; return 1; }
            return cmd_search(root, collect_args(argc, argv, 2));
        }
        if (cmd == "ask") {
            if (argc < 3) { std::cerr << "Missing question.\n"; return 1; }
            return cmd_ask(root, collect_args(argc, argv, 2));
        }
        if (cmd == "explain") {
            if (argc < 3) { std::cerr << "Missing file.\n"; return 1; }
            return cmd_explain(root, argv[2]);
        }
        print_help();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
