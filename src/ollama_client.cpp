#include "ollama_client.hpp"
#include "utils.hpp"

#include <utility>

OllamaClient::OllamaClient(Config config) : config_(std::move(config)) {}

std::vector<float> OllamaClient::embed(const std::string& input) const {
    const std::string payload =
        "{\"model\":\"" + json_escape(config_.embedding_model) +
        "\",\"input\":\"" + json_escape(input) +
        "\",\"keep_alive\":\"1m\"}";
    const std::string cmd =
        "curl -sS --max-time 180 -H 'Content-Type: application/json' " +
        shell_quote(config_.ollama_url + "/api/embed") +
        " -d " + shell_quote(payload) + " 2>/dev/null";
    return parse_first_embedding(run_command_capture(cmd));
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
