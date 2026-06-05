#include "retriever.hpp"
#include "index_store.hpp"
#include "ollama_client.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cmath>

static double dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        sum += static_cast<double>(a[i]) * b[i];
    return sum;
}

static double lexical_score(const std::string& query,
                              const Chunk& c,
                              const std::string& content) {
    const auto q = tokenize(query);
    if (q.empty()) return 0.0;
    const std::string hay = lower(c.path + " " + c.symbol + " " + content);
    double hits = 0.0;
    for (const auto& token : q)
        if (hay.find(token) != std::string::npos) hits += 1.0;
    return hits / static_cast<double>(q.size());
}

std::vector<RankedChunk> retrieve(const fs::path& root,
                                   const Config& cfg,
                                   const std::string& query,
                                   int top_k) {
    const auto chunks = load_manifest(root);
    const OllamaClient ollama(cfg);
    std::vector<float> qv = ollama.embed(query);
    if (qv.empty()) qv = hashed_embedding(query, cfg.fallback_embedding_dim);

    std::vector<RankedChunk> ranked;
    for (const auto& c : chunks) {
        const std::string content = slurp(chunk_path(root, c.id));
        const auto cv             = load_vector(vector_path(root, c.id));
        const double vs           = dot_product(qv, cv);
        const double ls           = lexical_score(query, c, content);
        const double score        = 0.62 * vs + 0.38 * ls;
        ranked.push_back({c, score, vs, ls, content});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const RankedChunk& a, const RankedChunk& b) { return a.score > b.score; });
    if (static_cast<int>(ranked.size()) > top_k)
        ranked.resize(static_cast<size_t>(top_k));
    return ranked;
}
