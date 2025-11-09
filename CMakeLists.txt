cmake_minimum_required(VERSION 3.20)
project(llm_service)

# vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Find system llama.cpp library
find_package(llama REQUIRED)

# Find dependencies via vcpkg
find_package(httplib REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# Generate gRPC code - FIXED VERSION
set(PROTO_FILE "proto/llm_service.proto")

# Protobuf generation
protobuf_generate(
    TARGET llm_service_proto
    PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto
    ${PROTO_FILE}
)

# gRPC generation  
protobuf_generate(
    TARGET llm_service_grpc
    PROTOC_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto
    GRPC
    PLUGIN "protoc-gen-grpc=${gRPC_CPP_PLUGIN_PROGRAM}"
    ${PROTO_FILE}
)

# Include generated directory
include_directories(${CMAKE_CURRENT_BINARY_DIR}/proto)

# Create executables
add_executable(llm_service 
    src/main.cpp
    src/grpc_server.cpp
    src/model_handler.cpp
)

add_executable(grpc_test_client 
    src/grpc_client.cpp
)

# Link generated protobuf/grpc code
target_sources(llm_service PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/proto/llm_service.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/proto/llm_service.grpc.pb.cc
)

target_sources(grpc_test_client PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/proto/llm_service.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/proto/llm_service.grpc.pb.cc
)

# Link libraries for main service
target_link_libraries(llm_service PRIVATE 
    llama
    httplib::httplib
    gRPC::grpc++
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    protobuf::libprotobuf
)

# Link libraries for test client
target_link_libraries(grpc_test_client PRIVATE 
    gRPC::grpc++
    spdlog::spdlog
    protobuf::libprotobuf
)

# Include directories
target_include_directories(llm_service PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

target_include_directories(grpc_test_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

# C++ standard
set_target_properties(llm_service grpc_test_client PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)