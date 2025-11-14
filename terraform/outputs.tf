output "coordinator_ip" {
  description = "Public IP address of coordinator instance"
  value       = aws_instance.coordinator.public_ip
}

output "parameter_server_ip" {
  description = "Public IP address of parameter server instance"
  value       = aws_instance.parameter_server.public_ip
}

output "worker_ips" {
  description = "Public IP addresses of worker instances"
  value       = aws_instance.worker[*].public_ip
}

output "coordinator_private_ip" {
  description = "Private IP address of coordinator instance"
  value       = aws_instance.coordinator.private_ip
}

output "parameter_server_private_ip" {
  description = "Private IP address of parameter server instance"
  value       = aws_instance.parameter_server.private_ip
}

output "worker_private_ips" {
  description = "Private IP addresses of worker instances"
  value       = aws_instance.worker[*].private_ip
}

output "coordinator_address" {
  description = "Coordinator gRPC address"
  value       = "${aws_instance.coordinator.private_ip}:50052"
}

output "parameter_server_address" {
  description = "Parameter server gRPC address"
  value       = "${aws_instance.parameter_server.private_ip}:50051"
}

