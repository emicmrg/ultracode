#include "tui/views/patches.hpp"

#include "support/utils.hpp"

#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <string>

using namespace ftxui;

namespace ultracode {
namespace tui {

Element render_patches_view(TuiState& state,
                             const fs::path& root,
                             const Config& /*cfg*/) {
    Elements items;

    const auto patches_dir = root / ".ultracode" / "patches";
    if (!fs::exists(patches_dir)) {
        items.push_back(text(" No patches yet.") | dim);
    } else {
        int count = 0;
        for (auto it = fs::directory_iterator(patches_dir);
             it != fs::directory_iterator(); ++it) {
            if (it->path().extension() == ".meta") {
                ++count;
                const bool selected = (count - 1 == state.patches_selected_index);
                const std::string name = it->path().stem().string();
                const std::string text_content = slurp(it->path());
                std::string patch_status = "?";
                if (text_content.find("\tapplied\t") != std::string::npos)
                    patch_status = "applied";
                else if (text_content.find("\trejected\t") != std::string::npos)
                    patch_status = "rejected";
                else
                    patch_status = "pending";

                Color st_color = patch_status == "applied"   ? Color::Green
                                 : patch_status == "rejected" ? Color::Red
                                                              : Color::Yellow;

                auto line = text(" " + patch_status + "  " + name) | color(st_color);
                if (selected) line = line | inverted;
                items.push_back(line);
            }
        }
        if (count == 0) {
            items.push_back(text(" No patches yet.") | dim);
        } else {
            items.push_back(separator());
            items.push_back(text(" arrows: select  a:apply  r:reject") | dim);
        }
    }

    return vbox(std::move(items)) | border | flex;
}

} // namespace tui
} // namespace ultracode
