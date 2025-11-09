cmake_minimum_required(VERSION 3.20)
project(llm_service)

# vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

# Find system llama.cpp library
find_package(llama REQUIRED)

# Find dependencies via vcpkg
find_package(httplib REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# Generate gRPC code
set(PROTO_FILES
    proto/llm_service.proto
)

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(ABS_PROTO_FILE ${PROTO_FILE} ABSOLUTE)
    get_filename_component(PROTO_DIR ${ABS_PROTO_FILE} DIRECTORY)
    get_filename_component(PROTO_NAME ${ABS_PROTO_FILE} NAME_WE)
    
    set(GENERATED_PROTO_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
    
    # Generate gRPC code
    protobuf_generate(
        LANGUAGE cpp
        PROTOC_OUT_DIR ${GENERATED_PROTO_DIR}
        TARGET llm_service_proto
        ${ABS_PROTO_FILE}
    )
    
    # Generate gRPC service code  
    protobuf_generate_grpc_cpp(
        TARGET llm_service_grpc
        PROTOC_OUT_DIR ${GENERATED_PROTO_DIR}
        ${ABS_PROTO_FILE}
    )
    
    # Include generated directory
    include_directories(${GENERATED_PROTO_DIR})
endforeach()

# Create executables
add_executable(llm_service 
    src/main.cpp
    src/grpc_server.cpp
    src/model_handler.cpp
    ${GENERATED_PROTO_DIR}/llm_service.pb.cc
    ${GENERATED_PROTO_DIR}/llm_service.grpc.pb.cc
)

add_executable(grpc_test_client 
    src/grpc_client.cpp
    ${GENERATED_PROTO_DIR}/llm_service.pb.cc
    ${GENERATED_PROTO_DIR}/llm_service.grpc.pb.cc
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
    ${GENERATED_PROTO_DIR}
)

target_include_directories(grpc_test_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GENERATED_PROTO_DIR}
)

# C++ standard
set_target_properties(llm_service grpc_test_client PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)