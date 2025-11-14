# Deployment Scripts

## Prerequisites

- Terraform installed
- AWS CLI configured
- `jq` installed (`apt-get install jq` or `brew install jq`)
- SSH key pair configured in AWS
- Binaries built (run `cmake .. && make` in `build/` directory)

## Startup Scripts

### `start_coordinator.sh`
Starts the coordinator service. Environment variables:
- `COORDINATOR_PORT`: Port to listen on (default: 50052)
- `BINARY_PATH`: Path to coordinator binary (default: /opt/parameter-server/coordinator)
- `LOG_FILE`: Log file path (default: /var/log/coordinator.log)

### `start_parameter_server.sh`
Starts the parameter server. Environment variables:
- `PS_PORT`: Port to listen on (default: 50051)
- `TOTAL_WORKERS`: Number of workers (default: 3)
- `CHECKPOINT_INTERVAL`: Checkpoint every N iterations (default: 10)
- `BINARY_PATH`: Path to parameter_server binary
- `LOG_FILE`: Log file path

### `start_worker.sh`
Starts a worker. Environment variables:
- `WORKER_ID`: Worker ID (required)
- `COORDINATOR_ADDR`: Coordinator address (required, e.g., "10.0.1.5:50052")
- `ITERATIONS`: Number of training iterations (default: 100)
- `WORKER_ADDR`: Worker address (optional)
- `WORKER_PORT`: Worker port (optional)
- `CHECKPOINT_PATH`: Path to checkpoint file for recovery (optional)
- `BINARY_PATH`: Path to worker_main binary
- `LOG_FILE`: Log file path

## Deployment Script

### `deploy.sh`

Automated deployment script that:
1. Builds the binaries
2. Runs `terraform apply` to create infrastructure
3. Waits for instances to be ready
4. Copies binaries and startup scripts to instances
5. Starts all services

**Usage:**
```bash
KEY_FILE=/path/to/key.pem ./scripts/deploy.sh
```

**Environment Variables:**
- `KEY_FILE`: Path to AWS key pair file (required)
- `SSH_USER`: SSH user (default: ubuntu)
- `ITERATIONS`: Number of training iterations (default: 100)
- `CHECKPOINT_INTERVAL`: Checkpoint interval (default: 10)

**Example:**
```bash
KEY_FILE=~/.ssh/my-key.pem ITERATIONS=200 ./scripts/deploy.sh
```

The script will output:
- IP addresses of all instances
- Commands to view logs on each instance
- Instructions to destroy infrastructure

**Viewing Logs:**
```bash
# Coordinator
ssh -i ~/.ssh/my-key.pem ubuntu@<coordinator-ip> 'tail -f /var/log/coordinator.log'

# Parameter Server
ssh -i ~/.ssh/my-key.pem ubuntu@<ps-ip> 'tail -f /var/log/parameter_server.log'

# Worker
ssh -i ~/.ssh/my-key.pem ubuntu@<worker-ip> 'tail -f /var/log/worker_0.log'
```

**Stopping Services:**
```bash
# On each instance
sudo pkill -f coordinator
sudo pkill -f parameter_server
sudo pkill -f worker_main
```

**Destroying Infrastructure:**
```bash
cd terraform
terraform destroy
```

