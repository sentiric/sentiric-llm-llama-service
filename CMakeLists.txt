cmake_minimum_required(VERSION 3.20)

if(DEFINED GGML_CUDA AND GGML_CUDA)
    project(llm-llama-service CXX CUDA)
else()
    project(llm-llama-service CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(GGML_CUDA)
    find_package(CUDAToolkit REQUIRED)
endif()

set(LLAMA_BUILD_COMMON ON CACHE BOOL "Force building of common library" FORCE)
add_subdirectory(llama.cpp)

set(LLAMA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/include)
set(LLAMA_COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp/common)

include(FetchContent)
FetchContent_Declare(
  sentiric_contracts
  GIT_REPOSITORY https://github.com/sentiric/sentiric-contracts.git
  GIT_TAG v1.10.0
)
FetchContent_MakeAvailable(sentiric_contracts)

set(PROTO_FILES "${sentiric_contracts_SOURCE_DIR}/proto/sentiric/llm/v1/local.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_GENERATED_SRCS
    "${GENERATED_DIR}/sentiric/llm/v1/local.pb.cc"
    "${GENERATED_DIR}/sentiric/llm/v1/local.grpc.pb.cc"
)
set(PROTO_GENERATED_HDRS
    "${GENERATED_DIR}/sentiric/llm/v1/local.pb.h"
    "${GENERATED_DIR}/sentiric/llm/v1/local.grpc.pb.h"
)

add_custom_command(
    OUTPUT ${PROTO_GENERATED_SRCS} ${PROTO_GENERATED_HDRS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND $<TARGET_FILE:protobuf::protoc>
        --proto_path=${sentiric_contracts_SOURCE_DIR}/proto
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
)

add_library(proto_lib STATIC
    ${PROTO_GENERATED_SRCS}
    ${PROTO_GENERATED_HDRS}
)
set_source_files_properties(${PROTO_GENERATED_SRCS} ${PROTO_GENERATED_HDRS} PROPERTIES GENERATED TRUE)
target_include_directories(proto_lib PUBLIC ${GENERATED_DIR})
target_link_libraries(proto_lib PUBLIC gRPC::grpc++ protobuf::libprotobuf)

add_executable(llm_service
    src/main.cpp
    src/llm_engine.cpp
    src/grpc_server.cpp
    src/http_server.cpp
    src/model_manager.cpp
    src/prompt_formatter.cpp
)
add_dependencies(llm_service proto_lib)

target_include_directories(llm_service PRIVATE 
    src
    ${LLAMA_INCLUDE_DIR}
    ${LLAMA_COMMON_INCLUDE_DIR}
)
target_link_libraries(llm_service PRIVATE
    proto_lib 
    common
    llama
    spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib c-ares::cares Threads::Threads
)
if(GGML_CUDA)
    target_link_libraries(llm_service PRIVATE CUDA::cudart CUDA::cuda_driver)
endif()

add_executable(llm_cli
    src/cli/main.cpp
    src/cli/cli_client.cpp
    src/cli/grpc_client.cpp
    src/cli/http_client.cpp
    src/cli/health_check.cpp
    src/cli/benchmark.cpp
)
add_dependencies(llm_cli proto_lib)

target_include_directories(llm_cli PRIVATE 
    src
    ${LLAMA_INCLUDE_DIR}
    ${LLAMA_COMMON_INCLUDE_DIR}
)
target_link_libraries(llm_cli PRIVATE
    proto_lib 
    common
    llama
    spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib c-ares::cares Threads::Threads
)
if(GGML_CUDA)
    target_link_libraries(llm_cli PRIVATE CUDA::cudart CUDA::cuda_driver)
endif()

