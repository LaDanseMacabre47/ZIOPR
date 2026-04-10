cmake_minimum_required(VERSION 3.8)
cmake_minimum_required(VERSION 3.8)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("TrayProject" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---- RPC stubs ----
# On Windows with MSVC: generate from IDL using MIDL compiler
# On other platforms (cross-compile): use pre-generated stubs
if(MSVC)
    set(RPC_IDL "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/StopService.idl")
    set(RPC_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/rpc_generated")
    file(MAKE_DIRECTORY "${RPC_GEN_DIR}")

    set(RPC_CLIENT_STUB "${RPC_GEN_DIR}/StopService_c.c")
    set(RPC_SERVER_STUB "${RPC_GEN_DIR}/StopService_s.c")
    set(RPC_HEADER      "${RPC_GEN_DIR}/StopService.h")

    add_custom_command(
        OUTPUT "${RPC_HEADER}" "${RPC_CLIENT_STUB}" "${RPC_SERVER_STUB}"
        COMMAND midl
            /acf "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/StopService.acf"
            /h   "StopService.h"
            /client stub
            /server stub
            /out  "${RPC_GEN_DIR}"
            "${RPC_IDL}"
        DEPENDS "${RPC_IDL}" "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/StopService.acf"
        COMMENT "Running MIDL compiler on StopService.idl"
    )

    add_custom_target(rpc_generated DEPENDS "${RPC_HEADER}" "${RPC_CLIENT_STUB}" "${RPC_SERVER_STUB}")
else()
    set(RPC_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/generated")

    set(RPC_CLIENT_STUB "${RPC_GEN_DIR}/StopService_c.c")
    set(RPC_SERVER_STUB "${RPC_GEN_DIR}/StopService_s.c")
    set(RPC_HEADER      "${RPC_GEN_DIR}/StopService.h")

    add_custom_target(rpc_generated)
endif()

add_subdirectory("TrayApp")
add_subdirectory("TrayService")
