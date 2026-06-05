# MVP Plan

## MVP goal

Build a local-first terminal coding assistant that can run acceptably on limited hardware by using a small C++ CLI, local indexing, cached embeddings, compact retrieval, and Ollama for model execution.

## Success criteria

- Build with CMake and a C++20 compiler.
- Work without internet after dependencies/models are available.
- Index a small or medium repository without crashing.
- Avoid re-sending the whole repository to the model.
- Return source paths and line ranges.
- Keep the CLI lightweight; the model process remains external.

## Phase 1: end-to-end local flow

Implemented in this MVP:

- CLI commands: `init`, `index`, `stats`, `search`, `ask`, `explain`.
- Local `.ultracode/` workspace.
- Heuristic code chunking.
- Ollama `/api/embed` integration.
- Ollama `/api/chat` integration.
- Local fallback embeddings.
- Brute-force vector search.
- Lexical scoring.
- Hybrid ranking.

## Phase 2: better parsing

- Introduce `ChunkExtractor` interface.
- Add Tree-sitter C/C++ extractor.
- Add Python and TypeScript Tree-sitter extractors.
- Preserve heuristic extractor as fallback.

## Phase 3: better performance

- Incremental indexing by file hash.
- Batched `/api/embed` calls.
- Memory-mapped vector store or compact binary format.
- Optional HNSW/USearch backend for large repos.

## Phase 4: coding assistant features

- Streaming chat output.
- Patch generation.
- Git diff awareness.
- `/context`, `/model`, `/clear` interactive commands.
- Safe apply/reject workflow for edits.
