add_executable(s3select_test s3select_test.cpp)
target_include_directories(s3select_test PUBLIC ../rapidjson/include/ ../include)

add_executable(s3select_perf_test s3select_perf_test.cpp)
target_include_directories(s3select_perf_test PUBLIC ../rapidjson/include/ ../include)

add_executable(s3select_test_json_parser s3select_test_json_parser.cpp)
target_include_directories(s3select_test_json_parser PUBLIC ../rapidjson/include/ ../include)

find_package(Arrow QUIET)

if(Arrow_FOUND)
  message( "arrow is installed")
  target_link_libraries(s3select_test gtest gtest_main boost_date_time boost_thread parquet arrow pthread )
  target_link_libraries(s3select_perf_test gtest gtest_main boost_date_time boost_thread parquet arrow pthread profiler)
  target_link_libraries(s3select_test_json_parser gtest gtest_main boost_date_time boost_thread parquet arrow pthread )
else()
  target_link_libraries(s3select_test gtest gtest_main boost_date_time boost_thread pthread )
  target_link_libraries(s3select_perf_test gtest gtest_main boost_date_time boost_thread pthread profiler)
  target_link_libraries(s3select_test_json_parser gtest gtest_main boost_date_time boost_thread pthread )
endif()

include(GoogleTest)
gtest_discover_tests(s3select_test)
gtest_discover_tests(s3select_perf_test)
gtest_discover_tests(s3select_test_json_parser)
