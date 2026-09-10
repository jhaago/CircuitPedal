cmake_minimum_required(VERSION 3.16)
project(CircuitPedal VERSION 0.18.0 LANGUAGES CXX)

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
    src/GenericCircuit.cpp
    src/GenericCircuitProcessor.cpp
    src/CircuitFile.cpp
)
target_include_directories(circuit_core PUBLIC src)
circuitpedal_target_defaults(circuit_core)

add_library(circuit_validation
    src/Validation.cpp
)
target_include_directories(circuit_validation PUBLIC src)
circuitpedal_target_defaults(circuit_validation)

add_executable(circuit_core_test
    src/core_test.cpp
)
target_link_libraries(circuit_core_test PRIVATE circuit_core)
circuitpedal_target_defaults(circuit_core_test)

add_executable(generic_circuit_test
    src/generic_circuit_test.cpp
)
target_link_libraries(generic_circuit_test PRIVATE circuit_core)
circuitpedal_target_defaults(generic_circuit_test)

add_executable(circuit_file_test
    src/circuit_file_test.cpp
)
target_link_libraries(circuit_file_test PRIVATE circuit_core)
target_compile_definitions(circuit_file_test PRIVATE
    CIRCUITPEDAL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
circuitpedal_target_defaults(circuit_file_test)

add_executable(validation_test
    src/validation_test.cpp
)
target_link_libraries(validation_test PRIVATE circuit_validation)
circuitpedal_target_defaults(validation_test)

add_executable(circuitpedal_validate
    src/validation_cli.cpp
)
target_link_libraries(circuitpedal_validate PRIVATE circuit_core circuit_validation)
circuitpedal_target_defaults(circuitpedal_validate)

if(BUILD_TESTING)
    add_test(NAME circuit_core_validation COMMAND circuit_core_test)
    add_test(NAME generic_circuit_validation COMMAND generic_circuit_test)
    add_test(NAME circuit_file_validation COMMAND circuit_file_test)
    add_test(NAME validation_framework COMMAND validation_test)

    add_test(NAME validation_cli_render
        COMMAND circuitpedal_validate render
            "${CMAKE_CURRENT_SOURCE_DIR}/circuits/two_transistor_fuzz_demo.cpedal"
            "${CMAKE_CURRENT_BINARY_DIR}/validation_smoke.csv"
            --sample-rate 48000
            --seconds 0.02
            --signal sine
            --frequency 110
            --amplitude 0.1
    )
    add_test(NAME validation_cli_compare_self
        COMMAND circuitpedal_validate compare
            "${CMAKE_CURRENT_BINARY_DIR}/validation_smoke.csv"
            "${CMAKE_CURRENT_BINARY_DIR}/validation_smoke.csv"
            --max-lag 8
            --max-nrms 0.000001
            --max-peak 0.000001
    )
    set_tests_properties(validation_cli_compare_self PROPERTIES
        DEPENDS validation_cli_render
    )
endif()

if(APPLE)
    enable_language(OBJCXX)

    add_library(circuit_mac_audio
        src/MacAudioEngine.cpp
    )
    target_include_directories(circuit_mac_audio PUBLIC src)
    target_link_libraries(circuit_mac_audio PUBLIC
        circuit_core
        "-framework AudioUnit"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreFoundation"
    )
    circuitpedal_target_defaults(circuit_mac_audio)

    add_executable(circuitpedal
        src/main_mac.cpp
    )
    target_link_libraries(circuitpedal PRIVATE circuit_mac_audio)
    circuitpedal_target_defaults(circuitpedal)

    file(GLOB CIRCUITPEDAL_CIRCUIT_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/circuits/*.cpedal"
    )

    add_executable(CircuitPedalGUI MACOSX_BUNDLE
        src/main_mac_gui.mm
        ${CIRCUITPEDAL_CIRCUIT_FILES}
    )
    set_source_files_properties(${CIRCUITPEDAL_CIRCUIT_FILES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources/circuits"
    )
    target_link_libraries(CircuitPedalGUI PRIVATE
        circuit_mac_audio
        "-framework AppKit"
    )
    target_compile_options(CircuitPedalGUI PRIVATE -fobjc-arc)
    set_target_properties(CircuitPedalGUI PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "CircuitPedal"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.jhaago.CircuitPedalGUI"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
    )
    circuitpedal_target_defaults(CircuitPedalGUI)
else()
    message(STATUS "Live audio target is macOS-only. Building circuit validation targets only.")
endif()
