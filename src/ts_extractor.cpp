#include "ts_extractor.hpp"
#include "utils.hpp"

#include <cstring>
#include <tree_sitter/api.h>

// ── External grammar declarations ────────────────────────────────────────────
extern "C" {
    TSLanguage *tree_sitter_c(void);
    TSLanguage *tree_sitter_cpp(void);
    TSLanguage *tree_sitter_go(void);
    TSLanguage *tree_sitter_python(void);
    TSLanguage *tree_sitter_typescript(void);
}

// ── Language → grammar mapping ───────────────────────────────────────────────

static TSLanguage *language_for(const std::string& lang) {
    if (lang == "cpp")                                return tree_sitter_cpp();
    if (lang == "go")                                 return tree_sitter_go();
    if (lang == "python")                             return tree_sitter_python();
    if (lang == "typescript" || lang == "javascript") return tree_sitter_typescript();
    return nullptr;
}

// ── Query strings per language ───────────────────────────────────────────────

// Each query captures named nodes that correspond to top-level symbols.
static const char *query_for(const std::string& lang) {
    if (lang == "cpp") {
        return
            "(function_definition) @symbol "
            "(class_specifier) @symbol "
            "(struct_specifier) @symbol";
    }
    if (lang == "go") {
        return
            "(function_declaration) @symbol "
            "(method_declaration) @symbol "
            "(type_declaration) @symbol";
    }
    if (lang == "python") {
        return
            "(function_definition) @symbol "
            "(class_definition) @symbol";
    }
    if (lang == "typescript" || lang == "javascript") {
        return
            "(function_declaration) @symbol "
            "(class_declaration) @symbol "
            "(method_definition) @symbol "
            "(lexical_declaration) @symbol";
    }
    return nullptr;
}

// ── Symbol name extraction ────────────────────────────────────────────────────

// Walks into `node` to find an identifier or type_identifier child and returns
// its text.  Returns an empty string if none is found.
static std::string symbol_name(TSNode node, const char *source, uint32_t source_len) {
    const char *target_types[] = {
        "identifier", "type_identifier", "field_identifier", nullptr
    };
    // Breadth-first search up to 3 levels deep
    struct Entry { TSNode node; int depth; };
    std::vector<Entry> queue;
    queue.push_back({node, 0});
    for (size_t qi = 0; qi < queue.size(); ++qi) {
        const TSNode n = queue[qi].node;
        const int   d = queue[qi].depth;
        const char *nt = ts_node_type(n);
        for (int i = 0; target_types[i]; ++i) {
            if (std::strcmp(nt, target_types[i]) == 0) {
                const uint32_t start = ts_node_start_byte(n);
                const uint32_t end   = ts_node_end_byte(n);
                if (start < source_len && end <= source_len)
                    return std::string(source + start, end - start);
            }
        }
        if (d < 3) {
            for (uint32_t c = 0; c < ts_node_child_count(n); ++c)
                queue.push_back({ts_node_child(n, c), d + 1});
        }
    }
    return {};
}

// ── ChunkExtractor implementation ────────────────────────────────────────────

bool TreeSitterExtractor::supports(const std::string& language) const {
    return language_for(language) != nullptr;
}

std::vector<Chunk> TreeSitterExtractor::extract(const std::string& content,
                                                  const std::string& path,
                                                  const std::string& language) const {
    TSLanguage *ts_lang = language_for(language);
    if (!ts_lang) return {};

    const char *query_src = query_for(language);
    if (!query_src) return {};

    // Build parser
    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, ts_lang);

    const char *src  = content.c_str();
    const uint32_t src_len = static_cast<uint32_t>(content.size());

    TSTree  *tree      = ts_parser_parse_string(parser, nullptr, src, src_len);
    TSNode   root_node = ts_tree_root_node(tree);

    // Compile query
    uint32_t error_offset = 0;
    TSQueryError error_type = TSQueryErrorNone;
    TSQuery *query = ts_query_new(ts_lang, query_src,
                                  static_cast<uint32_t>(std::strlen(query_src)),
                                  &error_offset, &error_type);

    std::vector<Chunk> chunks;

    if (query && error_type == TSQueryErrorNone) {
        TSQueryCursor *cursor = ts_query_cursor_new();
        ts_query_cursor_exec(cursor, query, root_node);

        TSQueryMatch match;
        while (ts_query_cursor_next_match(cursor, &match)) {
            for (uint16_t i = 0; i < match.capture_count; ++i) {
                TSNode node = match.captures[i].node;
                const uint32_t start_byte = ts_node_start_byte(node);
                const uint32_t end_byte   = ts_node_end_byte(node);
                const TSPoint  start_pt   = ts_node_start_point(node);
                const TSPoint  end_pt     = ts_node_end_point(node);

                // 1-based line numbers
                const int start_line = static_cast<int>(start_pt.row) + 1;
                const int end_line   = static_cast<int>(end_pt.row)   + 1;

                std::string sym = symbol_name(node, src, src_len);
                if (sym.empty()) sym = ts_node_type(node);

                std::string chunk_content;
                if (end_byte <= src_len)
                    chunk_content = std::string(src + start_byte, end_byte - start_byte);

                Chunk c;
                c.path       = path;
                c.language   = language;
                c.symbol     = sym;
                c.start_line = start_line;
                c.end_line   = end_line;
                c.hash       = fnv1a_hex(path + chunk_content);
                c.id         = c.hash;
                chunks.push_back(c);
            }
        }
        ts_query_cursor_delete(cursor);
        ts_query_delete(query);
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return chunks;
}
