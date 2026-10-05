cmake_minimum_required(VERSION 3.22)
project(invc_exam_workspace C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(sender sender-build)
add_subdirectory(receiver receiver-build)

add_custom_target(all_firmware ALL
    DEPENDS sender receiver
    COMMENT "Building all firmware targets"
)
