#include "tui/views/chat.hpp"

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

Elements wrap_lines(const std::string& raw_text, int width) {
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
    constexpr int kPageSize = 6;
    Elements history_items;

    const int total = static_cast<int>(state.chat_history.size());
    const int offset = state.chat_scroll * 2;
    const int start = std::max(0, total - kPageSize - offset);
    const int end = std::min(total, start + kPageSize);

    for (int i = start; i < end; ++i) {
        const auto& msg = state.chat_history[static_cast<size_t>(i)];
        const bool is_user = (i % 2 == 0);

        Element prefix;
        if (is_user) {
            prefix = text(" You ") | color(Color::Cyan) | bold;
        } else if (msg.empty() && i == static_cast<int>(state.chat_history.size()) - 1) {
            prefix = text(" AI  ") | color(Color::Green) | bold;
        } else {
            prefix = text(" AI  ") | color(Color::Green) | bold;
        }

        Element body;
        if (!is_user && msg.empty() && state.chat_streaming) {
            auto dots = text(" Thinking...") | dim | blink;
            body = hbox(Elements{dots, filler()}) | flex;
        } else {
            body = vbox(wrap_lines(msg, kWrapWidth));
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
        const int above = start;
        const int below = std::max(0, total - end);
        const int total_turns = total / 2;
        const int visible_first = start / 2 + 1;
        const int visible_last = (end - 1) / 2 + 1;
        history_items.push_back(separator());
        history_items.push_back(
            text(" arrows:scroll  turn " + std::to_string(visible_first) +
                 "-" + std::to_string(visible_last) + "/" +
                 std::to_string(total_turns) +
                 "  (older: " + std::to_string(above / 2) +
                 "  newer: " + std::to_string(below / 2) + ")") | dim);
    }

    auto history = vbox(std::move(history_items)) | vscroll_indicator | frame | flex;

    Elements children;
    children.push_back(history);
    children.push_back(input_rendered);
    return vbox(std::move(children)) | flex;
}

} // namespace tui
} // namespace ultracode
