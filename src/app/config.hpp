#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct Config {
    std::string ollama_url          = "http://127.0.0.1:11434";
    std::string embedding_model     = "nomic-embed-text";
    std::string chat_model          = "qwen2.5-coder:3b";
    int top_k                       = 8;
    int max_context_chars           = 24000;
    int fallback_embedding_dim      = 256;
    std::vector<std::string> ignore_dirs = {
        ".git", ".ultracode", "build", "dist", "target", "node_modules",
        ".next", ".venv", "venv", "__pycache__", ".idea", ".vscode"
    };
};

std::string default_config_json();
Config load_config(const std::filesystem::path& root);
