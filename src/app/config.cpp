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

#include "app/config.hpp"
#include "support/utils.hpp"

#include <regex>
#include <string>

std::string default_config_json() {
    return R"JSON({
  "ollama_url": "http://127.0.0.1:11434",
  "embedding_model": "nomic-embed-text",
  "chat_model": "qwen2.5-coder:3b",
  "plan_model": "gemma4-more-context:e4b",
  "develop_model": "gemma4-more-context:e4b",
  "top_k": 8,
  "max_context_chars": 24000,
  "embedding_batch_size": 16,
  "fallback_embedding_dim": 256,
  "notes": "This MVP uses Ollama /api/embed and /api/chat. If Ollama is unavailable, indexing falls back to local hashed embeddings."
}
)JSON";
}

std::string resolve_model(const Config& cfg, TaskType task) {
    switch (task) {
        case TaskType::Plan:    return cfg.plan_model.empty()    ? cfg.chat_model : cfg.plan_model;
        case TaskType::Develop: return cfg.develop_model.empty() ? cfg.chat_model : cfg.develop_model;
        default:                return cfg.chat_model;
    }
}

Config load_config(const fs::path& root) {
    Config cfg;
    const std::string text = slurp(root / ".ultracode" / "config.json");

    auto string_value = [&](const std::string& key) -> std::optional<std::string> {
        std::regex r("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
        std::smatch m;
        if (std::regex_search(text, m, r)) return m[1].str();
        return std::nullopt;
    };
    auto int_value = [&](const std::string& key) -> std::optional<int> {
        std::regex r("\\\"" + key + "\\\"\\s*:\\s*([0-9]+)");
        std::smatch m;
        if (std::regex_search(text, m, r)) return std::stoi(m[1].str());
        return std::nullopt;
    };

    if (auto v = string_value("ollama_url"))          cfg.ollama_url          = *v;
    if (auto v = string_value("embedding_model"))     cfg.embedding_model     = *v;
    if (auto v = string_value("chat_model"))          cfg.chat_model          = *v;
    if (auto v = string_value("plan_model"))          cfg.plan_model          = *v;
    if (auto v = string_value("develop_model"))       cfg.develop_model       = *v;
    if (auto v = int_value("top_k"))                  cfg.top_k               = *v;
    if (auto v = int_value("max_context_chars"))      cfg.max_context_chars   = *v;
    if (auto v = int_value("embedding_batch_size"))   cfg.embedding_batch_size = *v;
    if (auto v = int_value("fallback_embedding_dim")) cfg.fallback_embedding_dim = *v;

    if (cfg.plan_model.empty())    cfg.plan_model    = cfg.chat_model;
    if (cfg.develop_model.empty()) cfg.develop_model = cfg.chat_model;

    return cfg;
}
