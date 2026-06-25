#include "tui/views/context.hpp"

#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <sstream>
#include <string>

namespace fs = std::filesystem;
using namespace ftxui;

namespace ultracode {
namespace tui {

namespace {

Color lang_color(const std::string& lang) {
    if (lang == "cpp")        return Color::Cyan;
    if (lang == "go")         return Color::CyanLight;
    if (lang == "python")     return Color::Yellow;
    if (lang == "javascript") return Color::YellowLight;
    if (lang == "typescript") return Color::BlueLight;
    if (lang == "markdown")   return Color::Green;
    if (lang == "config")     return Color::Magenta;
    return Color::White;
}

}  // namespace

Element render_context_view(TuiState& state,
                             const fs::path& /*root*/,
                             const Config& /*cfg*/) {
    if (state.last_retrieved_chunks.empty()) {
        return text(" No context yet. Send a message in Chat (F2) first.") | dim | center | border | flex;
    }

    Elements items;
    for (size_t i = 0; i < state.last_retrieved_chunks.size(); ++i) {
        const auto& r = state.last_retrieved_chunks[i];
        const bool selected = (static_cast<int>(i) == state.selected_index);

        auto line = text(
            " " + std::to_string(i + 1) + ". " +
            r.chunk.path + ":" +
            std::to_string(r.chunk.start_line) + "-" +
            std::to_string(r.chunk.end_line)
        );
        if (selected) line = line | inverted;
        line = line | color(lang_color(r.chunk.language));
        items.push_back(line);

        std::ostringstream score_ss;
        score_ss << "    " << r.chunk.symbol
                 << "  score=" << std::fixed << std::setprecision(3) << r.score
                 << " vec=" << std::fixed << std::setprecision(4) << r.vector_score
                 << " lex=" << std::fixed << std::setprecision(4) << r.lexical_score;

        auto detail = text(score_ss.str()) | dim;
        if (selected) detail = detail | inverted;
        items.push_back(detail);
    }

    auto results = vbox(std::move(items)) | vscroll_indicator | frame | border | flex;

    auto header = text(" Context from last Chat query (" +
                       std::to_string(state.last_retrieved_chunks.size()) + " chunks)")
                  | bold | border;

    Elements children;
    children.push_back(header);
    children.push_back(results);
    return vbox(std::move(children)) | flex;
}

} // namespace tui
} // namespace ultracode
