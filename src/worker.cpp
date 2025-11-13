#include "worker.h"

#include <grpcpp/grpcpp.h>
#include "parameter_server.grpc.pb.h"
#include "coordinator.grpc.pb.h"
#include <thread>
#include <chrono>
#include <functional>
#include <cmath>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using parameter_server::ParameterServer;
using parameter_server::GradientUpdate;
using parameter_server::PushResponse;
using parameter_server::PullRequest;
using parameter_server::ParameterUpdate;
using parameter_server::Tensor;
using parameter_server::SyncStatusRequest;
using parameter_server::SyncStatusResponse;
using coordinator::Coordinator;
using coordinator::WorkerInfo;
using coordinator::RegisterResponse;
using coordinator::ListWorkersRequest;
using coordinator::ListWorkersResponse;
using coordinator::GetPSAddressRequest;
using coordinator::GetPSAddressResponse;
using coordinator::HeartbeatRequest;
using coordinator::HeartbeatResponse;
using coordinator::WorkerStatus;

namespace {
std::vector<Tensor> to_proto(const std::vector<TensorLite>& ts) {
  std::vector<Tensor> out;
  out.reserve(ts.size());
  for (const auto& t : ts) {
    Tensor proto;
    proto.set_name(t.name);
    for (int32_t d : t.shape) proto.add_shape(d);
    for (float v : t.data) proto.add_data(v);
    proto.set_dtype(t.dtype);
    out.push_back(std::move(proto));
  }
  return out;
}

std::vector<TensorLite> from_proto(const google::protobuf::RepeatedPtrField<Tensor>& ts) {
  std::vector<TensorLite> out;
  out.reserve(ts.size());
  for (const auto& t : ts) {
    TensorLite x;
    x.name = t.name();
    x.shape.assign(t.shape().begin(), t.shape().end());
    x.data.assign(t.data().begin(), t.data().end());
    x.dtype = t.dtype();
    out.push_back(std::move(x));
  }
  return out;
}
}  // namespace

Worker::Worker(int worker_id, const std::string& coordinator_address, const std::string& worker_address, int32_t worker_port)
  : worker_id_(worker_id), coordinator_address_(coordinator_address), 
    worker_address_(worker_address), worker_port_(worker_port), initialized_(false),
    running_(true), current_status_(0) {
  heartbeat_thread_ = std::thread(&Worker::heartbeat_loop, this);
}

Worker::~Worker() {
  running_ = false;
  if (heartbeat_thread_.joinable()) {
    heartbeat_thread_.join();
  }
}

bool Worker::initialize() {
  if (initialized_) return true;
  
  if (!discover_parameter_server()) {
    return false;
  }
  
  if (!register_with_coordinator()) {
    return false;
  }
  
  initialized_ = true;
  current_status_ = 0;
  return true;
}

bool Worker::reconnect() {
  initialized_ = false;
  return initialize();
}

bool Worker::query_with_retry(const std::function<bool()>& query_func, int max_retries) {
  for (int attempt = 0; attempt < max_retries; ++attempt) {
    if (query_func()) {
      return true;
    }
    
    int backoff_ms = static_cast<int>(std::pow(2, attempt) * 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
  }
  return false;
}

bool Worker::discover_parameter_server() {
  return query_with_retry([this]() {
    auto channel = grpc::CreateChannel(coordinator_address_, grpc::InsecureChannelCredentials());
    std::unique_ptr<Coordinator::Stub> stub = Coordinator::NewStub(channel);
    
    ClientContext ctx;
    GetPSAddressRequest req;
    GetPSAddressResponse resp;
    Status s = stub->GetParameterServerAddress(&ctx, req, &resp);
    
    if (s.ok()) {
      ps_address_ = resp.address() + ":" + std::to_string(resp.port());
      return true;
    }
    return false;
  });
}

bool Worker::register_with_coordinator() {
  return query_with_retry([this]() {
    auto channel = grpc::CreateChannel(coordinator_address_, grpc::InsecureChannelCredentials());
    std::unique_ptr<Coordinator::Stub> stub = Coordinator::NewStub(channel);
    
    ClientContext ctx;
    WorkerInfo req;
    req.set_worker_id(worker_id_);
    if (!worker_address_.empty()) {
      req.set_address(worker_address_);
    } else {
      req.set_address("localhost");
    }
    req.set_port(worker_port_);
    req.set_hostname("worker-" + std::to_string(worker_id_));
    
    RegisterResponse resp;
    Status s = stub->RegisterWorker(&ctx, req, &resp);
    
    if (s.ok() && resp.success()) {
      if (!ps_address_.empty() && ps_address_ != resp.parameter_server_address()) {
        ps_address_ = resp.parameter_server_address();
      }
      return true;
    }
    return false;
  });
}

std::vector<std::string> Worker::discover_peer_workers() {
  std::vector<std::string> peers;
  
  query_with_retry([this, &peers]() {
    auto channel = grpc::CreateChannel(coordinator_address_, grpc::InsecureChannelCredentials());
    std::unique_ptr<Coordinator::Stub> stub = Coordinator::NewStub(channel);
    
    ClientContext ctx;
    ListWorkersRequest req;
    ListWorkersResponse resp;
    Status s = stub->ListWorkers(&ctx, req, &resp);
    
    if (s.ok()) {
      peers.clear();
      for (const auto& worker : resp.workers()) {
        if (worker.worker_id() != worker_id_) {
          std::string addr = worker.address() + ":" + std::to_string(worker.port());
          peers.push_back(addr);
        }
      }
      return true;
    }
    return false;
  });
  
  return peers;
}

void Worker::send_heartbeat() {
  if (!initialized_) return;
  
  auto channel = grpc::CreateChannel(coordinator_address_, grpc::InsecureChannelCredentials());
  std::unique_ptr<Coordinator::Stub> stub = Coordinator::NewStub(channel);
  
  ClientContext ctx;
  HeartbeatRequest req;
  req.set_worker_id(worker_id_);
  req.set_status(static_cast<WorkerStatus>(current_status_.load()));
  
  HeartbeatResponse resp;
  stub->Heartbeat(&ctx, req, &resp);
}

void Worker::heartbeat_loop() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    if (initialized_) {
      send_heartbeat();
    }
  }
}

std::vector<TensorLite> Worker::pull_parameters(int iteration) {
  auto channel = grpc::CreateChannel(ps_address_, grpc::InsecureChannelCredentials());
  std::unique_ptr<ParameterServer::Stub> stub = ParameterServer::NewStub(channel);

  ClientContext ctx;
  PullRequest req;
  req.set_worker_id(worker_id_);
  req.set_iteration(iteration);
  ParameterUpdate resp;
  Status s = stub->ServeParameters(&ctx, req, &resp);
  if (!s.ok()) return {};
  return from_proto(resp.parameters());
}

bool Worker::push_gradients(int iteration, const std::vector<TensorLite>& grads, int& workers_received, int& total_workers) {
  auto channel = grpc::CreateChannel(ps_address_, grpc::InsecureChannelCredentials());
  std::unique_ptr<ParameterServer::Stub> stub = ParameterServer::NewStub(channel);

  ClientContext ctx;
  GradientUpdate req;
  req.set_worker_id(worker_id_);
  req.set_iteration(iteration);
  auto proto_tensors = to_proto(grads);
  for (const auto& t : proto_tensors) {
    *req.add_gradients() = t;
  }
  PushResponse resp;
  Status s = stub->ReceiveGradients(&ctx, req, &resp);
  if (!s.ok()) return false;
  workers_received = resp.workers_received();
  total_workers = resp.total_workers();
  return resp.aggregation_complete();
}

bool Worker::check_sync_ready(int iteration, int& workers_received, int& total_workers) {
  auto channel = grpc::CreateChannel(ps_address_, grpc::InsecureChannelCredentials());
  std::unique_ptr<ParameterServer::Stub> stub = ParameterServer::NewStub(channel);

  ClientContext ctx;
  SyncStatusRequest req;
  req.set_iteration(iteration);
  SyncStatusResponse resp;
  Status s = stub->CheckSyncStatus(&ctx, req, &resp);
  if (!s.ok()) return false;
  workers_received = resp.workers_received();
  total_workers = resp.total_workers();
  return resp.ready();
}

std::vector<TensorLite> Worker::compute_gradients(const std::vector<TensorLite>& params) {
  // simple dummy gradient: same shape as params, filled with 0.01
  std::vector<TensorLite> grads = params;
  for (auto& t : grads) {
    for (auto& v : t.data) v = 0.01f;
  }
  return grads;
}

bool Worker::run_iteration(int iteration) {
  current_status_ = 1;
  
  int retry_count = 0;
  const int max_retries = 3;
  
  while (retry_count < max_retries) {
    auto params = pull_parameters(iteration);
    
    if (params.empty() && retry_count < max_retries - 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      retry_count++;
      continue;
    }
    
    if (params.empty()) {
      TensorLite dummy;
      dummy.name = "weight";
      dummy.shape = {10, 10};
      dummy.dtype = 0;
      dummy.data.resize(100, 0.0f);
      params.push_back(dummy);
    }
    
    auto grads = compute_gradients(params);
    int workers_received = 0, total_workers = 0;
    bool aggregation_complete = push_gradients(iteration, grads, workers_received, total_workers);
    
    if (!aggregation_complete && retry_count < max_retries - 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      retry_count++;
      continue;
    }
    
    if (aggregation_complete) {
      current_status_ = 0;
      return true;
    }
    
    bool ready = false;
    int poll_count = 0;
    const int max_polls = 200;
    
    while (!ready && poll_count < max_polls) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      ready = check_sync_ready(iteration, workers_received, total_workers);
      
      if (ready) {
        current_status_ = 0;
        return true;
      }
      
      poll_count++;
      
      if (poll_count % 20 == 0) {
        int w, t;
        check_sync_ready(iteration, w, t);
      }
    }
    
    if (ready) {
      current_status_ = 0;
      return true;
    }
    
    if (retry_count < max_retries - 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      retry_count++;
    } else {
      break;
    }
  }
  
  current_status_ = 0;
  return false;
}

 
