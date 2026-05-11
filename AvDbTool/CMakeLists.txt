cmake_minimum_required(VERSION 3.8)

add_executable(AvDbTool "main.cpp" "AvDbFormat.h")

target_compile_definitions(AvDbTool PRIVATE UNICODE _UNICODE NOMINMAX)
target_link_libraries(AvDbTool PRIVATE bcrypt crypt32 advapi32)
set_property(TARGET AvDbTool PROPERTY CXX_STANDARD 20)