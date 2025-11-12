#include "coordinator.h"
#include <algorithm>

CoordinatorCore::CoordinatorCore(const std::string& ps_address, int32_t ps_port): ps_address_(ps_address), ps_port_(ps_port) {
}

bool CoordinatorCore::register_worker(const WorkerRegistryEntry& worker_info, std::string& ps_address, int32_t& total_workers) {
  std::lock_guard<std::mutex> lock(workers_mutex_);
  
  workers_[worker_info.worker_id] = worker_info;
  workers_[worker_info.worker_id].last_heartbeat = std::chrono::steady_clock::now();
  
  ps_address = ps_address_;
  total_workers = static_cast<int32_t>(workers_.size());
  
  return true;
}

bool CoordinatorCore::update_heartbeat(int32_t worker_id, int32_t status) {
  std::lock_guard<std::mutex> lock(workers_mutex_);
  
  auto it = workers_.find(worker_id);
  if (it == workers_.end()) {
    return false;
  }
  
  it->second.last_heartbeat = std::chrono::steady_clock::now();
  it->second.status = status;
  
  return true;
}

std::vector<WorkerRegistryEntry> CoordinatorCore::list_workers() {
  std::lock_guard<std::mutex> lock(workers_mutex_);
  
  std::vector<WorkerRegistryEntry> result;
  result.reserve(workers_.size());
  
  for (const auto& [id, info] : workers_) {
    result.push_back(info);
  }
  
  return result;
}

bool CoordinatorCore::get_parameter_server_address(std::string& address, int32_t& port) {
  address = ps_address_;
  port = ps_port_;
  return true;
}

void CoordinatorCore::remove_stale_workers(int64_t timeout_seconds) {
  std::lock_guard<std::mutex> lock(workers_mutex_);
  
  auto now = std::chrono::steady_clock::now();
  auto timeout = std::chrono::seconds(timeout_seconds);
  
  auto it = workers_.begin();
  while (it != workers_.end()) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_heartbeat);
    if (elapsed > timeout) {
      it = workers_.erase(it);
    } else {
      ++it;
    }
  }
}

