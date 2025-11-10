cmake_minimum_required(VERSION 3.20)
project(llm_service)

# Basit includes
include_directories(
    /opt/vcpkg/installed/x64-linux/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Basit libraries
link_directories(/opt/vcpkg/installed/x64-linux/lib)

# Sadece temel servis - GRPC OLMADAN
add_executable(llm_service 
    src/main.cpp
    src/http_server.cpp
    src/llm_engine.cpp
)

# Basit linking
target_link_libraries(llm_service 
    pthread
    spdlog
    httplib
    nlohmann_json
)

set_target_properties(llm_service PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)