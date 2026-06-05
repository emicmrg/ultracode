#include "llm/ollama_client.hpp"

#include "support/utils.hpp"

#include <sstream>
#include <utility>

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
    const std::string payload =
        "{\"model\":\"" + json_escape(config_.chat_model) +
        "\",\"stream\":false,\"keep_alive\":\"10m\",\"messages\":[" +
        "{\"role\":\"system\",\"content\":\"" + json_escape(system) + "\"}," +
        "{\"role\":\"user\",\"content\":\"" + json_escape(user) + "\"}]}";
    const std::string cmd =
        "curl -sS --max-time 300 -H 'Content-Type: application/json' " +
        shell_quote(config_.ollama_url + "/api/chat") +
        " -d " + shell_quote(payload) + " 2>/dev/null";
    const std::string response = run_command_capture(cmd);
    auto content = extract_json_string_after_key(response, "\"content\"");
    return content.value_or(response);
}
