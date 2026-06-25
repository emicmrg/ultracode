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
    int chat_scroll = 0;
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
