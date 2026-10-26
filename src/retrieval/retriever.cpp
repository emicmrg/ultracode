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

#include "retrieval/retriever.hpp"

#include "index/index_store.hpp"
#include "llm/ollama_client.hpp"
#include "support/utils.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace {

// BM25 parameters. k1 controls term-frequency saturation, b controls how
// strongly long documents are penalised (0 = no length normalisation,
// 1 = full).
constexpr double kBm25K1 = 1.5;
constexpr double kBm25B = 0.75;

double dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
        sum += static_cast<double>(a[i]) * b[i];
    return sum;
}

struct LexicalDoc {
    std::unordered_map<std::string, int> term_freq;
    int length = 0;
};

// Corpus statistics for BM25.
//
// The previous scorer returned the fraction of query tokens found anywhere in
// the chunk, using substring matching. That had three problems: no term
// frequency, no length normalisation (so long documents trivially contained
// every query token and saturated at 1.0), and substring matches ("cat" hit
// "concatenate"). BM25 addresses all three.
class LexicalIndex {
public:
    void add(const std::string& text) {
        LexicalDoc doc;
        std::unordered_set<std::string> seen;
        for (const auto& token : tokenize(text)) {
            ++doc.term_freq[token];
            ++doc.length;
            seen.insert(token);
        }
        for (const auto& token : seen) ++doc_freq_[token];
        docs_.push_back(std::move(doc));
    }

    void finalize() {
        long long total = 0;
        for (const auto& doc : docs_) total += doc.length;
        avg_length_ = docs_.empty()
                          ? 0.0
                          : static_cast<double>(total) / static_cast<double>(docs_.size());
    }

    double score(size_t index, const std::vector<std::string>& query) const {
        if (index >= docs_.size() || avg_length_ <= 0.0) return 0.0;
        const LexicalDoc& doc = docs_[index];
        const double corpus_size = static_cast<double>(docs_.size());

        double total = 0.0;
        for (const auto& token : query) {
            const auto tf_it = doc.term_freq.find(token);
            if (tf_it == doc.term_freq.end()) continue;

            const double tf = static_cast<double>(tf_it->second);
            const auto df_it = doc_freq_.find(token);
            const double df =
                df_it == doc_freq_.end() ? 0.0 : static_cast<double>(df_it->second);

            const double idf = std::log(1.0 + (corpus_size - df + 0.5) / (df + 0.5));
            const double length_norm =
                1.0 - kBm25B + kBm25B * (static_cast<double>(doc.length) / avg_length_);
            total += idf * (tf * (kBm25K1 + 1.0)) / (tf + kBm25K1 * length_norm);
        }
        return total;
    }

private:
    std::vector<LexicalDoc> docs_;
    std::unordered_map<std::string, int> doc_freq_;
    double avg_length_ = 0.0;
};

}  // namespace

std::vector<RankedChunk> score_all_chunks(const fs::path& root,
                                           const Config& cfg,
                                           const std::string& query,
                                           const std::set<std::string>& preferred_paths,
                                           const std::string& required_path) {
    const auto chunks = load_manifest(root);
    const auto vectors = load_vector_store(root);
    const OllamaClient ollama(cfg);
    std::vector<float> qv = ollama.embed(query);
    if (qv.empty()) qv = hashed_embedding(query, cfg.fallback_embedding_dim);

    // First pass: read contents and build the BM25 corpus. Path and symbol are
    // included so that a query naming a file or function still matches
    // lexically.
    std::vector<std::string> contents;
    contents.reserve(chunks.size());
    LexicalIndex lexical;
    for (const auto& c : chunks) {
        std::string content = slurp(chunk_path(root, c.id));
        lexical.add(c.path + " " + c.symbol + " " + content);
        contents.push_back(std::move(content));
    }
    lexical.finalize();

    const auto query_tokens = tokenize(query);

    // Second pass: BM25 is unbounded, so normalise by the best score for this
    // query to keep it on the same 0..1 scale as the vector score. Without
    // this the fixed 0.62/0.38 blend below would be meaningless.
    std::vector<double> raw_lexical(chunks.size(), 0.0);
    double max_lexical = 0.0;
    for (size_t i = 0; i < chunks.size(); ++i) {
        raw_lexical[i] = lexical.score(i, query_tokens);
        max_lexical = std::max(max_lexical, raw_lexical[i]);
    }

    std::vector<RankedChunk> scored;
    scored.reserve(chunks.size());
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& c = chunks[i];
        const auto it = vectors.find(c.id);
        const std::vector<float> cv = it != vectors.end() ? it->second : std::vector<float>{};
        const double vs = dot_product(qv, cv);
        const double ls = max_lexical > 0.0 ? raw_lexical[i] / max_lexical : 0.0;
        const double diff_bonus = preferred_paths.count(c.path) ? 0.15 : 0.0;
        const double file_bonus = (!required_path.empty() && c.path == required_path) ? 1.0 : 0.0;
        const double score = 0.62 * vs + 0.38 * ls + diff_bonus + file_bonus;
        scored.push_back({c, score, vs, ls, contents[i]});
    }
    return scored;
}

std::vector<RankedChunk> retrieve(const fs::path& root,
                                   const Config& cfg,
                                   const std::string& query,
                                   int top_k,
                                   const std::set<std::string>& preferred_paths,
                                   const std::string& required_path) {
    auto ranked = score_all_chunks(root, cfg, query, preferred_paths, required_path);
    std::sort(ranked.begin(), ranked.end(),
              [](const RankedChunk& a, const RankedChunk& b) { return a.score > b.score; });
    if (static_cast<int>(ranked.size()) > top_k)
        ranked.resize(static_cast<size_t>(top_k));
    return ranked;
}
