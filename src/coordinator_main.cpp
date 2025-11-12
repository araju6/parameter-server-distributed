#include <iostream>
#include <string>
#include "coordinator_service.h"

int main(int argc, char** argv) {
  std::string server_address = "0.0.0.0:50052";
  std::string ps_address = "localhost:50051";
  int32_t ps_port = 50051;
  
  if (argc > 1) {
    server_address = argv[1];
  }
  if (argc > 2) {
    ps_address = argv[2];
    size_t colon_pos = ps_address.find(':');
    if (colon_pos != std::string::npos) {
      ps_port = std::stoi(ps_address.substr(colon_pos + 1));
      ps_address = ps_address.substr(0, colon_pos);
    }
  }
  
  run_coordinator_server(server_address, ps_address, ps_port);
  return 0;
}

