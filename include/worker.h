#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>

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

  // run a single sync iteration: pull -> compute -> push -> check
  bool run_iteration(int iteration);
  
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

  int worker_id_;
  std::string coordinator_address_;
  std::string ps_address_;
  std::string worker_address_;
  int32_t worker_port_;
  bool initialized_;
  
  std::thread heartbeat_thread_;
  std::atomic<bool> running_;
  std::atomic<int32_t> current_status_;
};

