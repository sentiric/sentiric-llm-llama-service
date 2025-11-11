# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

if(DEFINED GGML_CUDA AND GGML_CUDA)
    project(llm-llama-service CXX CUDA)
else()
    project(llm-llama-service CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# --- Bağımlılıkları Bul ---
# DÜZELTME: Protobuf'un CMake modüllerini de iste
find_package(Protobuf REQUIRED COMPONENTS protobuf protoc)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(c-ares CONFIG REQUIRED)
find_package(Threads REQUIRED)

if(GGML_CUDA)
    find_package(CUDAToolkit REQUIRED)
endif()

# --- Alt Projeleri Ekle ---
add_subdirectory(llama.cpp)
include(FetchContent)
FetchContent_Declare(
  sentiric_contracts
  GIT_REPOSITORY https://github.com/sentiric/sentiric-contracts.git
  GIT_TAG v1.10.0
)
FetchContent_MakeAvailable(sentiric_contracts)

# --- Protobuf Kod Üretimi (Standart Yöntem) ---
set(PROTO_FILES "${sentiric_contracts_SOURCE_DIR}/proto/sentiric/llm/v1/local.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# DÜZELTME: add_custom_target yerine Protobuf_generate kullan
Protobuf_generate(
    TARGETS proto_lib
    LANGUAGE grpc
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS "${sentiric_contracts_SOURCE_DIR}/proto"
    GENERATE_EXTENSIONS .pb.cc .pb.h .grpc.pb.cc .grpc.pb.h
)

# --- Kütüphane ve Executable Hedeflerini Tanımla ---
# Not: proto_lib hedefi Protobuf_generate tarafından zaten oluşturuldu.
# Sadece include dizinini belirtmemiz yeterli.
target_include_directories(proto_lib PUBLIC ${GENERATED_DIR})


add_executable(llm_service
    src/main.cpp
    src/llm_engine.cpp
    src/grpc_server.cpp
    src/http_server.cpp
    src/model_manager.cpp
)
target_include_directories(llm_service PRIVATE src)

add_executable(llm_cli
    src/cli/main.cpp
    src/cli/cli_client.cpp
    src/cli/grpc_client.cpp
    src/cli/http_client.cpp
    src/cli/health_check.cpp
    src/cli/benchmark.cpp
)
target_include_directories(llm_cli PRIVATE src)

# --- Merkezi Linkleme Bloğu ---
target_link_libraries(llm_service PRIVATE
    proto_lib llama spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib gRPC::grpc++ c-ares::cares Threads::Threads
)

target_link_libraries(llm_cli PRIVATE
    proto_lib llama spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib gRPC::grpc++ c-ares::cares Threads::Threads
)

if(GGML_CUDA)
    message(STATUS "CUDA backend enabled, linking CUDA libraries to targets.")
    target_link_libraries(llm_service PRIVATE CUDA::cudart)
    target_link_libraries(llm_cli PRIVATE CUDA::cudart)
endif()