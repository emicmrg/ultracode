#pragma once

#include "tui/tui_app.hpp"
#include <ftxui/dom/elements.hpp>
#include <filesystem>

namespace ultracode {
namespace tui {

ftxui::Element render_context_view(TuiState& state,
                                    const std::filesystem::path& root,
                                    const Config& cfg);

} // namespace tui
} // namespace ultracode
