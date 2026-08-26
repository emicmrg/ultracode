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

#include "llm/ollama_client.hpp"

#include "support/utils.hpp"
#include "support/json.hpp"

#include <cstdio>
#include <ostream>
#include <sstream>
#include <utility>
#include <iostream>

using json = nlohmann::json;

namespace {

std::string build_chat_payload(const std::vector<ChatMessage>& messages,
                               const std::string& model_name,
                               bool stream) {
    json payload;
    payload["model"] = model_name;
    payload["stream"] = stream;
    payload["keep_alive"] = "10m";
    payload["messages"] = json::array();
    
    for (const auto& msg : messages) {
        payload["messages"].push_back({
            {"role", msg.role},
            {"content", msg.content}
        });
    }
    return payload.dump();
}

std::string resolve_model_name(const Config& config, TaskType task, const std::string& override_name) {
    if (!override_name.empty()) return override_name;
    return resolve_model(config, task);
}

}  // namespace

OllamaClient::OllamaClient(Config config) : config_(std::move(config)) {}

std::vector<float> OllamaClient::embed(const std::string& input) const {
    auto embeddings = embed_batch({input});
    if (!embeddings.empty()) {
        return embeddings.front();
    }
    return {};
}

std::vector<std::vector<float>> OllamaClient::embed_batch(
    const std::vector<std::string>& inputs) const {
    if (inputs.empty()) {
        return {};
    }

    json payload;
    payload["model"] = config_.embedding_model;
    payload["input"] = inputs;
    payload["keep_alive"] = "1m";

    const std::string cmd =
        "curl -sS --max-time 180 -H 'Content-Type: application/json' " +
        shell_quote(config_.ollama_url + "/api/embed") +
        " -d " + shell_quote(payload.dump()) + " 2>/dev/null";
        
    std::string response = run_command_capture(cmd);
    std::vector<std::vector<float>> embeddings;
    
    try {
        auto j = json::parse(response);
        if (j.contains("embeddings") && j["embeddings"].is_array()) {
            embeddings = j["embeddings"].get<std::vector<std::vector<float>>>();
        }
    } catch (...) {}
    
    if (embeddings.size() == inputs.size()) {
        return embeddings;
    }

    // Fallback: 1 by 1
    embeddings.clear();
    embeddings.reserve(inputs.size());
    for (const auto& input : inputs) {
        json single_payload;
        single_payload["model"] = config_.embedding_model;
        single_payload["input"] = input;
        single_payload["keep_alive"] = "1m";

        const std::string single_cmd =
            "curl -sS --max-time 180 -H 'Content-Type: application/json' " +
            shell_quote(config_.ollama_url + "/api/embed") +
            " -d " + shell_quote(single_payload.dump()) + " 2>/dev/null";
            
        std::string single_resp = run_command_capture(single_cmd);
        try {
            auto j = json::parse(single_resp);
            if (j.contains("embeddings") && j["embeddings"].is_array() && !j["embeddings"].empty()) {
                embeddings.push_back(j["embeddings"][0].get<std::vector<float>>());
            } else {
                embeddings.push_back({});
            }
        } catch (...) {
            embeddings.push_back({});
        }
    }
    return embeddings;
}

std::string OllamaClient::chat(const std::string& system,
                                const std::string& user) const {
    return chat({ChatMessage{"system", system}, ChatMessage{"user", user}});
}

std::string OllamaClient::chat(const std::vector<ChatMessage>& messages,
                                const std::string& model_override) const {
    return chat(messages, TaskType::Chat, model_override);
}

std::string OllamaClient::chat(const std::vector<ChatMessage>& messages,
                                TaskType task,
                                const std::string& model_override) const {
    const std::string payload = build_chat_payload(
        messages, resolve_model_name(config_, task, model_override), false);
        
    const std::string cmd =
        "curl -sS -H 'Content-Type: application/json' " +
        shell_quote(config_.ollama_url + "/api/chat") +
        " -d " + shell_quote(payload) + " 2>/dev/null";
        
    const std::string response = run_command_capture(cmd);
    
    try {
        auto j = json::parse(response);
        if (j.contains("message") && j["message"].contains("content")) {
            return j["message"]["content"].get<std::string>();
        }
    } catch (...) {}
    
    return response;
}

std::string OllamaClient::chat_stream(const std::vector<ChatMessage>& messages,
                                      std::ostream& out,
                                      const std::string& model_override) const {
    return chat_stream(messages, out, TaskType::Chat, model_override);
}

std::string OllamaClient::chat_stream(const std::vector<ChatMessage>& messages,
                                      std::ostream& out,
                                      TaskType task,
                                      const std::string& model_override) const {
    const std::string payload = build_chat_payload(
        messages, resolve_model_name(config_, task, model_override), true);
        
    const std::string cmd =
        "curl -sS -N -H 'Content-Type: application/json' " +
        shell_quote(config_.ollama_url + "/api/chat") +
        " -d " + shell_quote(payload) + " 2>/dev/null";

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        return {};
    }

    std::array<char, 4096> buffer{};
    std::string full_response;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        const std::string line(buffer.data());
        try {
            auto j = json::parse(line);
            if (j.contains("message") && j["message"].contains("content")) {
                std::string content = j["message"]["content"].get<std::string>();
                if (!content.empty()) {
                    out << content;
                    out.flush();
                    full_response += content;
                }
            }
        } catch (...) {
            // Partial JSON or other output, ignore and continue
        }
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return full_response;
}
