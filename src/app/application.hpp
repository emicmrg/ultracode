#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct CommandRequest {
    std::string name;
    std::string file_target;
    bool debug = false;
    std::vector<std::string> args;
};

int run_command(const std::filesystem::path& root, const CommandRequest& request);
