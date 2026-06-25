#include "tui/views/context.hpp"

#include "support/utils.hpp"

#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

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

Element build_file_preview(const std::vector<RankedChunk>& chunks,
                           int selected_index,
                           const fs::path& root) {
    if (selected_index < 0 ||
        selected_index >= static_cast<int>(chunks.size())) {
        return text(" Select a chunk to preview") | dim | center;
    }

    const auto& r = chunks[static_cast<size_t>(selected_index)];
    const fs::path file_path = root / r.chunk.path;
    if (!fs::exists(file_path)) {
        return text(" File not found: " + r.chunk.path) | dim | center;
    }

    const std::string file_content = slurp(file_path);
    const auto lines = split_lines(file_content);
    Elements file_lines;

    const int margin = static_cast<int>(std::to_string(lines.size()).size()) + 1;
    const int context_start = std::max(0, r.chunk.start_line - 2);
    const int context_end   = std::min(static_cast<int>(lines.size()), r.chunk.end_line + 1);

    for (int li = context_start; li < context_end; ++li) {
        const int one_based = li + 1;
        const bool in_chunk = (one_based >= r.chunk.start_line &&
                               one_based <= r.chunk.end_line);

        std::ostringstream num_ss;
        num_ss << std::setw(margin) << std::right << one_based << " ";

        auto num = text(num_ss.str()) | dim;
        auto content = text(lines[static_cast<size_t>(li)]);

        if (in_chunk) {
            num = num | inverted;
            content = content | inverted;
        }

        file_lines.push_back(hbox(Elements{num, content}));
    }

    auto header = text(" " + r.chunk.path + " (" +
                       std::to_string(r.chunk.start_line) + "-" +
                       std::to_string(r.chunk.end_line) + " of " +
                       std::to_string(lines.size()) + " lines)")
                  | bold | color(lang_color(r.chunk.language));

    Elements preview_items;
    preview_items.push_back(header);
    preview_items.push_back(separator());
    preview_items.push_back(vbox(std::move(file_lines)));
    return vbox(std::move(preview_items)) | vscroll_indicator | frame | border | flex;
}

}  // namespace

Element render_context_view(TuiState& state,
                             const fs::path& root,
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

    auto list_panel = vbox(std::move(items)) | vscroll_indicator | frame | border;

    auto preview_panel = build_file_preview(
        state.last_retrieved_chunks, state.selected_index, root);

    auto header = text(" Context from last Chat query (" +
                       std::to_string(state.last_retrieved_chunks.size()) + " chunks)")
                  | bold | border;

    auto body = hbox(Elements{
        list_panel | size(WIDTH, GREATER_THAN, 30),
        preview_panel | flex
    });

    Elements children;
    children.push_back(header);
    children.push_back(body | flex);
    return vbox(std::move(children)) | flex;
}

} // namespace tui
} // namespace ultracode
