cmake_minimum_required(VERSION 3.20)

# ILP Writer library - simplified
add_library(ilp_writer STATIC
    ilp_writer.cpp
)

target_include_directories(ilp_writer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ILP Writer application
add_executable(ilp_writer_app
    main.cpp
)

target_link_libraries(ilp_writer_app
    ilp_writer
    ingest
)