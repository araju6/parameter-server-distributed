#include "nccl_manager.h"

#ifdef HAVE_NCCL
#include <nccl.h>
#include <cuda_runtime.h>
#endif

#include <iostream>
#include <stdexcept>

int detect_num_gpus() {
    #ifdef HAVE_NCCL
    int count = 0;
    cudaGetDeviceCount(&count);
    return count;
    #else
    return 0;
    #endif
}

NCCLManager::NCCLManager() : num_gpus_(0), initialized_(false), comms_(nullptr) {
}

NCCLManager::~NCCLManager() {
  cleanup();
}

bool NCCLManager::initialize(int num_gpus) {
#ifdef HAVE_NCCL
  if (initialized_) {
    cleanup();
  }
  
  if (num_gpus <= 1) {
    return false;
  }
  
  num_gpus_ = num_gpus;
  devices_.resize(num_gpus_);
  
  for (int i = 0; i < num_gpus_; ++i) {
    devices_[i] = i;
  }
  
  ncclComm_t* comms = new ncclComm_t[num_gpus_];
  
  ncclUniqueId nccl_id;
  if (ncclGetUniqueId(&nccl_id) != ncclSuccess) {
    delete[] comms;
    return false;
  }
  
  ncclResult_t result = ncclGroupStart();
  if (result != ncclSuccess) {
    delete[] comms;
    return false;
  }
  
  for (int i = 0; i < num_gpus_; ++i) {
    cudaSetDevice(devices_[i]);
    result = ncclCommInitRank(&comms[i], num_gpus_, nccl_id, i);
    if (result != ncclSuccess) {
      ncclGroupEnd();
      delete[] comms;
      return false;
    }
  }
  
  result = ncclGroupEnd();
  if (result != ncclSuccess) {
    for (int i = 0; i < num_gpus_; ++i) {
      ncclCommDestroy(comms[i]);
    }
    delete[] comms;
    return false;
  }
  
  comms_ = comms;
  initialized_ = true;
  
  return true;
#else
  return false;
#endif
}

void NCCLManager::cleanup() {
#ifdef HAVE_NCCL
  if (initialized_ && comms_) {
    ncclComm_t* comms = static_cast<ncclComm_t*>(comms_);
    for (int i = 0; i < num_gpus_; ++i) {
      ncclCommDestroy(comms[i]);
    }
    delete[] comms;
    comms_ = nullptr;
  }
#endif
  initialized_ = false;
  num_gpus_ = 0;
}

bool NCCLManager::allreduce_float(float* data, size_t count, int gpu_id) {
#ifdef HAVE_NCCL
  if (!initialized_ || gpu_id < 0 || gpu_id >= num_gpus_) {
    return false;
  }
  
  ncclComm_t* comms = static_cast<ncclComm_t*>(comms_);
  cudaSetDevice(devices_[gpu_id]);
  
  ncclResult_t result = ncclAllReduce(data, data, count, ncclFloat, ncclSum, comms[gpu_id], nullptr);
  
  if (result != ncclSuccess) {
    return false;
  }
  
  return true;
#else
  return false;
#endif
}

