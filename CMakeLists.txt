cmake_minimum_required(VERSION 3.10)
project(ParameterServerDistributed)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile_commands.json for better IntelliSense support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_BINARY_DIR ${CMAKE_BINARY_DIR})

# Find packages
# Find gRPC first, which will handle Protobuf dependencies
find_package(gRPC REQUIRED)
find_package(Threads REQUIRED)

# Ensure Protobuf is available (gRPC should have found it, but we need the executable)
if(NOT Protobuf_PROTOC_EXECUTABLE)
  find_package(Protobuf REQUIRED)
endif()

if(Protobuf_PROTOC_EXECUTABLE)
  get_filename_component(PROTOBUF_BIN_DIR "${Protobuf_PROTOC_EXECUTABLE}" DIRECTORY)
endif()

if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
  find_program(gRPC_CPP_PLUGIN_EXECUTABLE
    NAMES grpc_cpp_plugin
    HINTS
      ${gRPC_BIN_DIR}
      ${PROTOBUF_BIN_DIR}
      /opt/homebrew/opt/grpc/bin
      /opt/homebrew/bin
      /usr/local/bin
      /usr/bin
  )
  if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
    message(FATAL_ERROR "grpc_cpp_plugin not found. Install gRPC (e.g. 'brew install grpc') or add the plugin to PATH.")
  endif()
endif()

# Include generated proto files
include_directories(${CMAKE_BINARY_DIR})

# Generate proto files
set(PROTO_FILES
  proto/parameter_server.proto
)

foreach(PROTO_FILE ${PROTO_FILES})
  get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
  
  set(PROTO_SRCS "${CMAKE_BINARY_DIR}/${PROTO_NAME}.pb.cc")
  set(PROTO_HDRS "${CMAKE_BINARY_DIR}/${PROTO_NAME}.pb.h")
  set(GRPC_SRCS "${CMAKE_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
  set(GRPC_HDRS "${CMAKE_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")
  
  add_custom_command(
    OUTPUT "${PROTO_SRCS}" "${PROTO_HDRS}" "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --cpp_out=${CMAKE_BINARY_DIR}
         --grpc_out=${CMAKE_BINARY_DIR}
         --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
         -I${CMAKE_SOURCE_DIR}/proto
         ${CMAKE_SOURCE_DIR}/${PROTO_FILE}
    DEPENDS ${CMAKE_SOURCE_DIR}/${PROTO_FILE}
    VERBATIM
  )
  
  list(APPEND PROTO_GENERATED_SRCS ${PROTO_SRCS} ${GRPC_SRCS})
endforeach()

# Parameter server executable
add_executable(parameter_server
  src/parameter_server.cpp
  src/parameter_server_service.cpp
  src/main.cpp
  ${PROTO_GENERATED_SRCS}
)

target_link_libraries(parameter_server
  protobuf::libprotobuf
  gRPC::grpc++
  Threads::Threads
)
