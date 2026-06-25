#include "tui/views/patches.hpp"

#include "support/utils.hpp"

#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

namespace ultracode {
namespace tui {

Element render_patches_view(TuiState& state,
                             const fs::path& root,
                             const Config& /*cfg*/) {
    state.patch_ids.clear();
    state.patch_statuses.clear();

    const auto patches_dir = root / ".ultracode" / "patches";

    auto build_list = [&]() -> Element {
        Elements items;
        if (!fs::exists(patches_dir)) {
            items.push_back(text(" No patches yet.") | dim);
            return vbox(std::move(items));
        }

        for (auto it = fs::directory_iterator(patches_dir);
             it != fs::directory_iterator(); ++it) {
            if (it->path().extension() != ".meta") continue;

            const std::string id = it->path().stem().string();
            const std::string text_content = slurp(it->path());
            std::string status = "?";
            if (text_content.find("\tapplied\t") != std::string::npos)
                status = "applied";
            else if (text_content.find("\trejected\t") != std::string::npos)
                status = "rejected";
            else
                status = "pending";

            state.patch_ids.push_back(id);
            state.patch_statuses.push_back(status);

            const int idx = static_cast<int>(state.patch_ids.size()) - 1;
            const bool selected = (idx == state.patches_selected_index);

            Color st_color = status == "applied"   ? Color::Green
                             : status == "rejected" ? Color::Red
                                                     : Color::Yellow;

            std::string label = status;
            if (label.size() < 8) label += std::string(8 - label.size(), ' ');
            auto line = text(" " + label + id.substr(0, 16)) | color(st_color);
            if (selected) line = line | inverted;
            items.push_back(line);
        }

        if (items.empty()) {
            items.push_back(text(" No patches yet.") | dim);
        } else {
            items.push_back(separator());
            items.push_back(text(" arrows:select  a:apply  r:reject") | dim);
        }

        return vbox(std::move(items));
    };

    auto build_preview = [&]() -> Element {
        if (state.patch_ids.empty() ||
            state.patches_selected_index < 0 ||
            state.patches_selected_index >= static_cast<int>(state.patch_ids.size())) {
            return text(" Select a patch to preview") | dim | center;
        }

        const std::string id = state.patch_ids[static_cast<size_t>(state.patches_selected_index)];
        const fs::path diff_path = patches_dir / (id + ".diff");
        if (!fs::exists(diff_path)) {
            return text(" Diff file not found") | dim | center;
        }

        const std::string diff_text = slurp(diff_path);
        Elements diff_lines;
        std::stringstream ss(diff_text);
        std::string line;
        while (std::getline(ss, line)) {
            if (line.empty()) {
                diff_lines.push_back(text(""));
            } else if (line[0] == '+') {
                diff_lines.push_back(text(line) | color(Color::Green));
            } else if (line[0] == '-') {
                diff_lines.push_back(text(line) | color(Color::Red));
            } else if (line[0] == '@') {
                diff_lines.push_back(text(line) | color(Color::Cyan));
            } else {
                diff_lines.push_back(text(line) | dim);
            }
        }

        auto content = vbox(std::move(diff_lines)) | frame | vscroll_indicator;
        if (diff_lines.empty()) {
            content = text(" Empty diff") | dim | center;
        }
        return content;
    };

    auto list_panel   = build_list()   | border | size(WIDTH, GREATER_THAN, 30);
    auto preview_panel = build_preview() | border | size(HEIGHT, GREATER_THAN, 10) | flex;

    Element body;
    if (state.patches_confirm_visible) {
        auto confirm_text = text(
            " " + state.patches_confirm_action + " patch " +
            (state.patch_ids.empty() ? "?"
             : state.patch_ids[static_cast<size_t>(std::min(
                   state.patches_selected_index,
                   static_cast<int>(state.patch_ids.size()) - 1))]) +
            "? [y/N] ") | bold | color(Color::Yellow);

        auto error_line = state.patches_error.empty()
            ? text("")
            : text(" error: " + state.patches_error) | color(Color::Red);

        body = vbox(Elements{
            hbox(Elements{list_panel | size(WIDTH, EQUAL, 36), preview_panel}),
            confirm_text | border,
            error_line,
        });
    } else {
        state.patches_error.clear();
        body = hbox(Elements{list_panel | size(WIDTH, EQUAL, 36), preview_panel});
    }

    return body | flex;
}

} // namespace tui
} // namespace ultracode
