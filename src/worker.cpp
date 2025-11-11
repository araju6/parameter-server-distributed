#include "worker.h"

#include <grpcpp/grpcpp.h>
#include "parameter_server.grpc.pb.h"

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

Worker::Worker(int worker_id, const std::string& ps_address): worker_id_(worker_id), ps_address_(ps_address) {}

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
  auto params = pull_parameters(iteration);
  if (params.empty()) return false;
  auto grads = compute_gradients(params);
  int workers_received = 0, total_workers = 0;
  bool complete = push_gradients(iteration, grads, workers_received, total_workers);
  if (!complete) {
    // optionally poll until ready; leaving it to caller to loop
    return false;
  }
  return true;
}

 
