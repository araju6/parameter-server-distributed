#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>

#ifdef HAVE_NCCL
#include "nccl_manager.h"
#include <cuda_runtime.h>
#endif

struct TensorLite {
  std::string name;
  std::vector<int32_t> shape;
  std::vector<float> data;
  int32_t dtype;  // 0=float32, 1=float64
};

class Worker {
 public:
  Worker(int worker_id, const std::string& coordinator_address, const std::string& worker_address = "", int32_t worker_port = 0);
  
  bool initialize();
  bool reconnect();

  // run a single sync iteration: pull -> compute -> push -> check
  bool run_iteration(int iteration);
  
  // Load checkpoint from parameter server
  // Returns true if successful, and sets epoch to the loaded checkpoint epoch
  bool load_checkpoint_from_server(const std::string& checkpoint_path, int32_t& epoch);
  
  ~Worker();

 private:
  bool discover_parameter_server();
  bool register_with_coordinator();
  std::vector<std::string> discover_peer_workers();
  bool query_with_retry(const std::function<bool()>& query_func, int max_retries = 5);
  void send_heartbeat();
  void heartbeat_loop();
  
  std::vector<TensorLite> pull_parameters(int iteration);
  std::vector<TensorLite> compute_gradients(const std::vector<TensorLite>& params);
  bool push_gradients(int iteration, const std::vector<TensorLite>& grads, int& workers_received, int& total_workers);
  bool check_sync_ready(int iteration, int& workers_received, int& total_workers);
  
#ifdef HAVE_NCCL
  std::vector<TensorLite> aggregate_gradients_multi_gpu(const std::vector<TensorLite>& grads);
#endif

  int worker_id_;
  std::string coordinator_address_;
  std::string ps_address_;
  std::string worker_address_;
  int32_t worker_port_;
  bool initialized_;
  
  std::thread heartbeat_thread_;
  std::atomic<bool> running_;
  std::atomic<int32_t> current_status_;
  
#ifdef HAVE_NCCL
  NCCLManager* nccl_manager_;
  int num_gpus_;
#endif
};

