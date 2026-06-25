#pragma once

#include "app/config.hpp"
#include "parsing/chunk.hpp"

#include <filesystem>
#include <string>
#include <functional>
#include <vector>

namespace ultracode {
namespace tui {

enum class Tab {
    Search = 0,
    Chat,
    Index,
    Patches,
    Count
};

struct TuiState {
    Tab active_tab = Tab::Search;
    std::string search_query;
    std::vector<RankedChunk> search_results;
    std::string search_error;
    bool search_running = false;

    std::string chat_input;
    std::vector<std::string> chat_history;
    bool chat_streaming = false;
    std::string chat_model_override;

    int selected_index = 0;

    std::string status_text;
    bool ollama_connected = false;
};

int run_tui(const std::filesystem::path& root, const Config& cfg);

} // namespace tui
} // namespace ultracode
