#!/bin/bash
# Benchmark runner for DevForge components
set -euo pipefail

ITERATIONS="${1:-5}"
RESULTS_DIR="bench_results_$(date +%Y%m%d_%H%M%S)"

echo "=== DevForge Benchmark Suite ==="
echo "Iterations: ${ITERATIONS}"
echo "Results: ${RESULTS_DIR}"
echo ""

mkdir -p "$RESULTS_DIR"

bench_go() {
    echo "--- Go API benchmarks ---"
    for i in $(seq 1 "$ITERATIONS"); do
        echo "  Run $i/$ITERATIONS..."
        go test -bench=. -benchmem ./... > "${RESULTS_DIR}/go_bench_${i}.txt" 2>&1 || true
    done
    grep -h "Benchmark" "${RESULTS_DIR}"/go_bench_*.txt | sort -k3 -n
}

bench_native() {
    echo "--- C++ native benchmarks ---"
    if [ -f lib/build/bench_compression ]; then
        for i in $(seq 1 "$ITERATIONS"); do
            echo "  Run $i/$ITERATIONS..."
            lib/build/bench_compression > "${RESULTS_DIR}/cpp_bench_${i}.txt" 2>&1 || true
        done
    else
        echo "  Skipped: native benchmark binary not found"
    fi
}

bench_python() {
    echo "--- Python benchmarks ---"
    for i in $(seq 1 "$ITERATIONS"); do
        echo "  Run $i/$ITERATIONS..."
        python -m pytest tests/ --benchmark-only > "${RESULTS_DIR}/py_bench_${i}.txt" 2>&1 || true
    done
}

bench_go
bench_native
bench_python

echo ""
echo "=== Results saved to ${RESULTS_DIR} ==="
