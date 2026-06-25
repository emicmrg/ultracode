# DevForge Platform Architecture

## Overview

DevForge is a developer tool platform for managing software artifacts across projects and teams. It provides artifact storage, versioning, scanning, and distribution with a REST API, web dashboard, and CLI.

## System Components

```
┌─────────────┐     ┌─────────────┐     ┌──────────────┐
│   Web UI    │     │   REST API  │     │   Worker     │
│ (TypeScript)│────▶│  (Go/C++)   │────▶│   (Go)       │
└─────────────┘     └──────┬──────┘     └──────┬───────┘
                           │                    │
                     ┌─────▼──────┐      ┌──────▼──────┐
                     │   Store    │      │   Queue     │
                     │ (SQLite/PG)│      │  (Redis)    │
                     └────────────┘      └─────────────┘
```

## Layers

### 1. API Layer (Go)
- `cmd/server` — HTTP server entry point with graceful shutdown
- `cmd/migrate` — Database migration runner
- Routes: users, projects, artifacts
- Middleware: authentication, rate limiting, CORS

### 2. Worker Layer (Go)
- `cmd/worker` — Background job processor
- Job types: artifact scanning, cleanup, index rebuild
- Dispatcher with configurable concurrency and retry

### 3. Native Libraries (C++)
- `lib/compression` — zlib and LZ4 wrappers
- `lib/crypto` — SHA-256, MD5, CRC32 hashing and AES cipher
- `lib/format` — JSON and MessagePack serialization

### 4. Web Frontend (TypeScript)
- Components: Dashboard, ProjectList, ArtifactViewer
- Hooks: useAuth, useWebSocket
- Services: API client, auth manager
- Utilities: formatting, validation

### 5. Python Utilities (Python)
- `api/` — Alternative API layer with models and middleware
- `scripts/` — Database seeding, type generation, benchmarks

## Data Flow

1. User uploads artifact via CLI or Web UI
2. Server validates and stores metadata in database
3. Binary is written to configured storage backend (local/S3)
4. Worker picks up scan job from queue
5. Worker analyzes artifact (size, checksum, contents)
6. Results are stored back to database
7. Users query artifacts via API or dashboard

## Security Considerations

- JWT-based authentication with configurable TTL
- OAuth2 support for GitHub and Google
- Role-based access control (admin, user, viewer)
- Rate limiting per IP and per user
- Input sanitization on all user-provided data
- TLS in production with certificate management
- Checksum validation on all artifact operations

## Performance Characteristics

- SQLite for single-node deployments, PostgreSQL for multi-node
- Connection pooling with configurable limits
- Artifact chunking for large file uploads
- Caching layer via Redis for frequent queries
- Native C++ compression for throughput-sensitive operations
- Worker pool with backpressure through bounded queues

## Deployment Models

### Single Node
- All services in one binary with SQLite
- Docker Compose for easy local setup

### Multi Node
- Separate server, worker, and database instances
- PostgreSQL with read replicas
- S3-compatible object storage
- Kubernetes with Helm chart
