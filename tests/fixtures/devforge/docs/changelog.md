# Changelog

All notable changes to the DevForge platform.

## [0.3.0] - 2024-06-01

### Added
- OAuth2 support for GitHub and Google providers
- Rate limiting middleware with token bucket algorithm
- Artifact expiration and automatic cleanup
- WebSocket support for real-time dashboard updates

### Changed
- Migrated configuration from JSON to YAML
- Improved pagination with cursor-based navigation option
- Upgraded Go to 1.21, C++ standard to C++20

### Fixed
- Memory leak in long-running worker connections
- Race condition in artifact upload under concurrent access
- Incorrect checksum validation for files over 2 GiB

## [0.2.0] - 2024-04-15

### Added
- Web dashboard with project and artifact views
- Native C++ compression and crypto libraries
- Database migration system with up/down support
- Project settings: max artifacts, retention, CI toggle

### Changed
- API responses now include pagination metadata
- Artifact model gains content_type and upload tracking
- Session management extracted to separate package

### Fixed
- Token validation edge case with near-expiry tokens
- SQLite connection not being properly closed on shutdown
- Empty project name allowed through validation

## [0.1.0] - 2024-03-01

### Added
- Initial release
- REST API for users, projects, and artifacts
- JWT-based authentication
- SQLite storage backend
- In-memory store for development
- CLI with server, worker, and migrate commands
- Docker Compose setup
- Basic health check endpoint
- FNV-1a hashing for content addressing
