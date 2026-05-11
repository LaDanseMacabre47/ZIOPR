cmake_minimum_required(VERSION 3.8)

project("TrayProject" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ----------------------------------------------------------------
# RPC: используем заранее сгенерированные stub-файлы
# (сгенерированы на Windows через MIDL и закоммичены в репозиторий)
# ----------------------------------------------------------------
set(RPC_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rpc_interface/generated")

add_subdirectory("TrayApp")
add_subdirectory("TrayService")
add_subdirectory("AvDbTool")