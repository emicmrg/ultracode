#include "tui/views/chat.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
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
                          const Config& /*cfg*/) {
    Elements history_items;
    int max_visible = 10;
    int start = std::max(0, static_cast<int>(state.chat_history.size()) - max_visible);
    for (int i = start; i < static_cast<int>(state.chat_history.size()); ++i) {
        const auto& msg = state.chat_history[static_cast<size_t>(i)];
        const bool is_user = (i % 2 == 0);

        Element prefix;
        if (is_user) {
            prefix = text(" You ") | color(Color::Cyan) | bold;
        } else {
            prefix = text(" AI  ") | color(Color::Green) | bold;
        }

        auto message = paragraph(msg) | flex;

        Elements msg_row;
        msg_row.push_back(prefix | size(WIDTH, EQUAL, 6));
        msg_row.push_back(separator() | color(Color::GrayDark));
        msg_row.push_back(message);
        history_items.push_back(hbox(std::move(msg_row)) | border);
    }
    if (state.chat_history.empty()) {
        history_items.push_back(
            text(" No conversation yet. Type a message and press Enter. "
                 "Use /patch <instruction> to generate a patch.")
            | dim | center);
    }

    auto history = vbox(std::move(history_items)) | vscroll_indicator | frame | flex;
    auto input = Input(&state.chat_input, "Type message and press Enter...");
    auto input_rendered = input->Render() | border | size(HEIGHT, EQUAL, 3);
    if (state.chat_streaming) {
        input_rendered = input_rendered | dim;
    }

    Elements children;
    children.push_back(history);
    children.push_back(input_rendered);
    return vbox(std::move(children)) | flex;
}

} // namespace tui
} // namespace ultracode
