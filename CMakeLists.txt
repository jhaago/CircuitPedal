cmake_minimum_required(VERSION 3.16)
project(CircuitPedal VERSION 0.2.0 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CIRCUITPEDAL_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

function(circuitpedal_target_defaults target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        )
        if(CIRCUITPEDAL_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(circuit_core
    src/DistortionPlusModel.cpp
)
target_include_directories(circuit_core PUBLIC src)
circuitpedal_target_defaults(circuit_core)

add_executable(circuit_core_test
    src/core_test.cpp
)
target_link_libraries(circuit_core_test PRIVATE circuit_core)
circuitpedal_target_defaults(circuit_core_test)

if(BUILD_TESTING)
    add_test(NAME circuit_core_validation COMMAND circuit_core_test)
endif()

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
    circuitpedal_target_defaults(circuitpedal)
else()
    message(STATUS "Live audio target is macOS-only. Building circuit_core_test only.")
endif()
