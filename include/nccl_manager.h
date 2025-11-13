#pragma once

#include <vector>
#include <memory>
#include <cstdint>

int detect_num_gpus();

class NCCLManager {
  public:
    NCCLManager();
    ~NCCLManager();
    
    bool initialize(int num_gpus);
    void cleanup();
    
    bool allreduce_float(float* data, size_t count, int gpu_id);
    
    int get_num_gpus() const { return num_gpus_; }
    bool is_initialized() const { return initialized_; }

  private:
    int num_gpus_;
    bool initialized_;
    void* comms_;
    std::vector<int> devices_;
};

