#include "parameter_server.h"
#include "parameter_server_service.h"
#include <grpcpp/grpcpp.h>
#include "parameter_server.grpc.pb.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class parameter_server_service_impl final : public parameter_server::ParameterServer::Service {

  public:
    parameter_server_service_impl(int total_workers, int checkpoint_interval = 10)
      : ps_(total_workers), checkpoint_interval_(checkpoint_interval), running_(true) {
      if (checkpoint_interval_ > 0) {
        checkpoint_thread_ = std::thread(&parameter_server_service_impl::periodic_checkpoint, this);
      }
    }
    
    ~parameter_server_service_impl() {
      running_ = false;
      if (checkpoint_thread_.joinable()) {
        checkpoint_thread_.join();
      }
    }

    Status ReceiveGradients(ServerContext* context, const parameter_server::GradientUpdate* request, parameter_server::PushResponse* response) override {
      std::vector<tensor> gradients;
      
      for (const auto& proto_tensor : request->gradients()) {
        tensor t;
        t.name = proto_tensor.name();
        t.shape.assign(proto_tensor.shape().begin(), proto_tensor.shape().end());
        t.data.assign(proto_tensor.data().begin(), proto_tensor.data().end());
        t.dtype = proto_tensor.dtype();
        gradients.push_back(t);
      }
      
      bool complete = ps_.receive_gradients(request->worker_id(), 
                                            request->iteration(), 
                                            gradients);
      
      response->set_success(true);
      response->set_message("gradients received");
      response->set_iteration(request->iteration());
      response->set_aggregation_complete(complete);
      
      int32_t workers_received = 0;
      ps_.check_sync_status(request->iteration(), workers_received);
      response->set_workers_received(workers_received);
      response->set_total_workers(ps_.get_total_workers());
      
      return Status::OK;
    }

    Status ServeParameters(ServerContext* context, const parameter_server::PullRequest* request, parameter_server::ParameterUpdate* response) override {
      auto params = ps_.serve_parameters(request->iteration());
      
      int32_t workers_received = 0;
      bool ready = ps_.check_sync_status(request->iteration(), workers_received);
      
      response->set_iteration(request->iteration());
      response->set_ready(ready);
      
      for (const auto& t : params) {
        parameter_server::Tensor* proto_tensor = response->add_parameters();
        proto_tensor->set_name(t.name);
        for (int32_t dim : t.shape) {
          proto_tensor->add_shape(dim);
        }
        for (float val : t.data) {
          proto_tensor->add_data(val);
        }
        proto_tensor->set_dtype(t.dtype);
      }
      
      return Status::OK;
    }

    Status CheckSyncStatus(ServerContext* context, const parameter_server::SyncStatusRequest* request, parameter_server::SyncStatusResponse* response) override {
      int32_t workers_received = 0;
      bool ready = ps_.check_sync_status(request->iteration(), workers_received);
      
      response->set_iteration(request->iteration());
      response->set_ready(ready);
      response->set_workers_received(workers_received);
      response->set_total_workers(ps_.get_total_workers());
      
      return Status::OK;
    }

    Status SaveCheckpoint(ServerContext* context, const parameter_server::SaveCheckpointRequest* request, parameter_server::SaveCheckpointResponse* response) override {
      std::string path = request->path();
      if (path.empty()) {
        std::ostringstream oss;
        oss << "checkpoint_epoch_" << request->epoch() << ".ckpt";
        path = oss.str();
      }
      
      bool success = ps_.save_checkpoint(request->epoch(), path);
      response->set_success(success);
      if (success) {
        response->set_message("checkpoint saved");
        response->set_checkpoint_path(path);
      } else {
        response->set_message("failed to save checkpoint");
      }
      
      return Status::OK;
    }

    Status LoadCheckpoint(ServerContext* context, const parameter_server::LoadCheckpointRequest* request, parameter_server::LoadCheckpointResponse* response) override {
      int32_t epoch = 0;
      bool success = ps_.load_checkpoint(request->path(), epoch);
      
      response->set_success(success);
      if (success) {
        response->set_message("checkpoint loaded");
        response->set_epoch(epoch);
        
        auto params = ps_.serve_parameters(0);
        for (const auto& t : params) {
          parameter_server::Tensor* proto_tensor = response->add_parameters();
          proto_tensor->set_name(t.name);
          for (int32_t dim : t.shape) {
            proto_tensor->add_shape(dim);
          }
          for (float val : t.data) {
            proto_tensor->add_data(val);
          }
          proto_tensor->set_dtype(t.dtype);
        }
      } else {
        response->set_message("failed to load checkpoint");
      }
      
      return Status::OK;
    }

    ParameterServerCore& get_parameter_server() {
      return ps_;
    }

  private:
    void periodic_checkpoint() {
      int32_t last_checkpointed_epoch = -1;
      while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        int32_t current_iter = ps_.get_current_iteration();
        int32_t current_epoch = current_iter / checkpoint_interval_;
        
        if (current_epoch > last_checkpointed_epoch && current_iter > 0) {
          std::ostringstream oss;
          oss << "checkpoint_epoch_" << current_epoch << ".ckpt";
          std::string path = oss.str();
          
          if (ps_.save_checkpoint(current_epoch, path)) {
            last_checkpointed_epoch = current_epoch;
            std::cout << "saved checkpoint: " << path << " (epoch " << current_epoch << ")" << std::endl;
          }
        }
      }
    }

    ParameterServerCore ps_;
    int checkpoint_interval_;
    std::thread checkpoint_thread_;
    std::atomic<bool> running_;
};

void run_server(const std::string& server_address, int total_workers, int checkpoint_interval) {
  parameter_server_service_impl service(total_workers, checkpoint_interval);
  
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "parameter server listening on " << server_address << std::endl;
  if (checkpoint_interval > 0) {
    std::cout << "periodic checkpointing every " << checkpoint_interval << " iterations" << std::endl;
  }
  
  server->Wait();
}

