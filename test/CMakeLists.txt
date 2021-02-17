add_executable(s3select_test s3select_test.cpp)
target_include_directories(s3select_test PUBLIC ../include)
target_link_libraries(s3select_test gtest gtest_main boost_date_time boost_thread boost_system -lpthread -lm)

include(GoogleTest)
gtest_discover_tests(s3select_test)
