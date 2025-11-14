#!/bin/bash
set -e

COORDINATOR_PORT=${COORDINATOR_PORT:-50052}
BINARY_PATH=${BINARY_PATH:-/opt/parameter-server/coordinator}
LOG_FILE=${LOG_FILE:-/var/log/coordinator.log}

export LD_LIBRARY_PATH=/usr/local/cuda-12.3/lib64:/usr/local/nccl/lib:$LD_LIBRARY_PATH

echo "starting coordinator on port $COORDINATOR_PORT" | tee -a "$LOG_FILE"
nohup "$BINARY_PATH" "0.0.0.0:$COORDINATOR_PORT" > "$LOG_FILE" 2>&1 &
echo $! > /var/run/coordinator.pid
echo "coordinator started with PID $(cat /var/run/coordinator.pid)"

