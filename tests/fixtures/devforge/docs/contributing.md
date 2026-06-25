# Contributing to DevForge

## Code of Conduct

Be respectful. Be constructive. Assume good intent.

## Development Process

1. Fork the repository
2. Create a feature branch: `git checkout -b feat/my-feature`
3. Make your changes
4. Write or update tests
5. Run the full test suite: `make check`
6. Commit with a descriptive message
7. Push and create a pull request

## Commit Conventions

We follow [Conventional Commits](https://www.conventionalcommits.org/):

```
feat: add artifact expiration policy
fix: handle empty checksum in artifact validation
docs: update API reference with auth examples
refactor: extract pagination logic into helper
test: add integration tests for OAuth flow
chore: update Go dependencies
```

Allowed types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `perf`, `ci`, `style`

## Code Style

### Go
- Follow [Effective Go](https://go.dev/doc/effective_go)
- Use `gofmt` for formatting
- Error messages should be lowercase, no trailing punctuation
- Prefer explicit over implicit

```go
// Good
func NewTokenManager(secret string, ttl time.Duration) *TokenManager {
    return &TokenManager{secret: []byte(secret), ttl: ttl}
}

// Avoid
func newTokenManager(s string, d time.Duration) *TokenManager { return &TokenManager{[]byte(s), d} }
```

### C++
- C++20 standard
- Namespaces: `devforge::compression`, `devforge::crypto`, `devforge::format`
- Use `#pragma once` for headers
- Prefer `std::string_view` for read-only string parameters
- RAII for resource management

```cpp
namespace devforge::crypto {

class Hasher {
public:
    explicit Hasher(std::string_view key);
    std::string hash_sha256(std::string_view input) const;
};

} // namespace devforge::crypto
```

### Python
- Type hints required on all function signatures
- Docstrings for public functions and classes
- Use `dataclasses` for models

```python
from dataclasses import dataclass

@dataclass
class Artifact:
    id: str
    name: str
    version: str
    size: int = 0

    def validate(self) -> None:
        if not self.name:
            raise ValueError("name is required")
```

### TypeScript
- Use interfaces for data shapes
- Use types for unions and utilities
- Async/await over raw promises
- Explicit return types on exported functions

```typescript
interface ApiResponse<T> {
  data: T | null;
  error: string | null;
}

async function fetchApi<T>(endpoint: string): Promise<ApiResponse<T>> {
  const response = await fetch(endpoint);
  return response.json();
}
```

## Testing Requirements

- Unit tests for all new functions
- Integration tests for API endpoints
- Chunking tests for all supported languages
- Edge cases: empty input, null values, boundary conditions
- Test file naming: `test_<module>.py`, `<module>_test.go`, `<module>.test.ts`

## Pull Request Checklist

- [ ] Tests pass: `make test`
- [ ] Linting passes: `make lint`
- [ ] No new compiler warnings
- [ ] Documentation updated if API changed
- [ ] Commit messages follow convention
- [ ] Branch is rebased on latest main

## Getting Help

- Open a GitHub issue for bugs
- Use GitHub Discussions for questions
- Tag with appropriate labels: `bug`, `enhancement`, `question`, `documentation`
