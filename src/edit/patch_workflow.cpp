#include "edit/patch_workflow.hpp"

#include "support/utils.hpp"

#include <chrono>
#include <filesystem>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path patches_dir(const fs::path& root) {
    return root / ".ultracode" / "patches";
}

fs::path patch_file_path(const fs::path& root, const std::string& patch_id) {
    return patches_dir(root) / (patch_id + ".diff");
}

fs::path patch_meta_path(const fs::path& root, const std::string& patch_id) {
    return patches_dir(root) / (patch_id + ".meta");
}

std::string join_strings(const std::vector<std::string>& values, char delim) {
    std::ostringstream ss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) {
            ss << delim;
        }
        ss << values[i];
    }
    return ss.str();
}

std::vector<std::string> split_strings(const std::string& value, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }
    return parts;
}

bool is_safe_repo_relative_path(const std::string& path_text) {
    const fs::path path(path_text);
    if (path.is_absolute()) {
        return false;
    }
    for (const auto& part : path) {
        if (part == "..") {
            return false;
        }
    }
    return !path.empty();
}

bool write_patch_meta(const fs::path& root, const PatchProposal& proposal) {
    std::ostringstream ss;
    ss << "id\tstatus\tinstruction\ttarget_paths\n";
    ss << proposal.id << '\t'
       << proposal.status << '\t'
       << proposal.instruction << '\t'
       << join_strings(proposal.target_paths, ',') << '\n';
    return write_text(patch_meta_path(root, proposal.id), ss.str());
}

bool update_patch_status(const fs::path& root,
                         const std::string& patch_id,
                         const std::string& next_status) {
    auto proposal = load_patch_proposal(root, patch_id);
    if (!proposal) {
        return false;
    }
    proposal->status = next_status;
    return write_patch_meta(root, *proposal);
}

}  // namespace

PatchProposal save_patch_proposal(const fs::path& root,
                                  const std::string& instruction,
                                  const std::string& diff_text,
                                  const std::vector<std::string>& target_paths) {
    fs::create_directories(patches_dir(root));
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    PatchProposal proposal;
    proposal.id = fnv1a_hex(instruction + diff_text + std::to_string(now));
    proposal.status = "pending";
    proposal.instruction = instruction;
    proposal.target_paths = target_paths;
    write_text(patch_file_path(root, proposal.id), diff_text);
    write_patch_meta(root, proposal);
    return proposal;
}

std::optional<PatchProposal> load_patch_proposal(const fs::path& root,
                                                 const std::string& patch_id) {
    std::stringstream ss(slurp(patch_meta_path(root, patch_id)));
    std::string header;
    std::string line;
    if (!std::getline(ss, header) || !std::getline(ss, line)) {
        return std::nullopt;
    }

    std::vector<std::string> fields;
    std::stringstream ls(line);
    std::string field;
    while (std::getline(ls, field, '\t')) {
        fields.push_back(field);
    }
    if (fields.size() < 4) {
        return std::nullopt;
    }

    PatchProposal proposal;
    proposal.id = fields[0];
    proposal.status = fields[1];
    proposal.instruction = fields[2];
    proposal.target_paths = split_strings(fields[3], ',');
    return proposal;
}

std::vector<std::string> extract_patch_target_paths(const std::string& diff_text) {
    std::set<std::string> unique_paths;
    std::stringstream ss(diff_text);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.rfind("+++ b/", 0) == 0) {
            const std::string path = line.substr(6);
            if (is_safe_repo_relative_path(path)) {
                unique_paths.insert(path);
            }
        }
    }
    return std::vector<std::string>(unique_paths.begin(), unique_paths.end());
}

bool apply_patch_proposal(const fs::path& root,
                          const std::string& patch_id,
                          std::string* error) {
    const auto proposal = load_patch_proposal(root, patch_id);
    if (!proposal) {
        if (error) *error = "Patch proposal not found.";
        return false;
    }
    if (proposal->status != "pending") {
        if (error) *error = "Patch proposal is not pending.";
        return false;
    }
    if (proposal->target_paths.empty()) {
        if (error) *error = "Patch proposal has no valid target paths.";
        return false;
    }
    for (const auto& path : proposal->target_paths) {
        if (!is_safe_repo_relative_path(path)) {
            if (error) *error = "Patch touches an unsafe path.";
            return false;
        }
    }

    const fs::path diff_path = patch_file_path(root, patch_id);
    const std::string quoted_root = shell_quote(root.string());
    const std::string quoted_patch = shell_quote(diff_path.string());
    const std::string check_cmd =
        "git -C " + quoted_root + " apply --check " + quoted_patch + " 2>&1";
    const std::string check_output = run_command_capture(check_cmd);
    if (!check_output.empty()) {
        if (error) *error = trim(check_output);
        return false;
    }

    const std::string apply_cmd =
        "git -C " + quoted_root + " apply " + quoted_patch + " 2>&1";
    const std::string apply_output = run_command_capture(apply_cmd);
    if (!apply_output.empty()) {
        if (error) *error = trim(apply_output);
        return false;
    }

    update_patch_status(root, patch_id, "applied");
    return true;
}

bool reject_patch_proposal(const fs::path& root,
                           const std::string& patch_id,
                           std::string* error) {
    const auto proposal = load_patch_proposal(root, patch_id);
    if (!proposal) {
        if (error) *error = "Patch proposal not found.";
        return false;
    }
    if (proposal->status != "pending") {
        if (error) *error = "Patch proposal is not pending.";
        return false;
    }
    update_patch_status(root, patch_id, "rejected");
    return true;
}
