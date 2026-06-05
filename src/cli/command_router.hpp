#pragma once

#include "app/application.hpp"

#include <optional>
#include <ostream>

std::optional<CommandRequest> parse_command_request(int argc, char** argv);
void print_help(std::ostream& out);
