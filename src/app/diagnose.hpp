#pragma once

#include "app/config.hpp"

#include <filesystem>
#include <string>

struct DiagnoseResult {
    bool ollama_reachable = false;
    bool embed_model_ok = false;
    bool chat_model_ok = false;
    std::string ollama_url;
    std::string embed_model;
    std::string chat_model;
    int embed_dim_ollama = 0;
    int embed_dim_fallback = 0;
    double embed_time_ollama_ms = 0.0;
    double embed_time_fallback_ms = 0.0;
    double cosine_sim = 0.0;
    std::string error;
};

DiagnoseResult run_diagnose(const std::filesystem::path& root, const Config& cfg);

void print_diagnose(const DiagnoseResult& result);
