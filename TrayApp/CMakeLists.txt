cmake_minimum_required(VERSION 3.8)

set(RPC_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../rpc_interface/generated")

add_executable(TrayApp WIN32
    "main.cpp"
    "TrayApp.cpp"   "TrayApp.h"
    "RpcClient.cpp" "RpcClient.h"
    "resource.h"
    "TrayApp.rc"
    "${RPC_GEN_DIR}/TrayService_c.c"
    "${RPC_GEN_DIR}/StopService_c.c"
)

target_include_directories(TrayApp PRIVATE "${RPC_GEN_DIR}")
target_compile_definitions(TrayApp PRIVATE UNICODE _UNICODE _M_AMD64 _AMD64_)
target_link_libraries(TrayApp PRIVATE
    rpcrt4 wtsapi32 advapi32 comctl32)

set_property(TARGET TrayApp PROPERTY CXX_STANDARD 20)
install(TARGETS TrayApp RUNTIME DESTINATION bin)