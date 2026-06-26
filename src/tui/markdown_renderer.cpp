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

#include "tui/markdown_renderer.hpp"

#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

using namespace ftxui;

namespace ultracode {
namespace tui {

namespace {

enum class SegType { Normal, Bold, InlineCode, Link };

struct Seg {
    SegType type = SegType::Normal;
    std::string text;
    std::string url; // only for Link
};

// Parse a single line for inline markdown:
//   `inline code`, **bold**, [text](url)
std::vector<Seg> parse_inline(const std::string& line_in) {
    std::vector<Seg> out;
    std::string line = line_in; // mutable copy
    size_t i = 0;
    while (i < line.size()) {
        // Inline code: `...`
        if (line[i] == '`') {
            size_t j = line.find('`', i + 1);
            if (j != std::string::npos) {
                if (i > 0) {
                    out.push_back({SegType::Normal, line.substr(0, i), ""});
                }
                out.push_back({SegType::InlineCode, line.substr(i + 1, j - i - 1), ""});
                line = line.substr(j + 1);
                i = 0;
                continue;
            }
        }
        // Bold: **...**
        if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*') {
            size_t j = line.find("**", i + 2);
            if (j != std::string::npos) {
                if (i > 0) {
                    out.push_back({SegType::Normal, line.substr(0, i), ""});
                }
                out.push_back({SegType::Bold, line.substr(i + 2, j - i - 2), ""});
                line = line.substr(j + 2);
                i = 0;
                continue;
            }
        }
        // Link: [text](url)
        if (line[i] == '[') {
            size_t close_bracket = line.find(']', i + 1);
            if (close_bracket != std::string::npos && close_bracket + 1 < line.size() && line[close_bracket + 1] == '(') {
                size_t close_paren = line.find(')', close_bracket + 2);
                if (close_paren != std::string::npos) {
                    if (i > 0) {
                        out.push_back({SegType::Normal, line.substr(0, i), ""});
                    }
                    std::string link_text = line.substr(i + 1, close_bracket - i - 1);
                    std::string link_url = line.substr(close_bracket + 2, close_paren - close_bracket - 2);
                    out.push_back({SegType::Link, link_text, link_url});
                    line = line.substr(close_paren + 1);
                    i = 0;
                    continue;
                }
            }
        }
        ++i;
    }
    if (!line.empty()) {
        out.push_back({SegType::Normal, line, ""});
    }
    return out;
}

// Split a segment at a given byte length, preserving style.
std::pair<Seg, Seg> split_seg(const Seg& seg, size_t len) {
    Seg first{seg.type, seg.text.substr(0, len), seg.url};
    Seg second{seg.type, seg.text.substr(len), seg.url};
    return {first, second};
}

// Word-wrap segments into lines. Each line is a vector of segments.
// Preserves styling across line breaks.
std::vector<std::vector<Seg>> wrap_segments(const std::vector<Seg>& segs, int width) {
    std::vector<std::vector<Seg>> lines;
    std::vector<Seg> current;
    int used = 0;

    for (auto seg : segs) {
        while (!seg.text.empty()) {
            int seg_len = static_cast<int>(seg.text.size());
            if (used == 0) {
                // Start of line: try to fit as much as possible
                if (seg_len <= width) {
                    current.push_back(seg);
                    used = seg_len;
                    break;
                }
                // Segment too long for an empty line: need to split
                int cut = width;
                // Try to find a space to break on
                int space_pos = -1;
                for (int k = width; k > 0; --k) {
                    if (static_cast<size_t>(k) < seg.text.size() && seg.text[static_cast<size_t>(k)] == ' ') {
                        space_pos = k;
                        break;
                    }
                }
                if (space_pos > 0) {
                    cut = space_pos;
                }
                auto [first, rest] = split_seg(seg, static_cast<size_t>(cut));
                current.push_back(first);
                lines.push_back(std::move(current));
                current.clear();
                used = 0;
                seg = rest;
                // If we broke on a space, skip it
                if (!seg.text.empty() && seg.text[0] == ' ') {
                    seg.text = seg.text.substr(1);
                }
            } else {
                // Middle of line: check if it fits with a space
                int need = 1 + seg_len;
                if (need <= width - used) {
                    // prepend a space to the seg text
                    seg.text = " " + seg.text;
                    seg_len += 1;
                    current.push_back(seg);
                    used += seg_len;
                    break;
                }
                // Doesn't fit: flush current line and retry
                lines.push_back(std::move(current));
                current.clear();
                used = 0;
            }
        }
    }
    if (!current.empty()) {
        lines.push_back(std::move(current));
    }
    return lines;
}

// Convert a segment into a styled FTXUI element.
Element seg_to_element(const Seg& seg) {
    switch (seg.type) {
        case SegType::Bold:
            return ftxui::text(seg.text) | bold | color(Color::White);
        case SegType::InlineCode:
            return ftxui::text(seg.text) | color(Color::CyanLight);
        case SegType::Link:
            return ftxui::text(seg.text) | underlined | color(Color::BlueLight);
        default:
            return ftxui::text(seg.text) | color(Color::Default);
    }
}

// Build an FTXUI element from a line of segments.
Element build_line(const std::vector<Seg>& line) {
    Elements children;
    for (const auto& seg : line) {
        children.push_back(seg_to_element(seg));
    }
    return hbox(std::move(children));
}

// Trim whitespace from start/end
std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

} // namespace

Elements render_markdown(const std::string& markdown_text, int width) {
    Elements out;
    std::stringstream ss(markdown_text);
    std::string line;
    bool in_code_block = false;
    std::vector<std::string> code_lines;
    std::string code_lang;

    while (std::getline(ss, line)) {
        // Code block handling
        if (!in_code_block && line.size() >= 3 && line.substr(0, 3) == "```") {
            in_code_block = true;
            code_lang = trim(line.substr(3));
            code_lines.clear();
            continue;
        }
        if (in_code_block && line.size() >= 3 && line.substr(0, 3) == "```") {
            in_code_block = false;
            // Render code block
            Elements code_elements;
            for (const auto& cl : code_lines) {
                code_elements.push_back(ftxui::text(cl) | color(Color::Cyan));
            }
            if (!code_lang.empty()) {
                code_elements.insert(code_elements.begin(),
                    ftxui::text(" " + code_lang + " ") | color(Color::GrayLight) | underlined);
            }
            out.push_back(vbox(std::move(code_elements)) | border | color(Color::Cyan));
            code_lines.clear();
            code_lang.clear();
            continue;
        }
        if (in_code_block) {
            code_lines.push_back(line);
            continue;
        }

        // Empty line
        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            out.push_back(ftxui::text(""));
            continue;
        }

        // Headers
        if (trimmed.size() >= 2 && trimmed[0] == '#') {
            int level = 0;
            while (level < static_cast<int>(trimmed.size()) && trimmed[level] == '#' && level < 6) ++level;
            if (level < static_cast<int>(trimmed.size()) && trimmed[level] == ' ') {
                std::string header_text = trim(trimmed.substr(level + 1));
                Element header_el = ftxui::text(header_text) | bold;
                if (level == 1) {
                    header_el = header_el | color(Color::YellowLight) | underlined;
                } else if (level == 2) {
                    header_el = header_el | color(Color::Yellow);
                } else {
                    header_el = header_el | color(Color::GrayLight);
                }
                out.push_back(std::move(header_el));
                continue;
            }
        }

        // Blockquote
        if (trimmed[0] == '>') {
            std::string quote_text = trim(trimmed.substr(1));
            auto segs = parse_inline(quote_text);
            auto wrapped = wrap_segments(segs, width - 2);
            for (size_t i = 0; i < wrapped.size(); ++i) {
                Element el = build_line(wrapped[i]) | color(Color::GrayLight);
                out.push_back(hbox(Elements{ftxui::text("┃ ") | color(Color::GrayDark), el}));
            }
            continue;
        }

        // List items
        bool is_list = false;
        std::string list_content;
        Element prefix_el;

        // Unordered: - item or * item
        if (trimmed.size() >= 2 && (trimmed[0] == '-' || trimmed[0] == '*') && trimmed[1] == ' ') {
            is_list = true;
            list_content = trim(trimmed.substr(2));
            prefix_el = ftxui::text("• ") | color(Color::GreenLight);
        }
        // Ordered: 1. item, 12. item
        else if (!trimmed.empty() && std::isdigit(static_cast<unsigned char>(trimmed[0]))) {
            size_t dot_pos = trimmed.find('.');
            if (dot_pos != std::string::npos && dot_pos + 1 < trimmed.size() && trimmed[dot_pos + 1] == ' ') {
                bool all_digits = true;
                for (size_t k = 0; k < dot_pos; ++k) {
                    if (!std::isdigit(static_cast<unsigned char>(trimmed[k]))) { all_digits = false; break; }
                }
                if (all_digits) {
                    is_list = true;
                    list_content = trim(trimmed.substr(dot_pos + 2));
                    prefix_el = ftxui::text(trimmed.substr(0, dot_pos + 1) + " ") | color(Color::GreenLight);
                }
            }
        }

        if (is_list) {
            auto segs = parse_inline(list_content);
            auto wrapped = wrap_segments(segs, width - 4);
            for (size_t i = 0; i < wrapped.size(); ++i) {
                Element el = build_line(wrapped[i]);
                if (i == 0) {
                    out.push_back(hbox(Elements{prefix_el, el}));
                } else {
                    out.push_back(hbox(Elements{ftxui::text("  ") | color(Color::Default), el}));
                }
            }
            continue;
        }

        // Normal paragraph with inline parsing
        auto segs = parse_inline(trimmed);
        auto wrapped = wrap_segments(segs, width);
        for (const auto& wline : wrapped) {
            out.push_back(build_line(wline));
        }
    }

    // Handle unclosed code block at EOF
    if (in_code_block && !code_lines.empty()) {
        Elements code_elements;
        for (const auto& cl : code_lines) {
            code_elements.push_back(ftxui::text(cl) | color(Color::Cyan));
        }
        if (!code_lang.empty()) {
            code_elements.insert(code_elements.begin(),
                ftxui::text(" " + code_lang + " ") | color(Color::GrayLight) | underlined);
        }
        out.push_back(vbox(std::move(code_elements)) | border | color(Color::Cyan));
    }

    return out;
}

} // namespace tui
} // namespace ultracode
