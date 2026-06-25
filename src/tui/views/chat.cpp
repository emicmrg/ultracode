#include "tui/views/chat.hpp"

#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

namespace ultracode {
namespace tui {

Element render_chat_view(TuiState& state,
                          const fs::path& /*root*/,
                          const Config& /*cfg*/,
                          Element input_rendered) {
    Elements history_items;
    int max_visible = 10;
    int start = std::max(0, static_cast<int>(state.chat_history.size()) - max_visible);
    for (int i = start; i < static_cast<int>(state.chat_history.size()); ++i) {
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
            body = paragraph(msg) | flex;
        }

        Elements msg_rows;
        msg_rows.push_back(prefix);
        msg_rows.push_back(body);
        history_items.push_back(vbox(std::move(msg_rows)) | border);
    }
    if (state.chat_history.empty()) {
        history_items.push_back(
            text(" No conversation yet. Type a message and press Enter. "
                 "Use /patch <instruction> to generate a patch.")
            | dim | center);
    }

    auto history = vbox(std::move(history_items)) | vscroll_indicator | frame | flex;

    Elements children;
    children.push_back(history);
    children.push_back(input_rendered);
    return vbox(std::move(children)) | flex;
}

} // namespace tui
} // namespace ultracode
