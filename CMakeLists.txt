cmake_minimum_required(VERSION 3.20)
project(ultra_low_latency_trading VERSION 0.1.0 LANGUAGES CXX)

# Enforce strict C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use clang by default and optimization flags
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(WARNING "This project is optimized for Clang. Consider using clang++.")
endif()

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags for low-latency performance
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fsanitize=address,undefined")

# Warning flags
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wno-unused-parameter  # Common in interfaces
)

# Find required packages
find_package(Threads REQUIRED)

# Find curl for HTTP requests (Alpha Vantage API)
find_package(CURL REQUIRED)

# Find Qt6 for GUI application
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network Charts)

# Enable Qt6 MOC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Check for common dependencies that will be needed
find_package(PkgConfig QUIET)

# Create interface library for common compile features
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_20)

# Create interface library for common dependencies
add_library(project_dependencies INTERFACE)
target_link_libraries(project_dependencies INTERFACE 
    Threads::Threads
    CURL::libcurl
)

# Create interface library for Qt dependencies
add_library(qt_dependencies INTERFACE)
target_link_libraries(qt_dependencies INTERFACE
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Charts
)

# Add subdirectories
add_subdirectory(src/engine/ingest)
add_subdirectory(src/engine/ilp_writer)

# Alpha Vantage → QuestDB Pipeline Application (Console)
add_executable(alpha_vantage_pipeline
    src/engine/alpha_vantage_pipeline_app.cpp
)

target_link_libraries(alpha_vantage_pipeline
    ingest
    ilp_writer
    project_dependencies
)

target_include_directories(alpha_vantage_pipeline PRIVATE
    src/engine/ingest
    src/engine/ilp_writer
)

# BSE Trading System Qt GUI Application
add_executable(bse_trading_gui
    src/gui/main.cpp
    src/gui/main_window.cpp
    src/gui/questdb_connector.cpp
)

target_link_libraries(bse_trading_gui
    qt_dependencies
    project_dependencies
)

target_include_directories(bse_trading_gui PRIVATE
    src/gui
)

# Temporarily disable tests during simplification
# add_subdirectory(tests)

# Enable testing
enable_testing()

# Performance targets
add_custom_target(perf
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_perf_tests.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running performance tests"
)

# Benchmarking target
add_custom_target(benchmark
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_phase1.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running Phase 1 end-to-end benchmark"
)

# Format target
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    add_custom_target(format
        COMMAND find ${CMAKE_CURRENT_SOURCE_DIR}/src -name "*.cpp" -o -name "*.h" | xargs ${CLANG_FORMAT} -i
        COMMENT "Formatting code with clang-format"
    )
endif()