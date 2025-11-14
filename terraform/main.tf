terraform {
  required_version = ">= 1.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
}

data "aws_vpc" "default" {
  default = true
}

data "aws_subnets" "default" {
  filter {
    name   = "vpc-id"
    values = [data.aws_vpc.default.id]
  }
}

resource "aws_security_group" "training_cluster" {
  name        = "parameter-server-training-sg"
  description = "security group for parameter server and workers"
  vpc_id      = data.aws_vpc.default.id

  ingress {
    description = "gRPC port for parameter server"
    from_port   = 50051
    to_port     = 50051
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  ingress {
    description = "gRPC port for coordinator"
    from_port   = 50052
    to_port     = 50052
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  ingress {
    description = "SSH"
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }

  tags = {
    Name = "parameter-server-training-sg"
  }
}

resource "aws_instance" "coordinator" {
  ami           = var.ami_id
  instance_type = var.coordinator_instance_type
  key_name      = var.key_pair_name

  vpc_security_group_ids = [aws_security_group.training_cluster.id]

  tags = {
    Name = "coordinator"
    Role = "coordinator"
  }

  user_data = file("${path.module}/user_data.sh")
}

resource "aws_instance" "parameter_server" {
  ami           = var.ami_id
  instance_type = var.ps_instance_type
  key_name      = var.key_pair_name

  vpc_security_group_ids = [aws_security_group.training_cluster.id]

  tags = {
    Name = "parameter-server"
    Role = "parameter-server"
  }

  user_data = file("${path.module}/user_data.sh")
}

resource "aws_instance" "worker" {
  count         = var.worker_count
  ami           = var.ami_id
  instance_type = var.worker_instance_type
  key_name      = var.key_pair_name

  vpc_security_group_ids = [aws_security_group.training_cluster.id]

  tags = {
    Name = "worker-${count.index}"
    Role = "worker"
  }

  user_data = file("${path.module}/user_data.sh")
}

