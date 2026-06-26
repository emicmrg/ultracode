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

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct PatchProposal {
    std::string id;
    std::string status;
    std::string instruction;
    std::vector<std::string> target_paths;
};

std::string sanitize_patch_text(const std::string& model_output);
bool validate_patch_text(const std::filesystem::path& root,
                         const std::string& diff_text,
                         std::string* error = nullptr);
PatchProposal save_patch_proposal(const std::filesystem::path& root,
                                  const std::string& instruction,
                                  const std::string& diff_text,
                                  const std::vector<std::string>& target_paths);
std::optional<PatchProposal> load_patch_proposal(const std::filesystem::path& root,
                                                 const std::string& patch_id);
std::vector<std::string> extract_patch_target_paths(const std::string& diff_text);
bool apply_patch_proposal(const std::filesystem::path& root,
                          const std::string& patch_id,
                          std::string* error = nullptr);
bool reject_patch_proposal(const std::filesystem::path& root,
                           const std::string& patch_id,
                           std::string* error = nullptr);
