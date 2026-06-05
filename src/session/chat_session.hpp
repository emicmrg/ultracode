#pragma once

#include "app/config.hpp"

#include <filesystem>
#include <iosfwd>

int run_interactive_chat(const std::filesystem::path& root,
                         const Config& cfg,
                         std::istream& in,
                         std::ostream& out);
