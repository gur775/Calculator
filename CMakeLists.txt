cmake_minimum_required(VERSION 3.20)

project(Calculator VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This project loads Windows DLL plugins and currently requires Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(set_common_options target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(set_runtime_directory target directory)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${directory}")
    foreach(configuration Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${configuration}" configuration_upper)
        set_target_properties(${target} PROPERTIES
            "RUNTIME_OUTPUT_DIRECTORY_${configuration_upper}" "${directory}"
        )
    endforeach()
endfunction()

add_executable(calc
    src/calculator.cpp
    src/main.cpp
    src/plugin_manager.cpp
)
target_include_directories(calc PRIVATE include src)
target_compile_definitions(calc PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
set_common_options(calc)
set_runtime_directory(calc "${CMAKE_BINARY_DIR}/bin")

add_library(funcsin SHARED plugins/sin_plugin.cpp)
target_include_directories(funcsin PRIVATE include)
target_compile_definitions(funcsin PRIVATE CALC_PLUGIN_BUILD)
set_common_options(funcsin)
set_runtime_directory(funcsin "${CMAKE_BINARY_DIR}/bin/plugins")

add_library(funcdeg SHARED plugins/power_plugin.cpp)
target_include_directories(funcdeg PRIVATE include)
target_compile_definitions(funcdeg PRIVATE CALC_PLUGIN_BUILD)
set_common_options(funcdeg)
set_runtime_directory(funcdeg "${CMAKE_BINARY_DIR}/bin/plugins")

