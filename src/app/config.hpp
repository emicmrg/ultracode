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

#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class TaskType { Chat, Plan, Develop };

struct Config {
    std::string ollama_url          = "http://127.0.0.1:11434";
    std::string embedding_model     = "nomic-embed-text";
    std::string chat_model          = "qwen2.5-coder:3b";
    std::string plan_model          = "gemma4-more-context:e4b";
    std::string develop_model       = "gemma4-more-context:e4b";
    int top_k                       = 8;
    int max_context_chars           = 24000;
    int embedding_batch_size        = 16;
    int fallback_embedding_dim      = 256;
    std::vector<std::string> ignore_dirs = {
        ".git", ".ultracode", "build", "dist", "target", "node_modules",
        ".next", ".venv", "venv", "__pycache__", ".idea", ".vscode"
    };
};

std::string resolve_model(const Config& cfg, TaskType task);
std::string default_config_json();
Config load_config(const std::filesystem::path& root);
