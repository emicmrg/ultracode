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

#include "session/chat_session.hpp"

#include "llm/ollama_client.hpp"
#include "retrieval/retriever.hpp"
#include "support/utils.hpp"
#include "vcs/git_diff.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct SessionState {
    std::string model_override;
    std::vector<ChatMessage> history;
    std::vector<std::string> last_sources;
};

std::string render_context_blocks(const std::vector<RankedChunk>& ranked,
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

void trim_history(SessionState& state) {
    constexpr size_t kMaxMessages = 6;
    if (state.history.size() > kMaxMessages) {
        state.history.erase(state.history.begin(),
                            state.history.begin() + static_cast<long>(state.history.size() - kMaxMessages));
    }
}

bool handle_meta_command(SessionState& state,
                         const std::string& line,
                         std::ostream& out) {
    if (line == "/clear") {
        state.history.clear();
        state.last_sources.clear();
        out << "Session cleared.\n";
        return true;
    }
    if (line == "/context") {
        out << "Active context:\n";
        if (state.last_sources.empty()) {
            out << "  none\n";
        } else {
            for (const auto& source : state.last_sources) {
                out << "  " << source << '\n';
            }
        }
        return true;
    }
    if (line.rfind("/model", 0) == 0) {
        const std::string model = trim(line.substr(std::string("/model").size()));
        if (model.empty()) {
            out << "Current session model: "
                << (state.model_override.empty() ? "<default>" : state.model_override) << '\n';
        } else {
            state.model_override = model;
            out << "Session model set to: " << state.model_override << '\n';
        }
        return true;
    }
    return false;
}

}  // namespace

int run_interactive_chat(const std::filesystem::path& root,
                         const Config& cfg,
                         std::istream& in,
                         std::ostream& out) {
    SessionState state;
    OllamaClient ollama(cfg);

    out << "Entering chat mode. Use /context, /model <name>, /clear, /exit.\n";

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }
        if (line == "/exit") {
            out << "Leaving chat mode.\n";
            return 0;
        }
        if (handle_meta_command(state, line, out)) {
            continue;
        }

        const auto diff_context = load_repo_diff_context(root, cfg.max_context_chars / 3);
        const auto ranked = retrieve(root, cfg, line, cfg.top_k, diff_context.changed_paths);
        state.last_sources.clear();
        for (const auto& r : ranked) {
            state.last_sources.push_back(
                r.chunk.path + ":" + std::to_string(r.chunk.start_line) + "-" +
                std::to_string(r.chunk.end_line) + " " + r.chunk.symbol);
        }

        std::vector<ChatMessage> messages;
        messages.push_back(ChatMessage{
            "system",
            "You are a local-first coding assistant. Use the provided repository context. "
            "Cite file paths and line ranges when possible."
        });
        for (const auto& message : state.history) {
            messages.push_back(message);
        }

        const std::string context = render_context_blocks(ranked, cfg.max_context_chars);
        messages.push_back(ChatMessage{
            "user",
            "Question:\n" + line +
            "\n\nRelevant code context:\n" + context +
            (diff_context.diff_text.empty() ? "" : "\nWorking tree diff:\n" + diff_context.diff_text)
        });

        out << "> ";
        const std::string response = ollama.chat_stream(messages, out, state.model_override);
        out << '\n';

        state.history.push_back(ChatMessage{"user", line});
        state.history.push_back(ChatMessage{"assistant", response});
        trim_history(state);
    }

    out << "Leaving chat mode.\n";
    return 0;
}
