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

#include "vcs/git_diff.hpp"

#include "support/utils.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace {

void collect_paths_from_diff(const std::string& diff_text,
                             std::set<std::string>& changed_paths) {
    std::stringstream ss(diff_text);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.rfind("+++ b/", 0) == 0) {
            changed_paths.insert(line.substr(6));
        }
    }
}

std::string trim_diff_text(std::string diff_text, int max_chars) {
    if (static_cast<int>(diff_text.size()) > max_chars) {
        diff_text.resize(static_cast<size_t>(max_chars));
    }
    return diff_text;
}

}  // namespace

RepoDiffContext load_repo_diff_context(const std::filesystem::path& root, int max_chars) {
    RepoDiffContext context;
    const std::string base = "git -C " + shell_quote(root.string()) + " ";
    const std::string worktree_diff =
        run_command_capture(base + "diff --no-ext-diff --relative 2>/dev/null");
    const std::string staged_diff =
        run_command_capture(base + "diff --cached --no-ext-diff --relative 2>/dev/null");

    if (!worktree_diff.empty()) {
        context.diff_text += worktree_diff;
    }
    if (!staged_diff.empty()) {
        if (!context.diff_text.empty()) {
            context.diff_text += "\n";
        }
        context.diff_text += staged_diff;
    }

    collect_paths_from_diff(context.diff_text, context.changed_paths);
    context.diff_text = trim_diff_text(context.diff_text, max_chars);
    return context;
}
