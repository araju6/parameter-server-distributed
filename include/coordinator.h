#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>
#include <cstdint>

struct WorkerRegistryEntry {
  int32_t worker_id;
  std::string address;
  int32_t port;
  std::string hostname;
  int32_t status;
  std::chrono::steady_clock::time_point last_heartbeat;
};

class CoordinatorCore {
    public:
        CoordinatorCore(const std::string& ps_address, int32_t ps_port);
        
        bool register_worker(const WorkerRegistryEntry& worker_info, std::string& ps_address, int32_t& total_workers);
        
        bool update_heartbeat(int32_t worker_id, int32_t status);
        
        std::vector<WorkerRegistryEntry> list_workers();
        
        bool get_parameter_server_address(std::string& address, int32_t& port);
        
        void remove_stale_workers(int64_t timeout_seconds = 30);

    private:
        std::string ps_address_;
        int32_t ps_port_;
        std::unordered_map<int32_t, WorkerRegistryEntry> workers_;
        std::mutex workers_mutex_;
};

