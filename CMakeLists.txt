cmake_minimum_required(VERSION 3.20)
project(llm-llama-service CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Bağımlılıkları bul
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(Threads REQUIRED)

# --- Manuel proto dosyalarını kullan ---
add_library(proto_lib STATIC
    src/local.pb.cc
    src/local.grpc.pb.cc
)

# vcpkg tarafından kurulan Protobuf ve gRPC başlık yollarını, 
# sistem başlıklarından (eğer varsa) daha öncelikli hale getiriyoruz.
# CMake'de find_package vcpkg'yi kullanıyorsa bu gereksiz olabilir, 
# ancak manuel olarak eklemek çakışmaları çözer.
target_include_directories(proto_lib PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    $<IF:$<TARGET_EXISTS:protobuf::libprotobuf>,$<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>,>
    $<IF:$<TARGET_EXISTS:gRPC::grpc++>,$<TARGET_PROPERTY:gRPC::grpc++,INTERFACE_INCLUDE_DIRECTORIES>,>
)

target_link_libraries(proto_lib PUBLIC 
    gRPC::grpc++ 
    protobuf::libprotobuf
)

# --- llama.cpp'yi dahil et ---
set(LLAMA_STATIC ON)
set(LLAMA_BUILD_TESTS OFF)
set(LLAMA_BUILD_EXAMPLES OFF)
add_subdirectory(vendor/llama.cpp)

# --- Ana uygulamayı tanımla ---
add_executable(llm_service
    src/main.cpp
    src/config.h
    src/llm_engine.cpp 
    src/llm_engine.h
    src/grpc_server.cpp 
    src/grpc_server.h
    src/http_server.cpp 
    src/http_server.h
)

# --- Kütüphaneleri bağla ---
target_link_libraries(llm_service PRIVATE
    proto_lib
    llama
    ggml_static
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    httplib::httplib
    Threads::Threads
)

# --- Include yolları ---
target_include_directories(llm_service PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    vendor/llama.cpp
    vendor/llama.cpp/common
)