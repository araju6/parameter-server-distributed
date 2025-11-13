#pragma once

#include <string>

void run_server(const std::string& server_address, int total_workers, int checkpoint_interval = 10);

