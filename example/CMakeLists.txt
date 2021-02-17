add_executable(s3select_example s3select_example.cpp)
target_include_directories(s3select_example PUBLIC ../include)
target_link_libraries(s3select_example boost_date_time boost_system boost_thread parquet arrow_shared)

add_executable(generate_rand_csv generate_rand_csv.c)

add_custom_command(OUTPUT expr_genrator.py COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/expr_genrator.py expr_genrator.py
    COMMENT "Copy expr_genrator.py"
    VERBATIM)

add_custom_target(expr_generator ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/expr_genrator.py)

