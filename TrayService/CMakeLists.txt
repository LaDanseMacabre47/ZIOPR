set(RPC_SERVER_STUB "${RPC_GEN_DIR}/StopService_s.c")
if(NOT MSVC)
    set_source_files_properties("${RPC_SERVER_STUB}" PROPERTIES GENERATED FALSE)
    set_source_files_properties("${RPC_SERVER_STUB}" PROPERTIES COMPILE_DEFINITIONS "_M_AMD64")
else()
    set_source_files_properties("${RPC_SERVER_STUB}" PROPERTIES GENERATED TRUE)
endif()

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
target_compile_definitions(TrayService PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(TrayService PRIVATE rpcrt4 wtsapi32 advapi32 userenv)

if(NOT MSVC)
    target_link_options(TrayService PRIVATE -municode)
endif()

set_property(TARGET TrayService PROPERTY CXX_STANDARD 20)

install(TARGETS TrayService RUNTIME DESTINATION bin)
