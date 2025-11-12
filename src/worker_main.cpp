#include <iostream>
#include <string>
#include "worker.h"

int main(int argc, char** argv) {
  std::string coordinator_addr = "localhost:50052";
  int worker_id = 0;
  int iterations = 1;
  std::string worker_addr = "";
  int32_t worker_port = 0;

  if (argc > 1) coordinator_addr = argv[1];
  if (argc > 2) worker_id = std::stoi(argv[2]);
  if (argc > 3) iterations = std::stoi(argv[3]);
  if (argc > 4) worker_addr = argv[4];
  if (argc > 5) worker_port = std::stoi(argv[5]);

  Worker w(worker_id, coordinator_addr, worker_addr, worker_port);
  
  if (!w.initialize()) {
    std::cerr << "worker " << worker_id << " failed to initialize" << std::endl;
    return 1;
  }
  
  for (int it = 0; it < iterations; ++it) {
    bool done = w.run_iteration(it);
    std::cout << "worker " << worker_id << " iter " << it << " done=" << (done ? "true" : "false") << std::endl;
  }
  return 0;
}


