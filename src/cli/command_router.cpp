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
    int arg_start = 2;
    if ((request.name == "search" || request.name == "ask" || request.name == "patch") &&
        argc >= 4 && std::string(argv[2]) == "--file") {
        request.file_target = argv[3];
        arg_start = 4;
    }

    if (request.name == "search" || request.name == "ask" || request.name == "patch") {
        request.args.push_back(collect_args(argc, argv, arg_start));
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
        << "  ultracode search [--file <path>] <query>\n"
        << "  ultracode ask [--file <path>] <question>\n"
        << "  ultracode chat\n"
        << "  ultracode patch [--file <path>] <instruction>\n"
        << "  ultracode apply <patch-id>\n"
        << "  ultracode reject <patch-id>\n"
        << "  ultracode explain <file>\n";
}
