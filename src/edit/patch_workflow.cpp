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

}  // namespace

std::string sanitize_patch_text(const std::string& model_output) {
    // Para SEARCH/REPLACE, no queremos eliminar las etiquetas, 
    // pero podemos limpiar bloques markdown genéricos si el modelo los añadió.
    std::string text = trim(model_output);
    const size_t diff_block = text.find("```");
    if (diff_block != std::string::npos) {
        // Solo extraemos si hay un SEARCH block adentro
        if (text.find("<<<<<<< SEARCH") != std::string::npos) {
            // Do not strip markdown blindly if it breaks the block, 
            // but usually we can just return the raw text as our parser is robust.
            return text;
        }
    }
    return text;
}

struct ParsedBlock {
    std::string file_path;
    std::string search_text;
    std::string replace_text;
    size_t start_pos;
};

std::vector<ParsedBlock> parse_search_replace_blocks(const std::string& text) {
    std::vector<ParsedBlock> blocks;
    size_t pos = 0;
    std::string current_file;

    while (pos < text.size()) {
        size_t file_pos = text.find("File: ", pos);
        size_t search_pos = text.find("<<<<<<< SEARCH", pos);

        if (search_pos == std::string::npos) break;

        // Find the nearest "File: " before this SEARCH block
        if (file_pos != std::string::npos && file_pos < search_pos) {
            size_t end_file = text.find('\n', file_pos);
            if (end_file != std::string::npos) {
                current_file = trim(text.substr(file_pos + 6, end_file - (file_pos + 6)));
                // Strip backticks if the model added them
                if (!current_file.empty() && current_file.front() == '`') current_file.erase(0, 1);
                if (!current_file.empty() && current_file.back() == '`') current_file.pop_back();
            }
        }

        size_t mid_pos = text.find("=======", search_pos);
        size_t end_pos = text.find(">>>>>>> REPLACE", mid_pos);

        if (mid_pos == std::string::npos || end_pos == std::string::npos) {
            break; // Malformed block
        }

        ParsedBlock block;
        block.file_path = current_file;
        block.start_pos = search_pos;
        
        size_t search_content_start = search_pos + 14;
        if (text[search_content_start] == '\n') search_content_start++;
        
        size_t search_content_end = mid_pos;
        if (text[search_content_end - 1] == '\n') search_content_end--;

        size_t replace_content_start = mid_pos + 7;
        if (text[replace_content_start] == '\n') replace_content_start++;

        size_t replace_content_end = end_pos;
        if (text[replace_content_end - 1] == '\n') replace_content_end--;

        block.search_text = text.substr(search_content_start, search_content_end - search_content_start);
        block.replace_text = text.substr(replace_content_start, replace_content_end - replace_content_start);

        blocks.push_back(block);
        pos = end_pos + 15;
    }
    return blocks;
}

bool validate_patch_text(const fs::path& root,
                         const std::string& diff_text,
                         std::string* error) {
    if (trim(diff_text).empty()) {
        if (error) *error = "Model returned an empty patch.";
        return false;
    }

    auto blocks = parse_search_replace_blocks(diff_text);
    if (blocks.empty()) {
        if (error) *error = "Model did not return any valid SEARCH/REPLACE blocks.";
        return false;
    }

    for (const auto& block : blocks) {
        if (block.file_path.empty()) {
            if (error) *error = "A SEARCH block is missing a 'File: <path>' declaration.";
            return false;
        }

        fs::path target_file = root / block.file_path;
        if (!fs::exists(target_file)) {
            if (error) *error = "Target file does not exist: " + block.file_path;
            return false;
        }

        std::string content = slurp(target_file);
        if (content.find(block.search_text) == std::string::npos) {
            if (error) *error = "SEARCH block not found exactly in " + block.file_path + 
                                ". Ensure it is character-for-character identical.";
            return false;
        }
    }

    return true;
}

std::string slugify(const std::string& text, size_t max_len = 35) {
    std::string slug;
    for (char c : text) {
        if (std::isalnum(c)) slug += static_cast<char>(std::tolower(c));
        else if (!slug.empty() && slug.back() != '-') slug += '-';
    }
    if (!slug.empty() && slug.back() == '-') slug.pop_back();
    if (slug.length() > max_len) slug = slug.substr(0, max_len);
    if (!slug.empty() && slug.back() == '-') slug.pop_back();
    if (slug.empty()) slug = "patch";
    return slug;
}

PatchProposal save_patch_proposal(const fs::path& root,
                                  const std::string& instruction,
                                  const std::string& diff_text,
                                  const std::vector<std::string>& target_paths) {
    fs::create_directories(patches_dir(root));
    const auto now = std::chrono::system_clock::now().time_since_epoch().count();
    PatchProposal proposal;
    
    std::string base_id = slugify(instruction);
    std::string hash = fnv1a_hex(instruction + diff_text + std::to_string(now)).substr(0, 4);
    proposal.id = base_id + "-" + hash;
    
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
    auto blocks = parse_search_replace_blocks(diff_text);
    for (const auto& block : blocks) {
        if (!block.file_path.empty() && is_safe_repo_relative_path(block.file_path)) {
            unique_paths.insert(block.file_path);
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

    const fs::path diff_path = patch_file_path(root, patch_id);
    std::string diff_text = slurp(diff_path);
    auto blocks = parse_search_replace_blocks(diff_text);

    // Validate all blocks again before touching disk
    for (const auto& block : blocks) {
        fs::path target_file = root / block.file_path;
        if (!fs::exists(target_file)) {
            if (error) *error = "File disappeared: " + block.file_path;
            return false;
        }
        std::string content = slurp(target_file);
        if (content.find(block.search_text) == std::string::npos) {
            if (error) *error = "Block no longer matches content in " + block.file_path;
            return false;
        }
    }

    // Apply all blocks
    for (const auto& block : blocks) {
        fs::path target_file = root / block.file_path;
        std::string content = slurp(target_file);
        
        size_t pos = content.find(block.search_text);
        if (pos != std::string::npos) {
            content.replace(pos, block.search_text.length(), block.replace_text);
            write_text(target_file, content);
        }
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
