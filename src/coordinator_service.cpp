#include "coordinator.h"
#include "coordinator_service.h"
#include <grpcpp/grpcpp.h>
#include "coordinator.grpc.pb.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using coordinator::Coordinator;
using coordinator::WorkerInfo;
using coordinator::RegisterResponse;
using coordinator::HeartbeatRequest;
using coordinator::HeartbeatResponse;
using coordinator::ListWorkersRequest;
using coordinator::ListWorkersResponse;
using coordinator::GetPSAddressRequest;
using coordinator::GetPSAddressResponse;
using coordinator::WorkerStatus;

class coordinator_service_impl final : public Coordinator::Service {
  public:
    coordinator_service_impl(const std::string& ps_address, int32_t ps_port): coordinator_(ps_address, ps_port), running_(true) {
      cleanup_thread_ = std::thread(&coordinator_service_impl::cleanup_loop, this);
    }
    
    ~coordinator_service_impl() {
      running_ = false;
      if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
      }
    }

    Status RegisterWorker(ServerContext* context, const coordinator::WorkerInfo* request, RegisterResponse* response) override {
      WorkerRegistryEntry info;
      info.worker_id = request->worker_id();
      info.address = request->address();
      info.port = request->port();
      info.hostname = request->hostname();
      info.status = 0;
      
      std::string ps_addr;
      int32_t total_workers = 0;
      bool success = coordinator_.register_worker(info, ps_addr, total_workers);
      
      response->set_success(success);
      if (success) {
        response->set_message("worker registered");
        response->set_parameter_server_address(ps_addr);
        response->set_total_workers(total_workers);
      } else {
        response->set_message("registration failed");
      }
      
      return Status::OK;
    }

    Status Heartbeat(ServerContext* context, const HeartbeatRequest* request, HeartbeatResponse* response) override {
      bool success = coordinator_.update_heartbeat(request->worker_id(), request->status());
      
      response->set_success(success);
      auto now = std::chrono::system_clock::now();
      auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
      response->set_timestamp(timestamp);
      
      return Status::OK;
    }

    Status ListWorkers(ServerContext* context, const ListWorkersRequest* request, ListWorkersResponse* response) override {
      auto workers = coordinator_.list_workers();
      
      for (const auto& w : workers) {
        coordinator::WorkerInfo* worker_info = response->add_workers();
        worker_info->set_worker_id(w.worker_id);
        worker_info->set_address(w.address);
        worker_info->set_port(w.port);
        worker_info->set_hostname(w.hostname);
      }
      
      response->set_total_workers(static_cast<int32_t>(workers.size()));
      
      return Status::OK;
    }

    Status GetParameterServerAddress(ServerContext* context, const GetPSAddressRequest* request, GetPSAddressResponse* response) override {
      std::string address;
      int32_t port;
      coordinator_.get_parameter_server_address(address, port);
      
      response->set_address(address);
      response->set_port(port);
      
      return Status::OK;
    }

  private:
    void cleanup_loop() {
      while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        coordinator_.remove_stale_workers(30);
      }
    }

    CoordinatorCore coordinator_;
    std::thread cleanup_thread_;
    std::atomic<bool> running_;
};

void run_coordinator_server(const std::string& server_address, const std::string& ps_address, int32_t ps_port) {
  coordinator_service_impl service(ps_address, ps_port);
  
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "coordinator listening on " << server_address << std::endl;
  std::cout << "parameter server: " << ps_address << ":" << ps_port << std::endl;
  
  server->Wait();
}

