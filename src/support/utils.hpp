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

inline std::string truncate_lines(const std::string& text, int max_lines) {
    if (max_lines <= 0) return text;
    auto lines = split_lines(text);
    if (static_cast<int>(lines.size()) <= max_lines) return text;
    lines.resize(static_cast<size_t>(max_lines));
    std::string result;
    for (const auto& line : lines) result += line + '\n';
    result += "...\n";
    return result;
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


