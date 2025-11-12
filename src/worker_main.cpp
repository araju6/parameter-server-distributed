#include <iostream>
#include <string>
#include "worker.h"

int main(int argc, char** argv) {
  std::string ps_addr = "localhost:50051";
  int worker_id = 0;
  int iterations = 1;

  if (argc > 1) ps_addr = argv[1];
  if (argc > 2) worker_id = std::stoi(argv[2]);
  if (argc > 3) iterations = std::stoi(argv[3]);

  Worker w(worker_id, ps_addr);
  for (int it = 0; it < iterations; ++it) {
    bool done = w.run_iteration(it);
    std::cout << "worker " << worker_id << " iter " << it << " done=" << (done ? "true" : "false") << std::endl;
  }
  return 0;
}


