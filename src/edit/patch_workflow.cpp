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

std::vector<std::string> split_tsv_preserve_empty(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (char ch : line) {
        if (ch == '\t') {
            fields.push_back(current);
            current.clear();
        } else {
            current += ch;
        }
    }
    fields.push_back(current);
    return fields;
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

std::string build_apply_check_command(const fs::path& root, const fs::path& patch_path) {
    return "git -C " + shell_quote(root.string()) +
           " apply --check " + shell_quote(patch_path.string()) + " 2>&1";
}

bool looks_like_diff_line(const std::string& line) {
    return line.rfind("diff --git ", 0) == 0 ||
           line.rfind("--- ", 0) == 0;
}

}  // namespace

std::string sanitize_patch_text(const std::string& model_output) {
    std::string text = trim(model_output);
    const size_t diff_block = text.find("```diff");
    const size_t plain_block = text.find("```");
    if (diff_block != std::string::npos) {
        const size_t start = text.find('\n', diff_block);
        if (start != std::string::npos) {
            const size_t end = text.find("```", start + 1);
            text = trim(text.substr(start + 1, end == std::string::npos ? std::string::npos : end - start - 1));
        }
    } else if (plain_block != std::string::npos) {
        const size_t start = text.find('\n', plain_block);
        if (start != std::string::npos) {
            const size_t end = text.find("```", start + 1);
            text = trim(text.substr(start + 1, end == std::string::npos ? std::string::npos : end - start - 1));
        }
    }

    std::stringstream ss(text);
    std::string line;
    std::vector<std::string> lines;
    bool started = false;
    while (std::getline(ss, line)) {
        if (!started && looks_like_diff_line(line)) {
            started = true;
        }
        if (started) {
            lines.push_back(line);
        }
    }

    if (!lines.empty()) {
        std::ostringstream out;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i) {
                out << '\n';
            }
            out << lines[i];
        }
        text = trim(out.str());
    }

    if (!text.empty() && text.back() != '\n') {
        text += '\n';
    }

    return text;
}

bool validate_patch_text(const fs::path& root,
                         const std::string& diff_text,
                         std::string* error) {
    if (trim(diff_text).empty()) {
        if (error) *error = "Model returned an empty patch.";
        return false;
    }

    const bool has_git_header = diff_text.find("diff --git ") != std::string::npos;
    const bool has_old_header = diff_text.find("\n--- ") != std::string::npos || diff_text.rfind("--- ", 0) == 0;
    const bool has_new_header = diff_text.find("\n+++ ") != std::string::npos || diff_text.rfind("+++ ", 0) == 0;
    const bool has_hunk = diff_text.find("\n@@ ") != std::string::npos || diff_text.rfind("@@ ", 0) == 0;
    if ((!has_git_header && !has_old_header) || !has_new_header || !has_hunk) {
        if (error) *error = "Model did not return a complete unified diff.";
        return false;
    }

    const auto target_paths = extract_patch_target_paths(diff_text);
    if (target_paths.empty()) {
        if (error) *error = "Patch does not contain valid target file paths.";
        return false;
    }

    fs::create_directories(patches_dir(root));
    const fs::path temp_patch = patches_dir(root) / (".validate-" + fnv1a_hex(diff_text) + ".diff");
    write_text(temp_patch, diff_text);
    const std::string check_output = run_command_capture(build_apply_check_command(root, temp_patch));
    std::error_code ec;
    fs::remove(temp_patch, ec);
    if (!check_output.empty()) {
        if (error) *error = trim(check_output);
        return false;
    }
    return true;
}

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

    const std::vector<std::string> fields = split_tsv_preserve_empty(line);
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
    const std::string check_output = run_command_capture(build_apply_check_command(root, diff_path));
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
