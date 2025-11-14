#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TERRAFORM_DIR="$PROJECT_ROOT/terraform"
BUILD_DIR="$PROJECT_ROOT/build"
KEY_FILE="${KEY_FILE:-}"
SSH_USER="${SSH_USER:-ubuntu}"
ACTION="${1:-}"
NEW_COUNT="${2:-}"

if [ -z "$KEY_FILE" ]; then
  echo "error: KEY_FILE environment variable not set"
  echo "usage: KEY_FILE=/path/to/key.pem ./scripts/scale_workers.sh <up|down> <count>"
  exit 1
fi

if [ ! -f "$KEY_FILE" ]; then
  echo "error: key file not found: $KEY_FILE"
  exit 1
fi

if [ -z "$ACTION" ] || [ -z "$NEW_COUNT" ]; then
  echo "usage: KEY_FILE=/path/to/key.pem ./scripts/scale_workers.sh <up|down> <count>"
  echo "  up <count>   - scale up to <count> workers"
  echo "  down <count> - scale down to <count> workers"
  exit 1
fi

if ! [[ "$NEW_COUNT" =~ ^[0-9]+$ ]] || [ "$NEW_COUNT" -lt 0 ]; then
  echo "error: count must be a non-negative integer"
  exit 1
fi

chmod 400 "$KEY_FILE"

cd "$TERRAFORM_DIR"

CURRENT_COUNT=$(terraform output -json worker_ips 2>/dev/null | jq 'length' || echo "0")
PS_IP=$(terraform output -raw parameter_server_ip 2>/dev/null || echo "")

if [ -z "$PS_IP" ]; then
  echo "error: parameter server not found. run deploy.sh first"
  exit 1
fi

echo "current worker count: $CURRENT_COUNT"
echo "target worker count: $NEW_COUNT"

if [ "$ACTION" = "up" ]; then
  if [ "$NEW_COUNT" -le "$CURRENT_COUNT" ]; then
    echo "error: new count ($NEW_COUNT) must be greater than current count ($CURRENT_COUNT)"
    exit 1
  fi
  
  echo "=== scaling up workers from $CURRENT_COUNT to $NEW_COUNT ==="
  
  terraform apply -var="worker_count=$NEW_COUNT" -auto-approve
  
  echo "=== waiting for new instances to be ready ==="
  sleep 10
  
  wait_for_ssh() {
    local ip=$1
    local max_attempts=30
    local attempt=0
    
    echo "waiting for SSH on $ip..."
    while [ $attempt -lt $max_attempts ]; do
      if ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no -o ConnectTimeout=5 "$SSH_USER@$ip" "echo 'connected'" 2>/dev/null; then
        echo "SSH ready on $ip"
        return 0
      fi
      attempt=$((attempt + 1))
      sleep 5
    done
    
    echo "error: SSH not ready on $ip after $max_attempts attempts"
    return 1
  }
  
  copy_worker_files() {
    local ip=$1
    local worker_id=$2
    
    echo "copying files to worker $worker_id ($ip)..."
    
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mkdir -p /opt/parameter-server && sudo chmod 755 /opt/parameter-server"
    
    scp -i "$KEY_FILE" -o StrictHostKeyChecking=no \
      "$BUILD_DIR/bin/worker_main" \
      "$SSH_USER@$ip:/tmp/"
    
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mv /tmp/worker_main /opt/parameter-server/ && sudo chmod +x /opt/parameter-server/worker_main"
    
    scp -i "$KEY_FILE" -o StrictHostKeyChecking=no \
      "$PROJECT_ROOT/scripts/start_worker.sh" \
      "$SSH_USER@$ip:/tmp/"
    
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mv /tmp/start_worker.sh /opt/parameter-server/ && sudo chmod +x /opt/parameter-server/start_worker.sh"
  }
  
  start_worker() {
    local ip=$1
    local worker_id=$2
    
    COORDINATOR_PRIVATE_IP=$(terraform output -raw coordinator_private_ip)
    COORDINATOR_ADDR="$COORDINATOR_PRIVATE_IP:50052"
    
    echo "starting worker $worker_id on $ip..."
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" \
      "sudo WORKER_ID=$worker_id COORDINATOR_ADDR=$COORDINATOR_ADDR ITERATIONS=100 BINARY_PATH=/opt/parameter-server/worker_main LOG_FILE=/var/log/worker_${worker_id}.log /opt/parameter-server/start_worker.sh"
  }
  
  NEW_WORKER_IPS=$(terraform output -json worker_ips | jq -r ".[$CURRENT_COUNT:]")
  
  for worker_ip in $NEW_WORKER_IPS; do
    wait_for_ssh "$worker_ip"
  done
  
  echo "=== copying binaries to new workers ==="
  worker_id=$CURRENT_COUNT
  for worker_ip in $NEW_WORKER_IPS; do
    copy_worker_files "$worker_ip" "$worker_id"
    worker_id=$((worker_id + 1))
  done
  
  echo "=== starting new workers ==="
  worker_id=$CURRENT_COUNT
  for worker_ip in $NEW_WORKER_IPS; do
    start_worker "$worker_ip" "$worker_id"
    worker_id=$((worker_id + 1))
    sleep 2
  done
  
  echo "=== updating parameter server worker count ==="
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$PS_IP" \
    "sudo pkill -f parameter_server"
  sleep 2
  
  COORDINATOR_PRIVATE_IP=$(terraform output -raw coordinator_private_ip)
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$PS_IP" \
    "sudo PS_PORT=50051 TOTAL_WORKERS=$NEW_COUNT CHECKPOINT_INTERVAL=10 BINARY_PATH=/opt/parameter-server/parameter_server LOG_FILE=/var/log/parameter_server.log /opt/parameter-server/start_parameter_server.sh"
  
  echo ""
  echo "=== scale up complete ==="
  echo "new worker count: $NEW_COUNT"
  
elif [ "$ACTION" = "down" ]; then
  if [ "$NEW_COUNT" -ge "$CURRENT_COUNT" ]; then
    echo "error: new count ($NEW_COUNT) must be less than current count ($CURRENT_COUNT)"
    exit 1
  fi
  
  if [ "$NEW_COUNT" -eq 0 ]; then
    echo "error: cannot scale down to 0 workers"
    exit 1
  fi
  
  echo "=== scaling down workers from $CURRENT_COUNT to $NEW_COUNT ==="
  echo "warning: this will terminate $(($CURRENT_COUNT - $NEW_COUNT)) worker instances"
  
  WORKER_IPS=$(terraform output -json worker_ips | jq -r '.[]')
  WORKER_IPS_ARRAY=($WORKER_IPS)
  
  WORKERS_TO_REMOVE=$(($CURRENT_COUNT - $NEW_COUNT))
  
  echo "stopping workers that will be removed..."
  for i in $(seq $NEW_COUNT $(($CURRENT_COUNT - 1))); do
    worker_ip="${WORKER_IPS_ARRAY[$i]}"
    echo "stopping worker $i on $worker_ip..."
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$worker_ip" \
      "sudo pkill -f worker_main" 2>/dev/null || true
  done
  
  echo "=== updating terraform state ==="
  terraform apply -var="worker_count=$NEW_COUNT" -auto-approve
  
  echo "=== updating parameter server worker count ==="
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$PS_IP" \
    "sudo pkill -f parameter_server"
  sleep 2
  
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$PS_IP" \
    "sudo PS_PORT=50051 TOTAL_WORKERS=$NEW_COUNT CHECKPOINT_INTERVAL=10 BINARY_PATH=/opt/parameter-server/parameter_server LOG_FILE=/var/log/parameter_server.log /opt/parameter-server/start_parameter_server.sh"
  
  echo ""
  echo "=== scale down complete ==="
  echo "new worker count: $NEW_COUNT"
  echo "note: coordinator will automatically remove stale workers via heartbeat timeout"
  
else
  echo "error: invalid action '$ACTION'. use 'up' or 'down'"
  exit 1
fi

echo ""
echo "current worker IPs:"
terraform output -json worker_ips | jq -r '.[]' | nl -v 0

