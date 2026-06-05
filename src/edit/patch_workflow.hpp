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
