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

#include <cstdio>
#include <ostream>
#include <sstream>
#include <utility>

namespace {

std::string build_chat_payload(const std::vector<ChatMessage>& messages,
                               const std::string& model_name,
                               bool stream) {
    std::ostringstream payload;
    payload << "{\"model\":\"" << json_escape(model_name)
            << "\",\"stream\":" << (stream ? "true" : "false")
            << ",\"keep_alive\":\"10m\",\"messages\":[";
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i) {
            payload << ',';
        }
        payload << "{\"role\":\"" << json_escape(messages[i].role)
                << "\",\"content\":\"" << json_escape(messages[i].content) << "\"}";
    }
    payload << "]}";
    return payload.str();
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

    std::ostringstream payload;
    payload << "{\"model\":\"" << json_escape(config_.embedding_model)
            << "\",\"input\":[";
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (i) {
            payload << ',';
        }
        payload << "\"" << json_escape(inputs[i]) << "\"";
    }
    payload << "],\"keep_alive\":\"1m\"}";

    const std::string cmd =
        "curl -sS --max-time 180 -H 'Content-Type: application/json' " +
        shell_quote(config_.ollama_url + "/api/embed") +
        " -d " + shell_quote(payload.str()) + " 2>/dev/null";
    auto embeddings = parse_embeddings(run_command_capture(cmd));
    if (embeddings.size() == inputs.size()) {
        return embeddings;
    }

    embeddings.clear();
    embeddings.reserve(inputs.size());
    for (const auto& input : inputs) {
        const std::string single_payload =
            "{\"model\":\"" + json_escape(config_.embedding_model) +
            "\",\"input\":\"" + json_escape(input) +
            "\",\"keep_alive\":\"1m\"}";
        const std::string single_cmd =
            "curl -sS --max-time 180 -H 'Content-Type: application/json' " +
            shell_quote(config_.ollama_url + "/api/embed") +
            " -d " + shell_quote(single_payload) + " 2>/dev/null";
        embeddings.push_back(parse_first_embedding(run_command_capture(single_cmd)));
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
        "curl -sS --max-time 300 -H 'Content-Type: application/json' " +
        shell_quote(config_.ollama_url + "/api/chat") +
        " -d " + shell_quote(payload) + " 2>/dev/null";
    const std::string response = run_command_capture(cmd);
    auto content = extract_json_string_after_key(response, "\"content\"");
    return content.value_or(response);
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
        "curl -sS -N --max-time 300 -H 'Content-Type: application/json' " +
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
        auto content = extract_json_string_after_key(line, "\"content\"");
        if (!content.has_value() || content->empty()) {
            continue;
        }
        out << *content;
        out.flush();
        full_response += *content;
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return full_response;
}
