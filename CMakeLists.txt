cmake_minimum_required(VERSION 3.20)
project(llm-llama-service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# --- vcpkg Bağımlılıkları ---
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(Threads REQUIRED)

# --- llama.cpp'yi Alt Dizin Olarak Ekle ---
set(LLAMA_STATIC ON)
set(LLAMA_BUILD_TESTS OFF)
set(LLAMA_BUILD_EXAMPLES OFF)
add_subdirectory(llama.cpp)

# --- Protobuf Kod Üretimi ---
include(FetchContent)
FetchContent_Declare(
  sentiric_contracts
  GIT_REPOSITORY https://github.com/sentiric/sentiric-contracts.git
  GIT_TAG v1.10.0
)
FetchContent_MakeAvailable(sentiric_contracts)

set(PROTO_FILES "${sentiric_contracts_SOURCE_DIR}/proto/sentiric/llm/v1/local.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
    OUTPUT
        "${GENERATED_DIR}/sentiric/llm/v1/local.pb.cc"
        "${GENERATED_DIR}/sentiric/llm/v1/local.grpc.pb.cc"
        "${GENERATED_DIR}/sentiric/llm/v1/local.pb.h"
        "${GENERATED_DIR}/sentiric/llm/v1/local.grpc.pb.h"
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
    "${GENERATED_DIR}/sentiric/llm/v1/local.pb.cc"
    "${GENERATED_DIR}/sentiric/llm/v1/local.grpc.pb.cc"
)
target_include_directories(proto_lib PUBLIC ${GENERATED_DIR})
target_link_libraries(proto_lib PUBLIC gRPC::grpc++ protobuf::libprotobuf)

# --- Ana Servis Hedefi ---
add_executable(llm_service
    src/main.cpp
    src/llm_engine.cpp
    src/grpc_server.cpp
    src/http_server.cpp
)
target_include_directories(llm_service PRIVATE src)
target_link_libraries(llm_service PRIVATE
    proto_lib llama spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib gRPC::grpc++ protobuf::libprotobuf c-ares::cares Threads::Threads
)

# --- CLI Aracı Hedefi ---
add_executable(llm_cli
    src/cli/main.cpp
    # KRİTİK DÜZELTME: Sadece .cpp dosyaları listelenir.
    src/cli/cli_client.cpp
    src/cli/grpc_client.cpp
    src/cli/http_client.cpp
    src/cli/health_check.cpp
    src/cli/benchmark.cpp
)
target_include_directories(llm_cli PRIVATE src)
target_link_libraries(llm_cli PRIVATE
    proto_lib spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib
    gRPC::grpc++ protobuf::libprotobuf c-ares::cares Threads::Threads
)