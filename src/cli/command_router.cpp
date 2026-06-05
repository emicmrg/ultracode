#include "cli/command_router.hpp"

#include <sstream>
#include <string>

namespace {

std::string collect_args(int argc, char** argv, int start) {
    std::ostringstream ss;
    for (int i = start; i < argc; ++i) {
        if (i > start) {
            ss << ' ';
        }
        ss << argv[i];
    }
    return ss.str();
}

}  // namespace

std::optional<CommandRequest> parse_command_request(int argc, char** argv) {
    if (argc < 2) {
        return std::nullopt;
    }

    CommandRequest request;
    request.name = argv[1];
    if (request.name == "search" || request.name == "ask") {
        request.args.push_back(collect_args(argc, argv, 2));
    } else if (argc >= 3) {
        request.args.push_back(argv[2]);
    }
    return request;
}

void print_help(std::ostream& out) {
    out << "ultracode - ultralight local coding assistant MVP\n\n"
        << "Usage:\n"
        << "  ultracode init\n"
        << "  ultracode index\n"
        << "  ultracode stats\n"
        << "  ultracode search <query>\n"
        << "  ultracode ask <question>\n"
        << "  ultracode chat\n"
        << "  ultracode explain <file>\n";
}
