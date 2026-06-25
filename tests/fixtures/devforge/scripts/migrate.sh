#!/bin/bash
# Database migration runner for DevForge
set -euo pipefail

DB_PATH="${1:-devforge.db}"
DIRECTION="${2:-up}"

echo "Running migrations: direction=${DIRECTION}, db=${DB_PATH}"

DEVFORGE_BIN="${DEVFORGE_BIN:-./bin/migrate}"

if [ ! -f "$DEVFORGE_BIN" ]; then
    echo "Building migrate binary..."
    go build -o "$DEVFORGE_BIN" ./cmd/migrate
fi

"$DEVFORGE_BIN" --db "$DB_PATH" --direction "$DIRECTION"

echo "Migrations complete."
