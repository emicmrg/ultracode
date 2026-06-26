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

#include "tui/views/chat.hpp"
#include "tui/markdown_renderer.hpp"

#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

namespace ultracode {
namespace tui {

namespace {

Elements wrap_lines_plain(const std::string& raw_text, int width) {
    Elements lines;
    std::stringstream ss(raw_text);
    std::string physical;
    while (std::getline(ss, physical)) {
        if (physical.empty()) {
            lines.push_back(text(""));
            continue;
        }
        while (static_cast<int>(physical.size()) > width) {
            int cut = static_cast<int>(physical.find_last_of(" \t", static_cast<size_t>(width)));
            if (cut <= 0) cut = width;
            lines.push_back(text(physical.substr(0, static_cast<size_t>(cut))));
            if (static_cast<size_t>(cut) < physical.size() && physical[static_cast<size_t>(cut)] == ' ')
                ++cut;
            physical = physical.substr(static_cast<size_t>(cut));
        }
        if (!physical.empty())
            lines.push_back(text(physical));
    }
    return lines;
}

}  // namespace

Element render_chat_view(TuiState& state,
                          const fs::path& /*root*/,
                          const Config& /*cfg*/,
                          Element input_rendered) {
    constexpr int kWrapWidth = 75;
    Elements history_items;

    for (size_t i = 0; i < state.chat_history.size(); ++i) {
        const auto& msg = state.chat_history[i];
        const bool is_user = (i % 2 == 0);

        Element prefix;
        if (is_user) {
            prefix = text(" You ") | color(Color::Cyan) | bold;
        } else if (msg.empty() && i == state.chat_history.size() - 1) {
            prefix = text(" AI  ") | color(Color::Green) | bold;
        } else {
            prefix = text(" AI  ") | color(Color::Green) | bold;
        }

        Element body;
        if (!is_user && msg.empty() && state.chat_streaming) {
            auto dots = text(" Thinking...") | dim | blink;
            body = hbox(Elements{dots, filler()}) | flex;
        } else if (is_user) {
            body = vbox(wrap_lines_plain(msg, kWrapWidth));
        } else {
            body = vbox(render_markdown(msg, kWrapWidth));
        }

        Elements msg_rows;
        msg_rows.push_back(prefix);
        msg_rows.push_back(separator() | dim);
        msg_rows.push_back(body);
        history_items.push_back(vbox(std::move(msg_rows)) | border);
    }
    if (state.chat_history.empty()) {
        history_items.push_back(
            text(" No conversation yet. Type a message and press Enter. "
                 "Use /patch <instruction> to generate a patch.")
            | dim | center);
    } else {
        const int turns = static_cast<int>(state.chat_history.size()) / 2;
        history_items.push_back(separator());
        history_items.push_back(
            text(" " + std::to_string(turns) + " turns  mouse-wheel/arrows/PageUp/PageDn:scroll")
            | dim);
    }

    auto history = vbox(std::move(history_items))
        | focusPositionRelative(0.0f, state.chat_scroll)
        | frame | vscroll_indicator | flex;

    Elements children;
    children.push_back(history);
    children.push_back(input_rendered);
    return vbox(std::move(children)) | flex;
}

} // namespace tui
} // namespace ultracode
