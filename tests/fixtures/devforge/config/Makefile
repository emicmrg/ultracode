.PHONY: all build test clean run dev setup

GOFILES = $(shell find . -type f -name '*.go' -not -path './vendor/*')
CPPFILES = $(shell find lib -type f -name '*.cpp' -o -name '*.hpp')
PYFILES = $(shell find api tests scripts -type f -name '*.py')
TSFILES = $(shell find web -type f -name '*.ts' -o -name '*.tsx')
CONFIGFILES = $(shell find config -type f -name '*.yaml' -o -name '*.json')

all: build-native build-go

build-native:
	@echo "=== Building native C++ libraries ==="
	cmake -S lib -B lib/build -DCMAKE_BUILD_TYPE=Release
	cmake --build lib/build --parallel $$(nproc)

build-go: go.sum
	@echo "=== Building Go binaries ==="
	go build -o bin/server ./cmd/server
	go build -o bin/worker ./cmd/worker
	go build -o bin/migrate ./cmd/migrate

build-frontend:
	@echo "=== Building frontend ==="
	cd web && npm ci && npm run build

test: test-go test-py test-ts

test-go:
	@echo "=== Running Go tests ==="
	go test ./... -v -race -count=1

test-py:
	@echo "=== Running Python tests ==="
	cd api && python -m pytest tests/ -v --cov=.

test-ts:
	@echo "=== Running TypeScript tests ==="
	cd web && npm test -- --coverage

lint: lint-go lint-py lint-ts lint-cpp

lint-go:
	@echo "=== Linting Go ==="
	golangci-lint run ./...

lint-py:
	@echo "=== Linting Python ==="
	ruff check api/ tests/ scripts/

lint-ts:
	@echo "=== Linting TypeScript ==="
	cd web && npx eslint . --ext .ts,.tsx

lint-cpp:
	@echo "=== Linting C++ ==="
	clang-format --dry-run --Werror $(CPPFILES)

lint-config:
	@echo "=== Validating configs ==="
	yamllint config/*.yaml
	python -m json.tool config/schemas/user.json > /dev/null
	python -m json.tool config/schemas/project.json > /dev/null

fmt:
	gofmt -w $(GOFILES)
	ruff format api/ tests/ scripts/
	clang-format -i $(CPPFILES)

clean:
	rm -rf bin/ lib/build/ web/dist/ web/node_modules/
	go clean -cache -testcache

dev:
	docker compose -f config/docker-compose.yml up --build

setup:
	docker compose -f config/docker-compose.yml run --rm migrate

db-seed:
	python scripts/seed_db.py --db devforge.db --count 100

bench:
	bash scripts/bench.sh

check: lint test
	@echo "=== All checks passed ==="
