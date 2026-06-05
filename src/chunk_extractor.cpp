#include "chunk_extractor.hpp"
#include "utils.hpp"

#ifdef ULTRACODE_USE_TREESITTER
#  include "ts_extractor.hpp"
#endif

#include <algorithm>
#include <regex>

// ──────────────────────────────────────────────────────────────────────────────
// Language detection
// ──────────────────────────────────────────────────────────────────────────────

std::string detect_language(const fs::path& path) {
    const std::string ext = lower(path.extension().string());
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" ||
        ext == ".c"   || ext == ".hpp"|| ext == ".h" || ext == ".hh")
        return "cpp";
    if (ext == ".go")                                   return "go";
    if (ext == ".py")                                   return "python";
    if (ext == ".js" || ext == ".jsx")                  return "javascript";
    if (ext == ".ts" || ext == ".tsx")                  return "typescript";
    if (ext == ".md" || ext == ".markdown")             return "markdown";
    if (ext == ".json" || ext == ".yaml" || ext == ".yml" ||
        ext == ".toml" || ext == ".mod"  || ext == ".sum")
        return "config";
    return "";
}

// ──────────────────────────────────────────────────────────────────────────────
// Heuristic helpers (internal)
// ──────────────────────────────────────────────────────────────────────────────

static std::vector<Chunk> chunk_by_lines(const std::vector<std::string>& lines,
                                          const std::string& path,
                                          const std::string& lang,
                                          int max_lines = 120) {
    std::vector<Chunk> chunks;
    for (int start = 1; start <= static_cast<int>(lines.size()); start += max_lines) {
        const int end = std::min(static_cast<int>(lines.size()), start + max_lines - 1);
        const std::string content = join_lines(lines, start, end);
        if (trim(content).empty()) continue;
        Chunk c;
        c.path       = path;
        c.language   = lang;
        c.symbol     = "lines_" + std::to_string(start) + "_" + std::to_string(end);
        c.start_line = start;
        c.end_line   = end;
        c.hash       = fnv1a_hex(path + content);
        c.id         = c.hash;
        chunks.push_back(c);
    }
    return chunks;
}

static int find_brace_block_end(const std::vector<std::string>& lines,
                                 int start_line,
                                 int max_lines = 400) {
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

static std::vector<Chunk> extract_cpp_like(const std::vector<std::string>& lines,
                                            const std::string& path,
                                            const std::string& lang) {
    std::vector<Chunk> chunks;
    std::regex class_re(R"(^\s*(class|struct|enum)\s+([A-Za-z_][A-Za-z0-9_]*))");
    std::regex func_re(
        R"(^\s*([A-Za-z_~][A-Za-z0-9_:<>~*&\s]+)\s+([A-Za-z_~][A-Za-z0-9_:~]*)\s*\([^;]*\)\s*(const\s*)?(\{|$))");
    // Matches JS/TS: (export )?(async )?function name(
    std::regex func_kw_re(
        R"(^\s*(?:export\s+)?(?:default\s+)?(?:async\s+)?function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\()");

    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        const std::string& line = lines[static_cast<size_t>(i - 1)];
        std::smatch m;
        std::string symbol;
        if (std::regex_search(line, m, class_re)) {
            symbol = m[1].str() + " " + m[2].str();
        } else if ((lang == "javascript" || lang == "typescript") &&
                   std::regex_search(line, m, func_kw_re)) {
            symbol = "function " + m[1].str() + "()";
        } else if (std::regex_search(line, m, func_re)) {
            symbol = m[2].str() + "()";
        } else {
            continue;
        }

        const int end = find_brace_block_end(lines, i);
        const std::string content = join_lines(lines, i, end);
        Chunk c;
        c.path       = path;
        c.language   = lang;
        c.symbol     = symbol;
        c.start_line = i;
        c.end_line   = end;
        c.hash       = fnv1a_hex(path + content);
        c.id         = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, lang);
    return chunks;
}

static std::vector<Chunk> extract_go(const std::vector<std::string>& lines,
                                      const std::string& path) {
    std::vector<Chunk> chunks;
    std::regex func_re(
        R"(^\s*func\s+(?:\([^)]*\)\s*)?([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\))");
    std::regex type_re(
        R"(^\s*type\s+([A-Za-z_][A-Za-z0-9_]*)\s+(struct|interface)\s*(\{|$))");

    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        const std::string& line = lines[static_cast<size_t>(i - 1)];
        std::smatch m;
        std::string symbol;
        if      (std::regex_search(line, m, func_re)) symbol = "func " + m[1].str() + "()";
        else if (std::regex_search(line, m, type_re)) symbol = "type " + m[1].str() + " " + m[2].str();
        else continue;

        const int end = find_brace_block_end(lines, i);
        const std::string content = join_lines(lines, i, end);
        Chunk c;
        c.path       = path;
        c.language   = "go";
        c.symbol     = symbol;
        c.start_line = i;
        c.end_line   = end;
        c.hash       = fnv1a_hex(path + content);
        c.id         = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, "go");
    return chunks;
}

static int indentation(const std::string& line) {
    int n = 0;
    for (char c : line) {
        if (c == ' ')       ++n;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

static std::vector<Chunk> extract_python(const std::vector<std::string>& lines,
                                          const std::string& path) {
    std::vector<Chunk> chunks;
    std::regex def_re(R"(^\s*(class|def)\s+([A-Za-z_][A-Za-z0-9_]*))");
    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        std::smatch m;
        if (!std::regex_search(lines[static_cast<size_t>(i - 1)], m, def_re)) continue;
        const int base_indent = indentation(lines[static_cast<size_t>(i - 1)]);
        int end = static_cast<int>(lines.size());
        for (int j = i + 1; j <= static_cast<int>(lines.size()); ++j) {
            const std::string& ln = lines[static_cast<size_t>(j - 1)];
            if (trim(ln).empty()) continue;
            if (indentation(ln) <= base_indent && std::regex_search(ln, def_re))
                { end = j - 1; break; }
            if (j - i > 400) { end = j; break; }
        }
        const std::string content = join_lines(lines, i, end);
        Chunk c;
        c.path       = path;
        c.language   = "python";
        c.symbol     = m[1].str() + " " + m[2].str();
        c.start_line = i;
        c.end_line   = end;
        c.hash       = fnv1a_hex(path + content);
        c.id         = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, "python");
    return chunks;
}

static std::vector<Chunk> extract_markdown(const std::vector<std::string>& lines,
                                            const std::string& path) {
    std::vector<Chunk> chunks;
    std::regex heading_re(R"(^(#{1,6})\s+(.+)$)");
    for (int i = 1; i <= static_cast<int>(lines.size()); ++i) {
        std::smatch m;
        if (!std::regex_search(lines[static_cast<size_t>(i - 1)], m, heading_re)) continue;
        const int level = static_cast<int>(m[1].str().size());
        int end = static_cast<int>(lines.size());
        for (int j = i + 1; j <= static_cast<int>(lines.size()); ++j) {
            std::smatch hm;
            if (std::regex_search(lines[static_cast<size_t>(j - 1)], hm, heading_re) &&
                static_cast<int>(hm[1].str().size()) <= level)
                { end = j - 1; break; }
        }
        const std::string content = join_lines(lines, i, end);
        Chunk c;
        c.path       = path;
        c.language   = "markdown";
        c.symbol     = m[2].str();
        c.start_line = i;
        c.end_line   = end;
        c.hash       = fnv1a_hex(path + content);
        c.id         = c.hash;
        chunks.push_back(c);
        i = end;
    }
    if (chunks.empty()) return chunk_by_lines(lines, path, "markdown");
    return chunks;
}

// ──────────────────────────────────────────────────────────────────────────────
// HeuristicExtractor implementation
// ──────────────────────────────────────────────────────────────────────────────

bool HeuristicExtractor::supports(const std::string& /*language*/) const {
    return true; // handles every language as a last resort
}

std::vector<Chunk> HeuristicExtractor::extract(const std::string& content,
                                                 const std::string& path,
                                                 const std::string& language) const {
    const auto lines = split_lines(content);
    if (language == "cpp" || language == "javascript" || language == "typescript")
        return extract_cpp_like(lines, path, language);
    if (language == "go")       return extract_go(lines, path);
    if (language == "python")   return extract_python(lines, path);
    if (language == "markdown") return extract_markdown(lines, path);
    return chunk_by_lines(lines, path, language.empty() ? "text" : language);
}

// ──────────────────────────────────────────────────────────────────────────────
// Public entry point
// ──────────────────────────────────────────────────────────────────────────────

std::vector<Chunk> extract_chunks(const fs::path& root, const fs::path& file) {
    const std::string rel     = fs::relative(file, root).generic_string();
    const std::string lang    = detect_language(file);
    const std::string content = slurp(file);

#ifdef ULTRACODE_USE_TREESITTER
    TreeSitterExtractor ts;
    if (ts.supports(lang)) {
        auto chunks = ts.extract(content, rel, lang);
        if (!chunks.empty()) return chunks;
    }
#endif

    HeuristicExtractor h;
    return h.extract(content, rel, lang);
}
