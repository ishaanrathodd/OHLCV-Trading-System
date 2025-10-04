cmake_minimum_required(VERSION 3.20)

# Ingest library - Alpha Vantage BSE integration
add_library(ingest STATIC
    alpha_vantage_client.cpp
)

target_include_directories(ingest PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Ingest application
add_executable(ingest_app
    main.cpp
)

target_link_libraries(ingest_app 
    ingest
    project_dependencies
)