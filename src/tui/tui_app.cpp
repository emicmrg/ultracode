#include "tui/tui_app.hpp"

#include "app/config.hpp"
#include "index/index_store.hpp"
#include "index/repo_scanner.hpp"
#include "llm/ollama_client.hpp"
#include "parsing/chunk_extractor.hpp"
#include "retrieval/retriever.hpp"
#include "support/utils.hpp"
#include "vcs/git_diff.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

namespace ultracode {
namespace tui {

namespace {

Color language_color(const std::string& lang) {
    if (lang == "cpp")        return Color::Cyan;
    if (lang == "go")         return Color::CyanLight;
    if (lang == "python")     return Color::Yellow;
    if (lang == "javascript") return Color::YellowLight;
    if (lang == "typescript") return Color::BlueLight;
    if (lang == "markdown")   return Color::Green;
    if (lang == "config")     return Color::Magenta;
    return Color::White;
}

Element render_search_view(TuiState& state,
                           const fs::path& root,
                           const Config& cfg) {
    auto input_box = Input(&state.search_query, "Type query and press Enter...");
    auto input_rendered = input_box->Render() | border | size(HEIGHT, EQUAL, 3);

    Elements result_items;
    if (state.search_running) {
        result_items.push_back(text(" Searching...") | dim);
    } else if (!state.search_error.empty()) {
        result_items.push_back(text(" " + state.search_error) | color(Color::Red));
    } else if (state.search_results.empty()) {
        result_items.push_back(text(" No results yet. Type a query and press Enter.") | dim);
    } else {
        for (size_t i = 0; i < state.search_results.size(); ++i) {
            const auto& r = state.search_results[i];
            const bool selected = (static_cast<int>(i) == state.selected_index);
            auto line = text(
                " " + std::to_string(i + 1) + ". " +
                r.chunk.path + ":" +
                std::to_string(r.chunk.start_line) + "-" +
                std::to_string(r.chunk.end_line) + "  "
            );
            if (selected) line = line | inverted;
            line = line | color(language_color(r.chunk.language));
            result_items.push_back(line);

            auto detail = text(
                "    " + r.chunk.symbol +
                "  score=" + std::to_string(r.score).substr(0, 5) +
                " vec=" + std::to_string(r.vector_score).substr(0, 5) +
                " lex=" + std::to_string(r.lexical_score).substr(0, 5)
            ) | dim;
            if (selected) detail = detail | inverted;
            result_items.push_back(detail);
        }
    }

    auto results = vbox(std::move(result_items)) | vscroll_indicator | frame | border | flex;

    Elements children;
    children.push_back(input_rendered);
    children.push_back(results);
    return vbox(std::move(children)) | flex;
}

std::string render_context_blocks_tui(const std::vector<RankedChunk>& ranked,
                                       int max_context_chars) {
    std::ostringstream ctx;
    int used = 0;
    for (const auto& r : ranked) {
        std::ostringstream block;
        block << "<chunk path=\"" << r.chunk.path << "\" lines=\""
              << r.chunk.start_line << "-" << r.chunk.end_line
              << "\" symbol=\"" << r.chunk.symbol << "\">\n"
              << r.content << "\n</chunk>\n\n";
        const std::string rendered = block.str();
        if (used + static_cast<int>(rendered.size()) > max_context_chars) {
            break;
        }
        ctx << rendered;
        used += static_cast<int>(rendered.size());
    }
    return ctx.str();
}

Element render_chat_view(TuiState& state,
                         const fs::path& /*root*/,
                         const Config& /*cfg*/) {
    Elements history_items;
    int max_visible = 10;
    int start = std::max(0, static_cast<int>(state.chat_history.size()) - max_visible);
    for (int i = start; i < static_cast<int>(state.chat_history.size()); ++i) {
        const auto& msg = state.chat_history[static_cast<size_t>(i)];
        bool is_user = (i % 2 == 0);
        auto prefix = is_user ? text(" You> ") | color(Color::Cyan) :
                                 text(" AI>  ") | color(Color::Green);
        history_items.push_back(hbox(Elements{prefix, paragraph(msg)}));
    }
    if (state.chat_history.empty()) {
        history_items.push_back(text(" No conversation yet. Type a message below.") | dim);
    }

    auto history = vbox(std::move(history_items)) | vscroll_indicator | frame | border | flex;
    auto input = Input(&state.chat_input, "Type message and press Enter...");
    auto input_rendered = input->Render() | border | size(HEIGHT, EQUAL, 3);
    if (state.chat_streaming) {
        input_rendered = input_rendered | dim;
    }

    Elements children;
    children.push_back(history);
    children.push_back(input_rendered);
    return vbox(std::move(children)) | flex;
}

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
            | color(language_color(lang)));
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

Element render_patch_view(TuiState& /*state*/,
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
                const std::string name = it->path().stem().string();
                const std::string text_content = slurp(it->path());
                std::string patch_status = "?";
                if (text_content.find("\tapplied\t") != std::string::npos) patch_status = "applied";
                else if (text_content.find("\trejected\t") != std::string::npos) patch_status = "rejected";
                else patch_status = "pending";

                Color st_color = patch_status == "applied" ? Color::Green :
                                 patch_status == "rejected" ? Color::Red : Color::Yellow;
                items.push_back(text(" " + patch_status + "  " + name) | color(st_color));
            }
        }
        if (count == 0) {
            items.push_back(text(" No patches yet.") | dim);
        }
    }

    return vbox(std::move(items)) | border | flex;
}

Element render_status_bar(const TuiState& state) {
    auto tabs = text(
        " F1:Search  F2:Chat  F3:Index  F4:Patches  Tab:Next  Esc:Quit"
    ) | dim;

    auto ollama_status = state.ollama_connected
        ? text(" Ollama:ON ") | color(Color::Green)
        : text(" Ollama:OFF ") | color(Color::Red);

    auto tab_names = std::vector<std::string>{"Search", "Chat", "Index", "Patches"};
    auto tab = text(" [" + tab_names[static_cast<int>(state.active_tab)] + "] ") | bold;

    return hbox(Elements{ollama_status, tab, tabs, filler(), text(" ultracode ") | dim})
        | border | size(HEIGHT, EQUAL, 3);
}

Element render_main(TuiState& state,
                    const fs::path& root,
                    const Config& cfg) {
    Element content;
    switch (state.active_tab) {
        case Tab::Search:  content = render_search_view(state, root, cfg); break;
        case Tab::Chat:    content = render_chat_view(state, root, cfg); break;
        case Tab::Index:   content = render_index_view(state, root, cfg); break;
        case Tab::Patches: content = render_patch_view(state, root, cfg); break;
        default:           content = text("Unknown tab") | center; break;
    }

    Elements children;
    children.push_back(content);
    children.push_back(render_status_bar(state));
    return vbox(std::move(children)) | flex;
}

bool handle_tui_input(Event event, TuiState& state) {
    if (event == Event::Escape) {
        return false;
    }
    if (event == Event::F1) {
        state.active_tab = Tab::Search;
        return true;
    }
    if (event == Event::F2) {
        state.active_tab = Tab::Chat;
        return true;
    }
    if (event == Event::F3) {
        state.active_tab = Tab::Index;
        return true;
    }
    if (event == Event::F4) {
        state.active_tab = Tab::Patches;
        return true;
    }
    if (event == Event::Tab) {
        state.active_tab = static_cast<Tab>(
            (static_cast<int>(state.active_tab) + 1) % static_cast<int>(Tab::Count));
        return true;
    }
    if (event == Event::TabReverse) {
        int prev = static_cast<int>(state.active_tab) - 1;
        if (prev < 0) prev = static_cast<int>(Tab::Count) - 1;
        state.active_tab = static_cast<Tab>(prev);
        return true;
    }
    return true;
}

}  // namespace

int run_tui(const fs::path& root, const Config& cfg) {
    TuiState state;
    state.ollama_connected = true;

    auto screen = ScreenInteractive::Fullscreen();
    auto renderer = Renderer([&] {
        return render_main(state, root, cfg);
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (!handle_tui_input(event, state)) {
            screen.ExitLoopClosure()();
            return true;
        }

        if (event == Event::Return && state.active_tab == Tab::Search &&
            !state.search_running) {
            if (!state.search_query.empty()) {
                state.search_running = true;
                state.search_results.clear();
                state.search_error.clear();
                screen.Post([&, root, cfg] {
                    try {
                        const auto diff_ctx = load_repo_diff_context(
                            root, cfg.max_context_chars / 3);
                        state.search_results = retrieve(
                            root, cfg, state.search_query, cfg.top_k,
                            diff_ctx.changed_paths);
                    } catch (const std::exception& e) {
                        state.search_error = e.what();
                    }
                    state.search_running = false;
                });
            }
            return true;
        }

        if (event == Event::Return && state.active_tab == Tab::Chat &&
            !state.chat_streaming && !state.chat_input.empty()) {
            std::string user_msg = state.chat_input;
            state.chat_input.clear();
            state.chat_history.push_back(user_msg);
            state.chat_streaming = true;

            screen.Post([&, root, cfg, user_msg] {
                try {
                    const auto diff_ctx = load_repo_diff_context(
                        root, cfg.max_context_chars / 3);
                    const auto ranked = retrieve(
                        root, cfg, user_msg, cfg.top_k,
                        diff_ctx.changed_paths);
                    const std::string ctx = render_context_blocks_tui(
                        ranked, cfg.max_context_chars);

                    OllamaClient ollama(cfg);
                    std::vector<ChatMessage> messages;
                    messages.push_back(ChatMessage{
                        "system",
                        "You are a local-first coding assistant. Be concise."
                        "Cite file paths and line ranges."
                    });
                    messages.push_back(ChatMessage{
                        "user",
                        "Question:\n" + user_msg +
                        "\n\nRelevant code context:\n" + ctx +
                        (diff_ctx.diff_text.empty() ? ""
                            : "\nWorking tree diff:\n" + diff_ctx.diff_text)
                    });

                    std::ostringstream captured;
                    const auto response = ollama.chat_stream(
                        messages, captured);
                    state.chat_history.push_back(response);
                } catch (const std::exception& e) {
                    state.chat_history.push_back(
                        std::string("error: ") + e.what());
                }
                state.chat_streaming = false;
            });
            return true;
        }

        if (event.is_character() && state.active_tab == Tab::Search) {
            state.search_query += event.character();
            return true;
        }
        if (event == Event::Backspace && state.active_tab == Tab::Search) {
            if (!state.search_query.empty()) {
                state.search_query.pop_back();
            }
            return true;
        }

        if (event.is_character() && state.active_tab == Tab::Chat) {
            state.chat_input += event.character();
            return true;
        }
        if (event == Event::Backspace && state.active_tab == Tab::Chat) {
            if (!state.chat_input.empty()) {
                state.chat_input.pop_back();
            }
            return true;
        }

        return true;
    });

    screen.Loop(component);
    return 0;
}

} // namespace tui
} // namespace ultracode
