# Terraform Configuration for Parameter Server Deployment

This directory contains Terraform configuration files to deploy the parameter server training cluster on AWS EC2.

## Prerequisites

1. AWS CLI configured with credentials
2. Terraform installed (>= 1.0)
3. An AWS key pair for SSH access
4. An AMI with Ubuntu 22.04 and NVIDIA drivers pre-installed (or use Deep Learning AMI)

## Setup

1. Copy `terraform.tfvars.example` to `terraform.tfvars`:
   ```bash
   cp terraform.tfvars.example terraform.tfvars
   ```

2. Edit `terraform.tfvars` with your values:
   - `aws_region`: Your preferred AWS region
   - `ami_id`: AMI ID (can find Deep Learning AMI in AWS console)
   - `key_pair_name`: Your AWS key pair name
   - `worker_count`: Number of worker instances (default: 3)

## Usage

Initialize Terraform:
```bash
cd terraform
terraform init
```

Plan deployment:
```bash
terraform plan
```

Apply deployment:
```bash
terraform apply
```

After deployment, Terraform will output:
- `coordinator_ip`: Public IP of coordinator
- `parameter_server_ip`: Public IP of parameter server
- `worker_ips`: List of worker public IPs
- `coordinator_address`: Private IP:port for coordinator
- `parameter_server_address`: Private IP:port for parameter server

Destroy infrastructure:
```bash
terraform destroy
```

## Architecture

- 1 coordinator instance (t3.medium)
- 1 parameter server instance (g4dn.xlarge)
- 3 worker instances (g4dn.xlarge each)

All instances use the default VPC and are in the same security group allowing:
- Port 50051 (parameter server gRPC)
- Port 50052 (coordinator gRPC)
- Port 22 (SSH)

## Notes

- The `user_data.sh` script installs CUDA, NCCL, and gRPC dependencies
- Binaries need to be copied to instances separately (see deployment script)
- Instances use private IPs for inter-instance communication

## Elastic Scaling

The `worker_count` variable can be changed to scale workers up or down:

```bash
# Scale to 5 workers
terraform apply -var="worker_count=5"

# Scale to 2 workers
terraform apply -var="worker_count=2"
```

**Note:** After changing worker count, you need to:
1. Restart the parameter server with the new worker count
2. New workers will automatically register with the coordinator
3. Use `scripts/scale_workers.sh` for automated scaling (recommended)

