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

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

inline std::string slurp(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline bool write_text(const fs::path& path, const std::string& content) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << content;
    return true;
}

inline std::string trim(std::string s) {
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline std::string fnv1a_hex(const std::string& input) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned char c : input) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

inline std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

inline std::string join_lines(const std::vector<std::string>& lines,
                               int start_one_based, int end_one_based) {
    std::ostringstream ss;
    const int start = std::max(1, start_one_based);
    const int end   = std::min(static_cast<int>(lines.size()), end_one_based);
    for (int i = start; i <= end; ++i) ss << lines[static_cast<size_t>(i - 1)] << '\n';
    return ss.str();
}

inline std::string json_escape(const std::string& s) {
    std::ostringstream out;
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"':  out << "\\\""; break;
            case '\b': out << "\\b";  break;
            case '\f': out << "\\f";  break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:
                if (c < 0x20)
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c);
                else
                    out << c;
        }
    }
    return out.str();
}

inline std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += "'";
    return out;
}

inline std::string run_command_capture(const std::string& cmd) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return {};
    std::array<char, 4096> buffer{};
    std::string result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        result += buffer.data();
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return result;
}

inline std::string unescape_json_string(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) { out += s[i]; continue; }
        char n = s[++i];
        switch (n) {
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case '\\': out += '\\'; break;
            case '"':  out += '"';  break;
            default:   out += n;   break;
        }
    }
    return out;
}

inline std::optional<std::string> extract_json_string_after_key(const std::string& json,
                                                                  const std::string& key) {
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
        if (escaped) { value += '\\'; value += c; escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') return unescape_json_string(value);
        value += c;
    }
    return std::nullopt;
}

inline std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    for (unsigned char c : text) {
        if (std::isalnum(c) || c == '_') cur += static_cast<char>(std::tolower(c));
        else if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

inline std::vector<float> hashed_embedding(const std::string& text, int dim) {
    std::vector<float> v(static_cast<size_t>(dim), 0.0f);
    for (const auto& token : tokenize(text)) {
        const std::string h = fnv1a_hex(token);
        const uint64_t n   = std::stoull(h.substr(0, 16), nullptr, 16);
        const size_t idx   = static_cast<size_t>(n % static_cast<uint64_t>(dim));
        const float sign   = ((n >> 63) & 1u) ? -1.0f : 1.0f;
        v[idx] += sign;
    }
    double norm = 0.0;
    for (float x : v) norm += static_cast<double>(x) * x;
    norm = std::sqrt(norm);
    if (norm > 0.0) for (float& x : v) x = static_cast<float>(x / norm);
    return v;
}

inline std::vector<float> parse_first_embedding(const std::string& json) {
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

inline std::vector<std::vector<float>> parse_embeddings(const std::string& json) {
    const size_t key = json.find("\"embeddings\"");
    if (key == std::string::npos) return {};
    const size_t outer = json.find('[', key);
    if (outer == std::string::npos) return {};

    std::vector<std::vector<float>> embeddings;
    for (size_t i = outer + 1; i < json.size(); ++i) {
        if (json[i] != '[') {
            if (json[i] == ']') break;
            continue;
        }

        const size_t start = i;
        int depth = 1;
        for (++i; i < json.size() && depth > 0; ++i) {
            if (json[i] == '[') ++depth;
            else if (json[i] == ']') --depth;
        }
        if (depth != 0) return {};

        const size_t end = i - 1;
        std::string body = json.substr(start + 1, end - start - 1);
        std::vector<float> values;
        std::stringstream ss(body);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item = trim(item);
            if (item.empty()) continue;
            try {
                values.push_back(std::stof(item));
            } catch (...) {
                return {};
            }
        }
        embeddings.push_back(values);
    }
    return embeddings;
}
