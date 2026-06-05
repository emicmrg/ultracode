#include "app/application.hpp"
#include "cli/command_router.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    const fs::path root = fs::current_path();
    const auto request = parse_command_request(argc, argv);
    if (!request) {
        print_help(std::cout);
        return 0;
    }

    try {
        return run_command(root, *request);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
