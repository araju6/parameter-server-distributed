#!/bin/bash
set -e

WORKER_ID=${WORKER_ID:-0}
COORDINATOR_ADDR=${COORDINATOR_ADDR:-""}
ITERATIONS=${ITERATIONS:-100}
WORKER_ADDR=${WORKER_ADDR:-""}
WORKER_PORT=${WORKER_PORT:-0}
CHECKPOINT_PATH=${CHECKPOINT_PATH:-""}
BINARY_PATH=${BINARY_PATH:-/opt/parameter-server/worker_main}
LOG_FILE=${LOG_FILE:-/var/log/worker_${WORKER_ID}.log}

if [ -z "$COORDINATOR_ADDR" ]; then
  echo "error: COORDINATOR_ADDR not set"
  exit 1
fi

export LD_LIBRARY_PATH=/usr/local/cuda-12.3/lib64:/usr/local/nccl/lib:$LD_LIBRARY_PATH

echo "starting worker $WORKER_ID connecting to coordinator $COORDINATOR_ADDR" | tee -a "$LOG_FILE"

if [ -n "$CHECKPOINT_PATH" ]; then
  nohup "$BINARY_PATH" "$COORDINATOR_ADDR" "$WORKER_ID" "$ITERATIONS" "$WORKER_ADDR" "$WORKER_PORT" "$CHECKPOINT_PATH" > "$LOG_FILE" 2>&1 &
else
  nohup "$BINARY_PATH" "$COORDINATOR_ADDR" "$WORKER_ID" "$ITERATIONS" "$WORKER_ADDR" "$WORKER_PORT" > "$LOG_FILE" 2>&1 &
fi

echo $! > /var/run/worker_${WORKER_ID}.pid
echo "worker $WORKER_ID started with PID $(cat /var/run/worker_${WORKER_ID}.pid)"

