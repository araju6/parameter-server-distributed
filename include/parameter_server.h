#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

struct tensor {
  std::string name;
  std::vector<int32_t> shape;
  std::vector<float> data;
  int32_t dtype;
};

// Central parameter server that coordinates distributed training.
class ParameterServerCore {
  public:
  
    ParameterServerCore(int total_workers);
    ~ParameterServerCore();

    void initialize_parameters(const std::vector<tensor>& initial_params);
    
    // receive gradients from a worker and aggregate when all workers have sent theirs
    bool receive_gradients(int32_t worker_id, int32_t iteration, const std::vector<tensor>& gradients);
    
    //send the current model parameters to workers
    std::vector<tensor> serve_parameters(int32_t iteration);
    
    bool check_sync_status(int32_t iteration, int32_t& workers_received);
    
    int get_total_workers() const { return total_workers_; }

  private:
    void aggregate_gradients(const std::vector<tensor>& gradients);
    
    int total_workers_;
    std::vector<tensor> parameters_;
    std::mutex params_mutex_;
    
    struct iteration_state {
      std::unordered_map<int32_t, std::vector<tensor>> worker_gradients;
      bool aggregated;
    };
    
    std::unordered_map<int32_t, iteration_state> iteration_states_;
    std::mutex state_mutex_;
    int32_t current_iteration_;
};

