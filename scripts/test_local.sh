#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BIN_DIR="${BUILD_DIR}/bin"

PS_ADDRESS="localhost:50051"
COORDINATOR_ADDRESS="localhost:50052"
TOTAL_WORKERS=2
ITERATIONS=5

cd "$PROJECT_ROOT"

if [ ! -d "$BUILD_DIR" ]; then
  echo "building project..."
  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"
  cmake ..
  make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

if [ ! -f "$BIN_DIR/parameter_server" ] || [ ! -f "$BIN_DIR/worker_main" ] || [ ! -f "$BIN_DIR/coordinator" ]; then
  echo "building executables..."
  cd "$BUILD_DIR"
  make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi

cd "$PROJECT_ROOT"

echo "starting coordinator on $COORDINATOR_ADDRESS..."
"$BIN_DIR/coordinator" "$COORDINATOR_ADDRESS" "$PS_ADDRESS" > /tmp/coordinator.log 2>&1 &
COORDINATOR_PID=$!

sleep 1

echo "starting parameter server on $PS_ADDRESS with $TOTAL_WORKERS workers..."
"$BIN_DIR/parameter_server" "$PS_ADDRESS" "$TOTAL_WORKERS" > /tmp/ps.log 2>&1 &
PS_PID=$!

sleep 2

echo "starting workers..."
"$BIN_DIR/worker_main" "$COORDINATOR_ADDRESS" 0 "$ITERATIONS" > /tmp/worker0.log 2>&1 &
WORKER0_PID=$!

"$BIN_DIR/worker_main" "$COORDINATOR_ADDRESS" 1 "$ITERATIONS" > /tmp/worker1.log 2>&1 &
WORKER1_PID=$!

echo "waiting for workers to complete..."
wait $WORKER0_PID $WORKER1_PID

echo "workers completed, stopping services..."
kill $PS_PID 2>/dev/null || true
kill $COORDINATOR_PID 2>/dev/null || true
wait $PS_PID 2>/dev/null || true
wait $COORDINATOR_PID 2>/dev/null || true

echo ""
echo "coordinator log:"
echo "---"
cat /tmp/coordinator.log
echo ""
echo "parameter server log:"
echo "---"
cat /tmp/ps.log
echo ""
echo "worker 0 log:"
echo "---"
cat /tmp/worker0.log
echo ""
echo "worker 1 log:"
echo "---"
cat /tmp/worker1.log

