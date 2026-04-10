cmake_minimum_required(VERSION 3.8)

set(RPC_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/../rpc_generated")

set(RPC_SERVER_STUB "${RPC_GEN_DIR}/StopService_s.c")
set_source_files_properties("${RPC_SERVER_STUB}" PROPERTIES GENERATED TRUE)

add_executable(TrayService
    "main.cpp"
    "ServiceMain.cpp"
    "ServiceMain.h"
    "SessionLauncher.cpp"
    "SessionLauncher.h"
    "RpcServer.cpp"
    "RpcServer.h"
    "${RPC_SERVER_STUB}"
)

add_dependencies(TrayService rpc_generated)

target_include_directories(TrayService PRIVATE "${RPC_GEN_DIR}")
target_compile_definitions(TrayService PRIVATE UNICODE _UNICODE)
target_link_libraries(TrayService PRIVATE rpcrt4 wtsapi32 advapi32 userenv)

set_property(TARGET TrayService PROPERTY CXX_STANDARD 20)

install(TARGETS TrayService RUNTIME DESTINATION bin)
