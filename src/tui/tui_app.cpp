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

#include "tui/tui_app.hpp"

#include "tui/components/status_bar.hpp"
#include "tui/views/context.hpp"
#include "tui/views/chat.hpp"
#include "tui/views/index.hpp"
#include "tui/views/patches.hpp"

#include "app/config.hpp"
#include "edit/patch_workflow.hpp"
#include "llm/ollama_client.hpp"
#include "retrieval/retriever.hpp"
#include "support/utils.hpp"
#include "vcs/git_diff.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace ftxui;

namespace ultracode {
namespace tui {

namespace {

std::string build_chat_system_prompt() {
    return
        "You are a local-first coding assistant. Answer using ONLY the provided "
        "code context. For every claim, cite at least one file path and line "
        "range: path/to/file.ext:10-25. If the context is insufficient, state "
        "what is missing instead of guessing. Be concise.";
}

std::string build_patch_system_prompt() {
    return
        "You are a local coding assistant. Return only a complete unified diff "
        "patch. Do not include markdown fences, prose, bullets, explanations, "
        "or code outside the patch. Every file change must include diff --git, "
        "---, +++, and @@ hunk headers.";
}

std::string context_blocks_xml(const std::vector<RankedChunk>& ranked,
                                int max_chars) {
    std::ostringstream ctx;
    int used = 0;
    for (const auto& r : ranked) {
        std::ostringstream block;
        block << "<chunk path=\"" << r.chunk.path << "\" lines=\""
              << r.chunk.start_line << "-" << r.chunk.end_line
              << "\" symbol=\"" << r.chunk.symbol << "\">\n"
              << r.content << "\n</chunk>\n\n";
        const std::string rendered = block.str();
        if (used + static_cast<int>(rendered.size()) > max_chars) {
            break;
        }
        ctx << rendered;
        used += static_cast<int>(rendered.size());
    }
    return ctx.str();
}

Element render_main(TuiState& state,
                    const fs::path& root,
                    const Config& cfg,
                    Element chat_input_element) {
    Element content;
    switch (state.active_tab) {
        case Tab::Context: content = render_context_view(state, root, cfg); break;
        case Tab::Chat:    content = render_chat_view(state, root, cfg, chat_input_element); break;
        case Tab::Index:   content = render_index_view(state, root, cfg);   break;
        case Tab::Patches: content = render_patches_view(state, root, cfg); break;
        default:           content = text("Unknown tab") | center; break;
    }

    Elements children;
    children.push_back(content);
    children.push_back(render_status_bar(state));
    return vbox(std::move(children)) | flex;
}

bool handle_global_keys(Event event, TuiState& state) {
    if (event == Event::F1) { state.active_tab = Tab::Context;  return true; }
    if (event == Event::F2) { state.active_tab = Tab::Chat;     return true; }
    if (event == Event::F3) { state.active_tab = Tab::Index;    return true; }
    if (event == Event::F4) { state.active_tab = Tab::Patches;  return true; }
    if (event == Event::Tab) {
        int next = (static_cast<int>(state.active_tab) + 1) % static_cast<int>(Tab::Count);
        state.active_tab = static_cast<Tab>(next);
        return true;
    }
    if (event == Event::TabReverse) {
        int prev = static_cast<int>(state.active_tab) - 1;
        if (prev < 0) prev = static_cast<int>(Tab::Count) - 1;
        state.active_tab = static_cast<Tab>(prev);
        return true;
    }
    return false;
}

}  // namespace

int run_tui(const fs::path& root, const Config& cfg) {
    TuiState state;
    state.ollama_connected = true;

    auto chat_input_component = Input(&state.chat_input, "Type message and press Enter...");

    auto screen = ScreenInteractive::Fullscreen();
    auto renderer = Renderer([&] {
        auto input_el = chat_input_component->Render()
            | border | size(HEIGHT, EQUAL, 3);
        if (state.chat_streaming) {
            input_el = input_el | dim;
        }
        return render_main(state, root, cfg, input_el);
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event.is_mouse()) {
            if (state.active_tab == Tab::Chat) {
                auto& m = event.mouse();
                if (m.button == Mouse::WheelUp)
                    state.chat_scroll = std::clamp(state.chat_scroll - 0.05f, 0.0f, 1.0f);
                else if (m.button == Mouse::WheelDown)
                    state.chat_scroll = std::clamp(state.chat_scroll + 0.05f, 0.0f, 1.0f);
            }
            return true;
        }

        // Handle confirmation dialog first (highest priority)
        if (state.patches_confirm_visible) {
            if (event == Event::Character('y') || event == Event::Character('Y')) {
                state.patches_error.clear();
                if (!state.patch_ids.empty() &&
                    state.patches_selected_index >= 0 &&
                    state.patches_selected_index < static_cast<int>(state.patch_ids.size())) {

                    const std::string pid = state.patch_ids[static_cast<size_t>(state.patches_selected_index)];
                    std::string err;
                    bool ok = false;
                    if (state.patches_confirm_action == "apply") {
                        ok = apply_patch_proposal(root, pid, &err);
                    } else if (state.patches_confirm_action == "reject") {
                        ok = reject_patch_proposal(root, pid, &err);
                    }
                    if (ok) {
                        state.patches_confirm_visible = false;
                        state.patches_confirm_action.clear();
                        screen.Post(Event::Custom);
                    } else {
                        state.patches_error = err;
                    }
                } else {
                    state.patches_error = "no patch selected";
                }
                return true;
            }
            if (event == Event::Character('n') || event == Event::Character('N')) {
                state.patches_confirm_visible = false;
                state.patches_confirm_action.clear();
                state.patches_error.clear();
                return true;
            }
            if (event == Event::Escape) {
                state.patches_confirm_visible = false;
                state.patches_confirm_action.clear();
                state.patches_error.clear();
                return true;
            }
            return true;
        }

        // Escape to quit TUI
        if (event == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }

        const bool handled = handle_global_keys(event, state);
        if (handled) {
            return true;
        }

        if (state.active_tab == Tab::Chat) {
            if (event == Event::Return && !state.chat_streaming &&
                !state.chat_input.empty()) {
                std::string user_msg = state.chat_input;
                state.chat_input.clear();

                // Handle /model command
                {
                    const std::string prefix6 = user_msg.size() >= 6 ? lower(user_msg.substr(0, 6)) : "";
                    if (prefix6 == "/model") {
                        std::string model = trim(user_msg.substr(6));
                        if (model.empty()) {
                            state.chat_history.push_back(
                                "model: " + (state.chat_model_override.empty()
                                    ? cfg.chat_model + " (default)"
                                    : state.chat_model_override));
                        } else {
                            state.chat_model_override = model;
                            state.chat_history.push_back("model set to: " + model);
                        }
                        state.chat_scroll = 1.0f;
                        screen.Post(Event::Custom);
                        return true;
                    }
                }

                bool is_patch = false;
                std::string patch_instruction;
                {
                    const std::string prefix7 = user_msg.size() >= 7 ? lower(user_msg.substr(0, 7)) : "";
                    if (prefix7 == "/patch ") {
                        is_patch = true;
                        patch_instruction = trim(user_msg.substr(7));
                    }
                }

                if (is_patch) {
                    std::string instruction = patch_instruction;
                    state.chat_history.push_back("/patch " + instruction);
                    state.chat_history.push_back("");
                    state.chat_scroll = 1.0f;
                    state.chat_streaming = true;
                    screen.Post(Event::Custom);

                    std::thread([&, root, cfg, instruction] {
                        try {
                            const auto diff_ctx = load_repo_diff_context(
                                root, cfg.max_context_chars / 3);
                            const auto ranked = retrieve(
                                root, cfg, instruction, cfg.top_k,
                                diff_ctx.changed_paths);
                            const std::string ctx = context_blocks_xml(
                                ranked, cfg.max_context_chars);

                            OllamaClient ollama(cfg);
                            std::string user = "Requested change:\n" + instruction +
                                "\n\nRelevant code context:\n" + ctx +
                                "\n\nPatch requirements:\n"
                                "- Modify only files that are necessary.\n"
                                "- Preserve existing coding style.\n"
                                "- Return only a valid unified diff.\n";
                            if (!diff_ctx.diff_text.empty()) {
                                user += "\nWorking tree diff:\n" + diff_ctx.diff_text;
                            }

                            const std::string raw = ollama.chat(
                                std::vector<ChatMessage>{
                                    ChatMessage{"system", build_patch_system_prompt()},
                                    ChatMessage{"user", user}
                                },
                                TaskType::Develop);
                            const std::string diff = sanitize_patch_text(raw);
                            std::string patch_error;
                            if (!state.chat_history.empty()) {
                                state.chat_history.pop_back();
                            }
                            if (!validate_patch_text(root, diff, &patch_error)) {
                                state.chat_history.push_back(
                                    "patch error: " + patch_error);
                                const int trunc = std::min(400, static_cast<int>(raw.size()));
                                state.chat_history.push_back(
                                    "raw output: " + raw.substr(0, static_cast<size_t>(trunc)) +
                                    (static_cast<int>(raw.size()) > trunc ? "..." : ""));
                                const std::string dbg_name =
                                    "patch-fail-" + fnv1a_hex(instruction).substr(0, 8) + ".txt";
                                const fs::path dbg_dir = root / ".ultracode" / "debug";
                                fs::create_directories(dbg_dir);
                                write_text(dbg_dir / dbg_name,
                                    "INSTRUCTION:\n" + instruction +
                                    "\n\nSYSTEM:\n" + build_patch_system_prompt() +
                                    "\n\nUSER:\n" + user +
                                    "\n\nRAW OUTPUT:\n" + raw +
                                    "\n\nSANITIZED:\n" + diff +
                                    "\n\nVALIDATION ERROR:\n" + patch_error);
                                state.chat_history.push_back(
                                    "debug: .ultracode/debug/" + dbg_name);
                            } else {
                                auto targets = extract_patch_target_paths(diff);
                                auto prop = save_patch_proposal(
                                    root, instruction, diff, targets);
                                state.chat_history.push_back(
                                    "patch saved: " + prop.id +
                                    " (" + std::to_string(targets.size()) +
                                    " file(s))");
                            }
                        } catch (const std::exception& e) {
                            if (!state.chat_history.empty()) {
                                state.chat_history.pop_back();
                            }
                            state.chat_history.push_back(
                                std::string("error: ") + e.what());
                        }
                        state.chat_scroll = 1.0f;
                        state.chat_streaming = false;
                        screen.Post(Event::Custom);
                    }).detach();
                    return true;
                }

                state.chat_history.push_back(user_msg);
                state.chat_history.push_back("");
                state.chat_scroll = 1.0f;
                state.chat_streaming = true;
                screen.Post(Event::Custom);

                std::thread([&, root, cfg, user_msg] {
                    try {
                        const auto diff_ctx = load_repo_diff_context(
                            root, cfg.max_context_chars / 3);
                        const auto ranked = retrieve(
                            root, cfg, user_msg, cfg.top_k,
                            diff_ctx.changed_paths);
                        const std::string ctx = context_blocks_xml(
                            ranked, cfg.max_context_chars);

                        OllamaClient ollama(cfg);
                        std::vector<ChatMessage> messages;
                        messages.push_back(ChatMessage{
                            "system", build_chat_system_prompt()});
                        messages.push_back(ChatMessage{
                            "user",
                            "Question:\n" + user_msg +
                            "\n\nRelevant code context:\n" + ctx +
                            (diff_ctx.diff_text.empty() ? ""
                                : "\nWorking tree diff:\n" + diff_ctx.diff_text)
                        });

                        std::ostringstream captured;
                        const auto response = ollama.chat_stream(
                            messages, captured, TaskType::Chat, state.chat_model_override);
                        if (!state.chat_history.empty()) {
                            state.chat_history.pop_back();
                        }
                        state.chat_history.push_back(response);
                        state.last_retrieved_chunks = ranked;
                    } catch (const std::exception& e) {
                        if (!state.chat_history.empty()) {
                            state.chat_history.pop_back();
                        }
                        state.chat_history.push_back(
                            std::string("error: ") + e.what());
                    }
                    state.chat_scroll = 1.0f;
                    state.chat_streaming = false;
                    screen.Post(Event::Custom);
                }).detach();
                return true;
            }

            if (event.is_character()) {
                state.chat_input += event.character();
                return true;
            }
            if (event == Event::Backspace && !state.chat_input.empty()) {
                state.chat_input.pop_back();
                return true;
            }
            if (event == Event::ArrowUp) {
                state.chat_scroll = std::clamp(state.chat_scroll - 0.05f, 0.0f, 1.0f);
                return true;
            }
            if (event == Event::ArrowDown) {
                state.chat_scroll = std::clamp(state.chat_scroll + 0.05f, 0.0f, 1.0f);
                return true;
            }
            if (event == Event::PageUp) {
                state.chat_scroll = std::clamp(state.chat_scroll - 0.2f, 0.0f, 1.0f);
                return true;
            }
            if (event == Event::PageDown) {
                state.chat_scroll = std::clamp(state.chat_scroll + 0.2f, 0.0f, 1.0f);
                return true;
            }
        }

        if (state.active_tab == Tab::Patches) {
            if (event == Event::ArrowUp || event == Event::ArrowLeft) {
                if (state.patches_selected_index > 0)
                    state.patches_selected_index--;
                return true;
            }
            if (event == Event::ArrowDown || event == Event::ArrowRight) {
                state.patches_selected_index++;
                return true;
            }
            if (event == Event::Character('a')) {
                state.patches_confirm_visible = true;
                state.patches_confirm_action = "apply";
                return true;
            }
            if (event == Event::Character('r')) {
                state.patches_confirm_visible = true;
                state.patches_confirm_action = "reject";
                return true;
            }
        }

        if (state.active_tab == Tab::Context) {
            if (event == Event::ArrowDown) {
                if (state.selected_index < static_cast<int>(
                        state.last_retrieved_chunks.size()) - 1)
                    state.selected_index++;
                return true;
            }
            if (event == Event::ArrowUp) {
                if (state.selected_index > 0)
                    state.selected_index--;
                return true;
            }
        }

        return true;
    });

    screen.Loop(component);
    return 0;
}

} // namespace tui
} // namespace ultracode
