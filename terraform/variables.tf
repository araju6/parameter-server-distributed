variable "aws_region" {
  description = "AWS region for deployment"
  type        = string
  default     = "us-east-1"
}

variable "ami_id" {
  description = "AMI ID for EC2 instances (should be Ubuntu with CUDA support)"
  type        = string
  default     = ""
}

variable "key_pair_name" {
  description = "Name of AWS key pair for SSH access"
  type        = string
}

variable "coordinator_instance_type" {
  description = "EC2 instance type for coordinator"
  type        = string
  default     = "t3.medium"
}

variable "ps_instance_type" {
  description = "EC2 instance type for parameter server"
  type        = string
  default     = "g4dn.xlarge"
}

variable "worker_instance_type" {
  description = "EC2 instance type for workers"
  type        = string
  default     = "g4dn.xlarge"
}

variable "worker_count" {
  description = "Number of worker instances"
  type        = number
  default     = 3
}

