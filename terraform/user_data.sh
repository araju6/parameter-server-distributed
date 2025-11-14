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

# create directory for binaries
mkdir -p /opt/parameter-server
chmod 755 /opt/parameter-server

# note: binaries will be copied via SCP in deployment script
echo "user_data.sh completed" > /tmp/user_data_complete.txt

