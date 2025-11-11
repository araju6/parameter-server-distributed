#include "parameter_server.h"

#include <algorithm>
#include <numeric>

ParameterServerCore::ParameterServerCore(int total_workers) : total_workers_(total_workers), current_iteration_(0) {}

ParameterServerCore::~ParameterServerCore() {}

void ParameterServerCore::initialize_parameters(const std::vector<tensor>& initial_params) {
  std::lock_guard<std::mutex> lock(params_mutex_);
  parameters_ = initial_params;
}

bool ParameterServerCore::receive_gradients(int32_t worker_id, int32_t iteration,const std::vector<tensor>& gradients) {
  
    std::lock_guard<std::mutex> lock(state_mutex_);
  
  if (iteration > current_iteration_) {
    current_iteration_ = iteration;
  }
  
  auto& state = iteration_states_[iteration];
  state.worker_gradients[worker_id] = gradients;
  
  if (state.worker_gradients.size() == static_cast<size_t>(total_workers_)) {
    std::vector<tensor> aggregated;
    
    for (size_t i = 0; i < gradients.size(); ++i) {
      tensor agg_tensor;
      agg_tensor.name = gradients[i].name;
      agg_tensor.shape = gradients[i].shape;
      agg_tensor.dtype = gradients[i].dtype;
      
      size_t data_size = gradients[i].data.size();
      agg_tensor.data.resize(data_size, 0.0f);
      
      for (const auto& [wid, grad_vec] : state.worker_gradients) {
        if (i < grad_vec.size()) {
          for (size_t j = 0; j < data_size && j < grad_vec[i].data.size(); ++j) {
            agg_tensor.data[j] += grad_vec[i].data[j];
          }
        }
      }
      
      for (size_t j = 0; j < data_size; ++j) {
        agg_tensor.data[j] /= total_workers_;
      }
      
      aggregated.push_back(agg_tensor);
    }
    
        {
        std::lock_guard<std::mutex> params_lock(params_mutex_);
        ParameterServerCore::aggregate_gradients(aggregated);
        }
    
    state.aggregated = true;
    return true;
  }
  
  return false;
}

void ParameterServerCore::aggregate_gradients(const std::vector<tensor>& gradients) {
  if (parameters_.empty()) {
    parameters_ = gradients;
    return;
  }
  
  for (size_t i = 0; i < gradients.size() && i < parameters_.size(); ++i) {
    if (gradients[i].name == parameters_[i].name &&
        gradients[i].shape == parameters_[i].shape) {
      for (size_t j = 0; j < gradients[i].data.size() && j < parameters_[i].data.size(); ++j) {
        parameters_[i].data[j] -= gradients[i].data[j]; // can add learning rate here
      }
    }
  }
}

std::vector<tensor> ParameterServerCore::serve_parameters(int32_t iteration) {
  std::lock_guard<std::mutex> lock(params_mutex_);
  std::vector<tensor> result = parameters_;
  return result;
}

bool ParameterServerCore::check_sync_status(int32_t iteration, int32_t& workers_received) {
  std::lock_guard<std::mutex> lock(state_mutex_);
  
  auto it = iteration_states_.find(iteration);
  if (it == iteration_states_.end()) {
    workers_received = 0;
    return false;
  }
  
  workers_received = static_cast<int32_t>(it->second.worker_gradients.size());
  return it->second.aggregated;
}

