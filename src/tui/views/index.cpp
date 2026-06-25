#include "tui/views/index.hpp"

#include "index/index_store.hpp"

#include <ftxui/dom/elements.hpp>
#include <filesystem>
#include <map>
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

Element render_index_view(TuiState& /*state*/,
                          const fs::path& root,
                          const Config& /*cfg*/) {
    Elements items;
    items.push_back(text(" Repository: " + root.string()) | bold);

    const auto manifest = load_manifest(root);
    items.push_back(text(" Chunks indexed: " + std::to_string(manifest.size())));

    std::map<std::string, int> by_lang;
    for (const auto& c : manifest) {
        by_lang[c.language]++;
    }
    for (const auto& [lang, count] : by_lang) {
        items.push_back(text("   " + lang + ": " + std::to_string(count))
            | color(lang_color(lang)));
    }

    if (const auto summary = load_index_run_summary(root)) {
        items.push_back(separator());
        items.push_back(text(" Last index run:") | bold);
        items.push_back(text("   Scanned: " + std::to_string(summary->scanned_files)));
        items.push_back(text("   New:     " + std::to_string(summary->new_files)));
        items.push_back(text("   Changed: " + std::to_string(summary->changed_files)));
        items.push_back(text("   Reused:  " + std::to_string(summary->reused_files)));
        items.push_back(text("   Deleted: " + std::to_string(summary->deleted_files)));
    } else {
        items.push_back(text(" No index data yet. Run 'index' first.") | dim);
    }

    return vbox(std::move(items)) | border | flex;
}

} // namespace tui
} // namespace ultracode
