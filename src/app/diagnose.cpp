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

#include "app/diagnose.hpp"

#include "llm/ollama_client.hpp"
#include "support/utils.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace {

double dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        sum += static_cast<double>(a[i]) * b[i];
    return sum;
}

double norm(const std::vector<float>& v) {
    double s = 0.0;
    for (float x : v) s += static_cast<double>(x) * x;
    return std::sqrt(s);
}

double cosine(const std::vector<float>& a, const std::vector<float>& b) {
    const double n = norm(a) * norm(b);
    if (n < 1e-12) return 0.0;
    return dot_product(a, b) / n;
}

bool check_ollama_reachable(const std::string& url) {
    const std::string cmd = "curl -sS --max-time 5 -o /dev/null -w '%{http_code}' "
            + shell_quote(url) + " 2>/dev/null";
    const std::string result = run_command_capture(cmd);
    return trim(result) == "200";
}

bool check_model_available(const std::string& url, const std::string& model) {
    const std::string cmd = "curl -sS --max-time 5 " +
            shell_quote(url + "/api/show") +
            " -d '{\"model\":\"" + json_escape(model) + "\"}' 2>/dev/null";
    const std::string result = run_command_capture(cmd);
    return result.find("\"error\"") == std::string::npos && !result.empty();
}

}  // namespace

DiagnoseResult run_diagnose(const std::filesystem::path& /*root*/, const Config& cfg) {
    DiagnoseResult r;
    r.ollama_url   = cfg.ollama_url;
    r.embed_model  = cfg.embedding_model;
    r.chat_model   = cfg.chat_model;
    r.embed_dim_fallback = cfg.fallback_embedding_dim;

    r.ollama_reachable = check_ollama_reachable(cfg.ollama_url);
    if (!r.ollama_reachable) {
        r.error = "Ollama not reachable at " + cfg.ollama_url;
        return r;
    }

    r.embed_model_ok = check_model_available(cfg.ollama_url, cfg.embedding_model);
    r.chat_model_ok  = check_model_available(cfg.ollama_url, cfg.chat_model);

    const std::string test_text = "authentication token validation session";

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<float> vec_ollama;
    if (r.embed_model_ok) {
        const OllamaClient ollama(cfg);
        vec_ollama = ollama.embed(test_text);
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto t2 = std::chrono::steady_clock::now();
    const auto vec_fallback = hashed_embedding(test_text, cfg.fallback_embedding_dim);
    const auto t3 = std::chrono::steady_clock::now();

    r.embed_time_ollama_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.embed_time_fallback_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    r.embed_dim_ollama = vec_ollama.empty() ? 0 : static_cast<int>(vec_ollama.size());

    if (!vec_ollama.empty() && !vec_fallback.empty()) {
        const int min_dim = std::min(static_cast<int>(vec_ollama.size()),
                                     static_cast<int>(vec_fallback.size()));
        std::vector<float> o(vec_ollama.begin(), vec_ollama.begin() + min_dim);
        std::vector<float> f(vec_fallback.begin(), vec_fallback.begin() + min_dim);
        r.cosine_sim = cosine(o, f);
    }

    if (!r.embed_model_ok) {
        r.error = "Embedding model '" + cfg.embedding_model + "' not available";
    }

    return r;
}

void print_diagnose(const DiagnoseResult& r) {
    auto status = [](bool ok) -> const char* {
        return ok ? "\033[32mOK\033[0m" : "\033[31mFAIL\033[0m";
    };

    std::cout << "Ollama URL:        " << r.ollama_url << "  " << status(r.ollama_reachable) << '\n';
    std::cout << "Embedding model:   " << r.embed_model << "  " << status(r.embed_model_ok) << '\n';
    std::cout << "Chat model:        " << r.chat_model << "  " << status(r.chat_model_ok) << '\n';
    std::cout << "Embedding dim:     " << r.embed_dim_ollama << '\n';
    std::cout << "Fallback dim:      " << r.embed_dim_fallback << '\n';
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Embed time:        " << r.embed_time_ollama_ms << " ms (Ollama)  "
              << r.embed_time_fallback_ms << " ms (fallback)\n";
    std::cout << std::fixed << std::setprecision(4);
    if (r.embed_dim_ollama > 0) {
        std::cout << "Cosine sim:        " << r.cosine_sim
                  << (r.cosine_sim < 0.01 ? "  \033[33m[WARN: Ollama and fallback vectors are nearly orthogonal]\033[0m" : "")
                  << '\n';
    }
    if (!r.error.empty()) {
        std::cout << "Diagnose error:    \033[31m" << r.error << "\033[0m\n";
    }

    std::cout << '\n';
    if (!r.ollama_reachable) {
        std::cout << "Run: ollama serve\n";
    } else if (!r.embed_model_ok) {
        std::cout << "Run: ollama pull " << r.embed_model << '\n';
    }
    if (!r.chat_model_ok) {
        std::cout << "Run: ollama pull " << r.chat_model << '\n';
    }
}
