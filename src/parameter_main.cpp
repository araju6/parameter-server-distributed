#include <iostream>
#include <string>
#include "parameter_server_service.h"

int main(int argc, char** argv) {
  std::string server_address = "0.0.0.0:50051";
  int total_workers = 2;
  
  if (argc > 1) {
    server_address = argv[1];
  }
  if (argc > 2) {
    total_workers = std::stoi(argv[2]);
  }
  
  run_server(server_address, total_workers);
  return 0;
}

