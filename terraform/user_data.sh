#!/bin/bash
set -e

apt-get update
apt-get install -y build-essential cmake git wget

# install CUDA toolkit (simplified - assumes NVIDIA driver is pre-installed on AMI)
if ! command -v nvcc &> /dev/null; then
  wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
  dpkg -i cuda-keyring_1.1-1_all.deb
  apt-get update
  apt-get -y install cuda-toolkit-12-3
  export PATH=/usr/local/cuda-12.3/bin:$PATH
  export LD_LIBRARY_PATH=/usr/local/cuda-12.3/lib64:$LD_LIBRARY_PATH
fi

# install NCCL
if [ ! -d "/usr/local/nccl" ]; then
  wget https://developer.download.nvidia.com/compute/redist/nccl/v2.20.3/nccl_2.20.3-1+cuda12.3_x86_64.txz
  tar -xJf nccl_2.20.3-1+cuda12.3_x86_64.txz
  mv nccl_2.20.3-1+cuda12.3_x86_64 /usr/local/nccl
  export LD_LIBRARY_PATH=/usr/local/nccl/lib:$LD_LIBRARY_PATH
fi

# install gRPC and protobuf dependencies
apt-get install -y libprotobuf-dev protobuf-compiler libgrpc++-dev libgrpc-dev

# create directory for binaries and logs
mkdir -p /opt/parameter-server
mkdir -p /var/log
mkdir -p /var/run
chmod 755 /opt/parameter-server

# create systemd service files (will be enabled by deployment script)
cat > /etc/systemd/system/coordinator.service << 'EOF'
[Unit]
Description=Parameter Server Coordinator
After=network.target

[Service]
Type=simple
ExecStart=/opt/parameter-server/start_coordinator.sh
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

cat > /etc/systemd/system/parameter_server.service << 'EOF'
[Unit]
Description=Parameter Server
After=network.target

[Service]
Type=simple
ExecStart=/opt/parameter-server/start_parameter_server.sh
Restart=always
RestartSec=10
Environment="TOTAL_WORKERS=3"
Environment="CHECKPOINT_INTERVAL=10"

[Install]
WantedBy=multi-user.target
EOF

cat > /etc/systemd/system/worker.service << 'EOF'
[Unit]
Description=Parameter Server Worker
After=network.target

[Service]
Type=simple
ExecStart=/opt/parameter-server/start_worker.sh
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# note: binaries and startup scripts will be copied via SCP in deployment script
echo "user_data.sh completed" > /tmp/user_data_complete.txt

