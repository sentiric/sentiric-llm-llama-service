cmake_minimum_required(VERSION 3.20)
project(llm-llama-service CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg aracılığıyla TÜM C++ bağımlılıklarını bul
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Protobuf CONFIG REQUIRED)

# --- SENTIRIC-CONTRACTS'TAN PROTO DOSYALARINI AL ---
include(FetchContent)
FetchContent_Declare(
  sentiric_contracts
  GIT_REPOSITORY https://github.com/sentiric/sentiric-contracts.git
  GIT_TAG v1.10.0
)
FetchContent_MakeAvailable(sentiric_contracts)

# --- KOD ÜRETİMİNİ BU PROJE İÇİNDE YAP ---
add_library(proto_lib INTERFACE)

set(PROTO_FILES "${sentiric_contracts_SOURCE_DIR}/proto/sentiric/llm/v1/local.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

protobuf_generate(
    TARGET proto_lib
    LANGUAGE cpp
    GENERATE_EXTENSIONS .pb.cc .pb.h
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS "${sentiric_contracts_SOURCE_DIR}/proto"
    GENERATED_DIR ${GENERATED_DIR}
)
protobuf_generate(
    TARGET proto_lib
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.cc .grpc.pb.h
    PROTOS ${PROTO_FILES}
    IMPORT_DIRS "${sentiric_contracts_SOURCE_DIR}/proto"
    PLUGIN "protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
    GENERATED_DIR ${GENERATED_DIR}
)

target_include_directories(proto_lib INTERFACE "${GENERATED_DIR}")

# --- llama.cpp'yi submodule olarak dahil et ---
set(LLAMA_STATIC ON)
add_subdirectory(vendor/llama.cpp)

# --- Ana Sunucu Uygulaması ---
add_executable(llm_service
    src/main.cpp
    src/llm_engine.cpp
    src/grpc_server.cpp
    src/http_server.cpp
)

target_link_libraries(llm_service PRIVATE
    proto_lib
    llama ggml_static
    spdlog::spdlog nlohmann_json::nlohmann_json httplib::httplib
    Threads::Threads
)

target_include_directories(llm_service PRIVATE
    src
    vendor/llama.cpp
    ${GENERATED_DIR} # <-- DÜZELTME 1: Üretilen dosyaların yolunu buraya ekle
)

# --- C++ Test İstemcisi ---
add_executable(grpc_test_client src/grpc_test_client.cpp)
target_link_libraries(grpc_test_client PRIVATE proto_lib spdlog::spdlog)
target_include_directories(grpc_test_client PRIVATE
    src
    ${GENERATED_DIR} # <-- DÜZELTME 2: Üretilen dosyaların yolunu buraya ekle
)