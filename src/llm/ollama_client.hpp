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
    std::string chat(const std::vector<ChatMessage>& messages,
                     TaskType task,
                     const std::string& model_override = "") const;
    std::string chat_stream(const std::vector<ChatMessage>& messages,
                            std::ostream& out,
                            const std::string& model_override = "") const;
    std::string chat_stream(const std::vector<ChatMessage>& messages,
                            std::ostream& out,
                            TaskType task,
                            const std::string& model_override = "") const;

private:
    Config config_;
};
