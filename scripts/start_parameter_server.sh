#!/bin/bash
set -e

PS_PORT=${PS_PORT:-50051}
TOTAL_WORKERS=${TOTAL_WORKERS:-3}
CHECKPOINT_INTERVAL=${CHECKPOINT_INTERVAL:-10}
BINARY_PATH=${BINARY_PATH:-/opt/parameter-server/parameter_server}
LOG_FILE=${LOG_FILE:-/var/log/parameter_server.log}

export LD_LIBRARY_PATH=/usr/local/cuda-12.3/lib64:/usr/local/nccl/lib:$LD_LIBRARY_PATH

echo "starting parameter server on port $PS_PORT with $TOTAL_WORKERS workers" | tee -a "$LOG_FILE"
nohup "$BINARY_PATH" "0.0.0.0:$PS_PORT" "$TOTAL_WORKERS" "$CHECKPOINT_INTERVAL" > "$LOG_FILE" 2>&1 &
echo $! > /var/run/parameter_server.pid
echo "parameter server started with PID $(cat /var/run/parameter_server.pid)"

