#include "config.hpp"
#include "utils.hpp"

#include <regex>
#include <string>

std::string default_config_json() {
    return R"JSON({
  "ollama_url": "http://127.0.0.1:11434",
  "embedding_model": "nomic-embed-text",
  "chat_model": "qwen2.5-coder:3b",
  "top_k": 8,
  "max_context_chars": 24000,
  "fallback_embedding_dim": 256,
  "notes": "This MVP uses Ollama /api/embed and /api/chat. If Ollama is unavailable, indexing falls back to local hashed embeddings."
}
)JSON";
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
    if (auto v = int_value("top_k"))                  cfg.top_k               = *v;
    if (auto v = int_value("max_context_chars"))      cfg.max_context_chars   = *v;
    if (auto v = int_value("fallback_embedding_dim")) cfg.fallback_embedding_dim = *v;
    return cfg;
}
