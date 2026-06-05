#pragma once

#include "app/config.hpp"

#include <iosfwd>
#include <string>
#include <vector>

struct ChatMessage {
    std::string role;
    std::string content;
};

class OllamaClient {
public:
    explicit OllamaClient(Config config);

    std::vector<float> embed(const std::string& input) const;
    std::vector<std::vector<float>> embed_batch(const std::vector<std::string>& inputs) const;
    std::string chat(const std::string& system, const std::string& user) const;
    std::string chat(const std::vector<ChatMessage>& messages,
                     const std::string& model_override = "") const;
    std::string chat_stream(const std::vector<ChatMessage>& messages,
                            std::ostream& out,
                            const std::string& model_override = "") const;

private:
    Config config_;
};
