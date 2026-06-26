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
