cmake_minimum_required(VERSION 3.16)
project(CircuitPedal VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(circuit_core
    src/DistortionPlusModel.cpp
)
target_include_directories(circuit_core PUBLIC src)

add_executable(circuit_core_test
    src/core_test.cpp
)
target_link_libraries(circuit_core_test PRIVATE circuit_core)

if(APPLE)
    add_executable(circuitpedal
        src/main_mac.cpp
    )
    target_link_libraries(circuitpedal PRIVATE
        circuit_core
        "-framework AudioUnit"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
else()
    message(STATUS "Live audio target is V0.1 macOS-only. Building circuit_core_test only.")
endif()
