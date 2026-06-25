#include "tui/components/status_bar.hpp"

#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

using namespace ftxui;

namespace ultracode {
namespace tui {

Element render_status_bar(const TuiState& state) {
    auto tabs = text(
        " F1:Context  F2:Chat  F3:Index  F4:Patches  Tab:Next  Esc:Quit"
    ) | dim;

    auto ollama_status = state.ollama_connected
        ? text(" Ollama:ON ") | color(Color::Green)
        : text(" Ollama:OFF ") | color(Color::Red);

    auto tab_names = std::vector<std::string>{"Context", "Chat", "Index", "Patches"};
    auto tab = text(" [" + tab_names[static_cast<int>(state.active_tab)] + "] ") | bold;

    return hbox(Elements{ollama_status, tab, tabs, filler(), text(" ultracode ") | dim})
        | border | size(HEIGHT, EQUAL, 3);
}

} // namespace tui
} // namespace ultracode
