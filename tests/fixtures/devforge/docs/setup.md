# Setting Up DevForge

## Prerequisites

- Go 1.21+
- C++20 compiler (GCC 13+ or Clang 17+)
- CMake 3.20+
- Python 3.11+ (optional, for scripts)
- Node.js 20+ (for frontend)
- Docker and Docker Compose (for containerized setup)

## Quick Start (Docker)

```bash
git clone https://github.com/devforge/platform.git
cd platform
docker compose -f config/docker-compose.yml up --build
```

The API will be available at `http://localhost:8080` and the dashboard at `http://localhost:5173`.

## Manual Build

### 1. Build Native Libraries

```bash
cmake -S lib -B lib/build -DCMAKE_BUILD_TYPE=Release
cmake --build lib/build --parallel $(nproc)
```

### 2. Build Go Binaries

```bash
go mod download
go build -o bin/server ./cmd/server
go build -o bin/worker ./cmd/worker
go build -o bin/migrate ./cmd/migrate
```

### 3. Run Database Migrations

```bash
./bin/migrate --db devforge.db --direction up
```

### 4. Start the Server

```bash
./bin/server --port 8080 --auth=true
```

### 5. Start the Worker

```bash
./bin/worker
```

## Configuration

Copy the default configuration and modify as needed:

```bash
cp config/default.yaml config/local.yaml
```

Key configuration sections:

```yaml
server:
  port: 8080         # API server port
  host: "0.0.0.0"    # Bind address

auth:
  jwt:
    secret: "..."     # Set via JWT_SECRET env var
    ttl: 24h          # Token expiration

database:
  driver: sqlite3     # or "postgres"
  path: "devforge.db" # For SQLite only

storage:
  artifacts:
    path: "./data"    # Local storage path
    max_size: 10737418240  # 10 GiB limit
```

## Environment Variables

| Variable | Description | Default |
|---|---|---|
| `JWT_SECRET` | JWT signing secret | Required |
| `DB_HOST` | Database host | localhost |
| `DB_USER` | Database user | devforge |
| `DB_PASSWORD` | Database password | Required |
| `DB_NAME` | Database name | devforge |
| `STORAGE_BACKEND` | Storage backend (local/s3) | local |
| `AWS_ACCESS_KEY_ID` | S3 access key | — |
| `AWS_SECRET_ACCESS_KEY` | S3 secret key | — |
| `LOG_LEVEL` | Log level (debug/info/warn/error) | info |

## Development Workflow

```bash
# Run all tests
make test

# Run linting
make lint

# Auto-format code
make fmt

# Seed database with test data
make db-seed

# Run benchmarks
make bench
```

## Verifying the Installation

```bash
# Health check
curl http://localhost:8080/health

# Login and get token
TOKEN=$(curl -s -X POST http://localhost:8080/auth/login \
  -H "Content-Type: application/json" \
  -d '{"email":"admin@devforge.io","password":"admin123"}' \
  | python -c "import sys,json; print(json.load(sys.stdin)['token'])")

# List projects
curl http://localhost:8080/api/v1/projects \
  -H "Authorization: Bearer $TOKEN"
```
