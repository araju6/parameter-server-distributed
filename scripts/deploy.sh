#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TERRAFORM_DIR="$PROJECT_ROOT/terraform"
BUILD_DIR="$PROJECT_ROOT/build"
KEY_FILE="${KEY_FILE:-}"
SSH_USER="${SSH_USER:-ubuntu}"
ITERATIONS="${ITERATIONS:-100}"
CHECKPOINT_INTERVAL="${CHECKPOINT_INTERVAL:-10}"

if [ -z "$KEY_FILE" ]; then
  echo "error: KEY_FILE environment variable not set"
  echo "usage: KEY_FILE=/path/to/key.pem ./scripts/deploy.sh"
  exit 1
fi

if [ ! -f "$KEY_FILE" ]; then
  echo "error: key file not found: $KEY_FILE"
  exit 1
fi

chmod 400 "$KEY_FILE"

echo "=== building binaries ==="
cd "$PROJECT_ROOT"
if [ ! -d "$BUILD_DIR" ]; then
  mkdir -p "$BUILD_DIR"
  cd "$BUILD_DIR"
  cmake ..
else
  cd "$BUILD_DIR"
fi
make -j$(nproc)

echo "=== deploying with terraform ==="
cd "$TERRAFORM_DIR"
terraform init
terraform apply -auto-approve

echo "=== waiting for instances to be ready ==="
sleep 10

COORDINATOR_IP=$(terraform output -raw coordinator_ip)
PS_IP=$(terraform output -raw parameter_server_ip)
WORKER_IPS=$(terraform output -json worker_ips | jq -r '.[]')
COORDINATOR_PRIVATE_IP=$(terraform output -raw coordinator_private_ip)
PS_PRIVATE_IP=$(terraform output -raw parameter_server_private_ip)
WORKER_COUNT=$(terraform output -json worker_ips | jq 'length')

COORDINATOR_ADDR="$COORDINATOR_PRIVATE_IP:50052"
PS_ADDR="$PS_PRIVATE_IP:50051"

echo "coordinator: $COORDINATOR_IP"
echo "parameter server: $PS_IP"
echo "workers: $WORKER_IPS"
echo "coordinator address: $COORDINATOR_ADDR"
echo "parameter server address: $PS_ADDR"

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

copy_files() {
  local ip=$1
  local role=$2
  
  echo "copying files to $role ($ip)..."
  
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mkdir -p /opt/parameter-server && sudo chmod 755 /opt/parameter-server"
  
  local binary_name="$role"
  if [ "$role" = "worker" ]; then
    binary_name="worker_main"
  fi
  
  scp -i "$KEY_FILE" -o StrictHostKeyChecking=no \
    "$BUILD_DIR/bin/$binary_name" \
    "$SSH_USER@$ip:/tmp/"
  
  local target_name="$role"
  if [ "$role" = "worker" ]; then
    target_name="worker_main"
  fi
  
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mv /tmp/$binary_name /opt/parameter-server/$target_name && sudo chmod +x /opt/parameter-server/$target_name"
  
  if [ "$role" = "parameter_server" ]; then
    scp -i "$KEY_FILE" -o StrictHostKeyChecking=no \
      "$PROJECT_ROOT/scripts/start_parameter_server.sh" \
      "$SSH_USER@$ip:/tmp/"
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mv /tmp/start_parameter_server.sh /opt/parameter-server/ && sudo chmod +x /opt/parameter-server/start_parameter_server.sh"
  elif [ "$role" = "worker" ]; then
    scp -i "$KEY_FILE" -o StrictHostKeyChecking=no \
      "$PROJECT_ROOT/scripts/start_worker.sh" \
      "$SSH_USER@$ip:/tmp/"
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mv /tmp/start_worker.sh /opt/parameter-server/ && sudo chmod +x /opt/parameter-server/start_worker.sh"
  elif [ "$role" = "coordinator" ]; then
    scp -i "$KEY_FILE" -o StrictHostKeyChecking=no \
      "$PROJECT_ROOT/scripts/start_coordinator.sh" \
      "$SSH_USER@$ip:/tmp/"
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" "sudo mv /tmp/start_coordinator.sh /opt/parameter-server/ && sudo chmod +x /opt/parameter-server/start_coordinator.sh"
  fi
}

start_service() {
  local ip=$1
  local role=$2
  local worker_id=${3:-0}
  
  echo "starting $role on $ip..."
  
  if [ "$role" = "coordinator" ]; then
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" \
      "sudo COORDINATOR_PORT=50052 BINARY_PATH=/opt/parameter-server/coordinator LOG_FILE=/var/log/coordinator.log /opt/parameter-server/start_coordinator.sh"
  elif [ "$role" = "parameter_server" ]; then
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" \
      "sudo PS_PORT=50051 TOTAL_WORKERS=$WORKER_COUNT CHECKPOINT_INTERVAL=$CHECKPOINT_INTERVAL BINARY_PATH=/opt/parameter-server/parameter_server LOG_FILE=/var/log/parameter_server.log /opt/parameter-server/start_parameter_server.sh"
  elif [ "$role" = "worker" ]; then
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$ip" \
      "sudo WORKER_ID=$worker_id COORDINATOR_ADDR=$COORDINATOR_ADDR ITERATIONS=$ITERATIONS BINARY_PATH=/opt/parameter-server/worker_main LOG_FILE=/var/log/worker_${worker_id}.log /opt/parameter-server/start_worker.sh"
  fi
}

echo "=== waiting for SSH access ==="
wait_for_ssh "$COORDINATOR_IP"
wait_for_ssh "$PS_IP"
for worker_ip in $WORKER_IPS; do
  wait_for_ssh "$worker_ip"
done

echo "=== copying binaries ==="
copy_files "$COORDINATOR_IP" "coordinator"
copy_files "$PS_IP" "parameter_server"
worker_id=0
for worker_ip in $WORKER_IPS; do
  copy_files "$worker_ip" "worker"
  worker_id=$((worker_id + 1))
done

echo "=== starting services ==="
start_service "$COORDINATOR_IP" "coordinator"
sleep 5
start_service "$PS_IP" "parameter_server"
sleep 5
worker_id=0
for worker_ip in $WORKER_IPS; do
  start_service "$worker_ip" "worker" "$worker_id"
  worker_id=$((worker_id + 1))
  sleep 2
done

echo ""
echo "=== deployment complete ==="
echo "coordinator: $COORDINATOR_IP (logs: ssh $SSH_USER@$COORDINATOR_IP 'tail -f /var/log/coordinator.log')"
echo "parameter server: $PS_IP (logs: ssh $SSH_USER@$PS_IP 'tail -f /var/log/parameter_server.log')"
echo "workers:"
worker_id=0
for worker_ip in $WORKER_IPS; do
  echo "  worker $worker_id: $worker_ip (logs: ssh $SSH_USER@$worker_ip 'tail -f /var/log/worker_${worker_id}.log')"
  worker_id=$((worker_id + 1))
done
echo ""
echo "to destroy infrastructure: cd terraform && terraform destroy"

