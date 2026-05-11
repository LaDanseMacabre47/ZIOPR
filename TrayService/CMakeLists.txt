cmake_minimum_required(VERSION 3.8)

set(RPC_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../rpc_interface/generated")

add_executable(TrayService
    "main.cpp"
    "ServiceMain.cpp"     "ServiceMain.h"
    "SessionLauncher.cpp" "SessionLauncher.h"
    "RpcServer.cpp"       "RpcServer.h"
    "AuthManager.cpp"     "AuthManager.h"
    "LicenseManager.cpp"  "LicenseManager.h"
    "HttpClient.cpp"      "HttpClient.h"
    "ApiConfig.h"
    "DeviceId.h"
    "${RPC_GEN_DIR}/TrayService_s.c"
    "${RPC_GEN_DIR}/StopService_s.c"
)

target_include_directories(TrayService PRIVATE "${RPC_GEN_DIR}")
target_compile_definitions(TrayService PRIVATE UNICODE _UNICODE _M_AMD64 _AMD64_ NOMINMAX)
target_link_libraries(TrayService PRIVATE
    rpcrt4 wtsapi32 advapi32 userenv winhttp iphlpapi)

# Консольное приложение (не WIN32) — нужен main(), не WinMain()
set_target_properties(TrayService PROPERTIES
    WIN32_EXECUTABLE FALSE
    CXX_STANDARD 20)

install(TARGETS TrayService RUNTIME DESTINATION bin)