#pragma once

#include "app/config.hpp"

#include <string>
#include <vector>

class OllamaClient {
public:
    explicit OllamaClient(Config config);

    std::vector<float> embed(const std::string& input) const;
    std::vector<std::vector<float>> embed_batch(const std::vector<std::string>& inputs) const;
    std::string chat(const std::string& system, const std::string& user) const;

private:
    Config config_;
};
