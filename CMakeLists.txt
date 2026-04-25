cmake_minimum_required(VERSION 3.8)

if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("TrayProject" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(RPC_IDL     "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/TrayService.idl")
set(RPC_ACF     "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/TrayService.acf")
set(RPC_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/rpc_generated")
file(MAKE_DIRECTORY "${RPC_GEN_DIR}")

set(RPC_HEADER      "${RPC_GEN_DIR}/TrayService.h")
set(RPC_CLIENT_STUB "${RPC_GEN_DIR}/TrayService_c.c")
set(RPC_SERVER_STUB "${RPC_GEN_DIR}/TrayService_s.c")

add_custom_command(
    OUTPUT  "${RPC_HEADER}" "${RPC_CLIENT_STUB}" "${RPC_SERVER_STUB}"
    COMMAND midl
        /acf  "${RPC_ACF}"
        /h    "TrayService.h"
        /client stub /server stub
        /out  "${RPC_GEN_DIR}"
        "${RPC_IDL}"
    DEPENDS "${RPC_IDL}" "${RPC_ACF}"
    COMMENT "MIDL: TrayService.idl"
)
add_custom_target(rpc_generated
    DEPENDS "${RPC_HEADER}" "${RPC_CLIENT_STUB}" "${RPC_SERVER_STUB}")

set(STOP_IDL    "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/StopService.idl")
set(STOP_ACF    "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/StopService.acf")
set(STOP_HEADER "${RPC_GEN_DIR}/StopService.h")
set(STOP_CLIENT "${RPC_GEN_DIR}/StopService_c.c")
set(STOP_SERVER "${RPC_GEN_DIR}/StopService_s.c")

add_custom_command(
    OUTPUT  "${STOP_HEADER}" "${STOP_CLIENT}" "${STOP_SERVER}"
    COMMAND midl
        /acf  "${STOP_ACF}"
        /h    "StopService.h"
        /client stub /server stub
        /out  "${RPC_GEN_DIR}"
        "${STOP_IDL}"
    DEPENDS "${STOP_IDL}" "${STOP_ACF}"
    COMMENT "MIDL: StopService.idl"
)
add_custom_target(rpc_stop_generated
    DEPENDS "${STOP_HEADER}" "${STOP_CLIENT}" "${STOP_SERVER}")

add_subdirectory("TrayApp")
add_subdirectory("TrayService")