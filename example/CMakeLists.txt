add_executable(s3select_example s3select_example.cpp)
target_include_directories(s3select_example PUBLIC ../include ../rapidjson/include)
add_executable(s3select_scaleup s3select_scaleup.cpp)
target_include_directories(s3select_scaleup PUBLIC ../include ../rapidjson/include)

find_package(Arrow QUIET)

if(Arrow_FOUND)
  message( "arrow is installed")
  add_executable(csv_to_parquet csv_to_parquet.cpp)
  target_include_directories(csv_to_parquet PUBLIC ../include)
  target_link_libraries(s3select_example boost_date_time boost_system boost_thread parquet arrow boost_filesystem)
  target_link_libraries(s3select_scaleup boost_date_time boost_system boost_thread parquet arrow boost_filesystem pthread)
  target_link_libraries(csv_to_parquet boost_date_time boost_system boost_thread parquet arrow)
else()
  target_link_libraries(s3select_example boost_date_time boost_system boost_thread boost_filesystem)
  target_link_libraries(s3select_scaleup boost_date_time boost_system boost_thread boost_filesystem)
endif()

add_executable(generate_rand_csv generate_rand_csv.c)

add_custom_command(OUTPUT expr_genrator.py COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/expr_genrator.py expr_genrator.py
    COMMENT "Copy expr_genrator.py"
    VERBATIM)

add_custom_target(expr_generator ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/expr_genrator.py)

