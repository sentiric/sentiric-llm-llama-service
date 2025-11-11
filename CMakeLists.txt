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

# --- Alt Projeleri Ekle ---
add_subdirectory(llama.cpp)
include(FetchContent)
FetchContent_Declare(
  sentiric_contracts
  GIT_REPOSITORY https://github.com/sentiric/sentiric-contracts.git
  GIT_TAG v1.10.0
)
FetchContent_MakeAvailable(sentiric_contracts)

# --- Kod Üretimi ---
set(PROTO_FILES "${sentiric_contracts_SOURCE_DIR}/proto/sentiric/llm/v1/local.proto")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# protoc komutu için özel bir hedef oluştur
add_custom_target(generate_protobuf ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND $<TARGET_FILE:protobuf::protoc>
        --proto_path=${sentiric_contracts_SOURCE_DIR}/proto
        --cpp_out=${GENERATED_DIR}
        --grpc_out=${GENERATED_DIR}
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        ${PROTO_FILES}
    DEPENDS ${PROTO_FILES}
)

# --- Kütüphane ve Executable Hedeflerini Tanımla ---
add_library(proto_lib STATIC
    "${GENERATED_DIR}/sentiric/llm/v1/local.pb.cc"
    "${GENERATED_DIR}/sentiric/llm/v1/local.grpc.pb.cc"
)

add_executable(llm_service
    src/main.cpp
    src/llm_engine.cpp
    src/grpc_server.cpp
    src/http_server.cpp
    src/model_manager.cpp
)

add_executable(llm_cli
    src/cli/main.cpp
    src/cli/cli_client.cpp
    src/cli/grpc_client.cpp
    src/cli/http_client.cpp
    src/cli/health_check.cpp
    src/cli/benchmark.cpp
)

# --- Derleme Bağımlılıkları ve Include Yollarını Ayarla ---
# proto_lib'in derlenmesi için generate_protobuf hedefinin bitmesini bekle
add_dependencies(proto_lib generate_protobuf)

# proto_lib'in kendi header'larını bulabilmesi için include yolunu ekle
target_include_directories(proto_lib PUBLIC ${GENERATED_DIR})
target_include_directories(llm_service PRIVATE src)
target_include_directories(llm_cli PRIVATE src)

# --- Merkezi Linkleme Bloğu ---
target_link_libraries(proto_lib PUBLIC gRPC::grpc++ protobuf::libprotobuf)

target_link_libraries(llm_service PRIVATE
    proto_lib llama spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib c-ares::cares Threads::Threads
)

target_link_libraries(llm_cli PRIVATE
    proto_lib llama spdlog::spdlog nlohmann_json::nlohmann_json
    httplib::httplib c-ares::cares Threads::Threads
)

if(GGML_CUDA)
    message(STATUS "CUDA backend enabled, linking CUDA libraries to targets.")
    target_link_libraries(llm_service PRIVATE CUDA::cudart)
    target_link_libraries(llm_cli PRIVATE CUDA::cudart)
endif()