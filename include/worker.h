#pragma once

#include <string>
#include <vector>
#include <memory>

struct TensorLite {
  std::string name;
  std::vector<int32_t> shape;
  std::vector<float> data;
  int32_t dtype;  // 0=float32, 1=float64
};

class Worker {
 public:
  Worker(int worker_id, const std::string& ps_address);

  // run a single sync iteration: pull -> compute -> push -> check
  bool run_iteration(int iteration);

 private:
  std::vector<TensorLite> pull_parameters(int iteration);
  std::vector<TensorLite> compute_gradients(const std::vector<TensorLite>& params);
  bool push_gradients(int iteration, const std::vector<TensorLite>& grads, int& workers_received, int& total_workers);
  bool check_sync_ready(int iteration, int& workers_received, int& total_workers);

  int worker_id_;
  std::string ps_address_;
};

