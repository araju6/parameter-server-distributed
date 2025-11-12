#include "parameter_server.h"
#include "parameter_server_service.h"
#include <grpcpp/grpcpp.h>
#include "parameter_server.grpc.pb.h"
#include <iostream>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

class parameter_server_service_impl final : public parameter_server::ParameterServer::Service {

  public:
    parameter_server_service_impl(int total_workers): ps_(total_workers) {}

    Status PushGradients(ServerContext* context, const parameter_server::GradientUpdate* request, parameter_server::PushResponse* response) override {
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

    Status PullParameters(ServerContext* context, const parameter_server::PullRequest* request, parameter_server::ParameterUpdate* response) override {
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

    ParameterServerCore& get_parameter_server() {
      return ps_;
    }

  private:
    ParameterServerCore ps_;
};

void run_server(const std::string& server_address, int total_workers) {
  parameter_server_service_impl service(total_workers);
  
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "parameter server listening on " << server_address << std::endl;
  
  server->Wait();
}

