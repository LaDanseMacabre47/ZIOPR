set(RPC_CLIENT_STUB "${RPC_GEN_DIR}/StopService_c.c")
if(NOT MSVC)
    set_source_files_properties("${RPC_CLIENT_STUB}" PROPERTIES GENERATED FALSE)
else()
    set_source_files_properties("${RPC_CLIENT_STUB}" PROPERTIES GENERATED TRUE)
endif()

add_executable(TrayApp WIN32
    "main.cpp"
    "TrayApp.cpp"
    "TrayApp.h"
    "RpcClient.cpp"
    "RpcClient.h"
    "resource.h"
    "${RPC_CLIENT_STUB}"
)

add_dependencies(TrayApp rpc_generated)

target_include_directories(TrayApp PRIVATE "${RPC_GEN_DIR}")
target_compile_definitions(TrayApp PRIVATE UNICODE _UNICODE)
target_link_libraries(TrayApp PRIVATE rpcrt4 wtsapi32 advapi32)

set_property(TARGET TrayApp PROPERTY CXX_STANDARD 20)

install(TARGETS TrayApp RUNTIME DESTINATION bin)
