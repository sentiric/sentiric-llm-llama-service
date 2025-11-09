cmake_minimum_required(VERSION 3.20)
project(llm-llama-service CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg aracılığıyla temel kütüphaneleri bul
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(Threads REQUIRED)

# --- SENTIRIC-CONTRACTS ENTEGRASYONU ---
include(FetchContent)
FetchContent_Declare(
  sentiric_contracts
  GIT_REPOSITORY https://github.com/sentiric/sentiric-contracts.git
  GIT_TAG v1.10.0
)
FetchContent_MakeAvailable(sentiric_contracts)

# 'sentiric-contracts'tan gelen C++ kaynak dosyalarını bir kütüphane olarak tanımla
file(GLOB_RECURSE CONTRACT_SOURCES
    "${sentiric_contracts_SOURCE_DIR}/gen/cpp/*.cc"
)
add_library(sentiric_contracts_lib STATIC ${CONTRACT_SOURCES})
target_include_directories(sentiric_contracts_lib PUBLIC
    "${sentiric_contracts_SOURCE_DIR}/gen/cpp"
)

# --- llama.cpp'yi submodule olarak dahil et ---
set(LLAMA_STATIC ON)
set(LLAMA_BUILD_TESTS OFF)
set(LLAMA_BUILD_EXAMPLES OFF)
add_subdirectory(vendor/llama.cpp)

# --- Ana Sunucu Uygulaması ---
add_executable(llm_service
    src/main.cpp
    src/llm_engine.cpp
    src/grpc_server.cpp
    src/http_server.cpp
)

target_link_libraries(llm_service PRIVATE
    sentiric_contracts_lib
    llama
    ggml_static
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    httplib::httplib
    gRPC::grpc++
    protobuf::libprotobuf
    Threads::Threads
)

target_include_directories(llm_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    vendor/llama.cpp
    vendor/llama.cpp/common
)

# --- YENİ: C++ Test İstemcisi ---
add_executable(grpc_test_client
    src/grpc_test_client.cpp
)

target_link_libraries(grpc_test_client PRIVATE
    sentiric_contracts_lib
    gRPC::grpc++
    protobuf::libprotobuf
    spdlog::spdlog
)

target_include_directories(grpc_test_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)