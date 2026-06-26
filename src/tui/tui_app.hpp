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

#include "app/config.hpp"
#include "parsing/chunk.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ultracode {
namespace tui {

enum class Tab {
    Context = 0,
    Chat,
    Index,
    Patches,
    Count
};

struct TuiState {
    Tab active_tab = Tab::Context;

    std::string chat_input;
    std::vector<std::string> chat_history;
    bool chat_streaming = false;
    std::string chat_model_override;
    std::vector<RankedChunk> last_retrieved_chunks;

    int selected_index = 0;
    int patches_selected_index = 0;
    float chat_scroll = 1.0f;
    std::vector<std::string> patch_ids;
    std::vector<std::string> patch_statuses;
    std::string patches_confirm_action;
    bool patches_confirm_visible = false;
    std::string patches_error;

    std::string status_text;
    bool ollama_connected = false;
};

int run_tui(const std::filesystem::path& root, const Config& cfg);

} // namespace tui
} // namespace ultracode
