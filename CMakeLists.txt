cmake_minimum_required(VERSION 3.22)
project(invc_exam_workspace NONE)

find_program(CMAKE_EXECUTABLE cmake REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Build sender subproject
add_custom_target(sender
    COMMAND ${CMAKE_EXECUTABLE} --preset ${CMAKE_BUILD_TYPE}
    COMMAND ${CMAKE_EXECUTABLE} --build --preset ${CMAKE_BUILD_TYPE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sender"
    COMMENT "Building sender (${CMAKE_BUILD_TYPE})"
    USES_TERMINAL
)

# Build receiver subproject
add_custom_target(receiver
    COMMAND ${CMAKE_EXECUTABLE} --preset ${CMAKE_BUILD_TYPE}
    COMMAND ${CMAKE_EXECUTABLE} --build --preset ${CMAKE_BUILD_TYPE}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/receiver"
    COMMENT "Building receiver (${CMAKE_BUILD_TYPE})"
    USES_TERMINAL
)

# Build all subprojects
add_custom_target(all_firmware ALL
    DEPENDS sender receiver
    COMMENT "Building all firmware targets"
)
