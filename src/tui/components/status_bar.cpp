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
