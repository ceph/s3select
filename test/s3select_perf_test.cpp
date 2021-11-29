#include <chrono>
#include <inttypes.h>
#include <iostream>
#include <math.h>
#include "gperftools/profiler.h"
#include "s3select_test.h"

#define COUNT 100000

void run_query(std::string& query, std::string& csv, std::string profiler_file, std::string& result)
{
  auto start = std::chrono::high_resolution_clock::now();

  ProfilerStart(profiler_file.c_str());

  result = run_s3select(query, csv);

  ProfilerFlush();
  ProfilerStop();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<long, std::micro> elapsed_us =
          std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

  fprintf(stderr, "Total time taken by query: %" PRIu64 " μs\nAverage time per row: %" PRIu64 " μs\n",
          elapsed_us.count(), elapsed_us.count() / (COUNT * 10));
}

void generate_number_csv(std::string& out, size_t size) {
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << 1 << "," << 2.0 << std::endl;
    ss << -3 << "," << 4.10 << std::endl;
    ss << 5 << "," << -6.09 << std::endl;
    ss << -7 << "," << -8.3 << std::endl;
    ss << 9 << "," << 10.5 << std::endl;
    ss << -11 << "," << 12.35 << std::endl;
    ss << -13 << "," << -14.35 << std::endl;
    ss << -15 << "," << 16.15 << std::endl;
    ss << 17 << "," << 18.95 << std::endl;
    ss << -19 << "," << 20.49 << std::endl;
  }
  out = ss.str();
}

void generate_timestamp_csv(std::string& out, size_t size) {
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << "2007T" << std::endl;
    ss << "2019-10-25T01:50:22+04:30" << std::endl;
    ss << "2008-08-08T15:10-00:00" << std::endl;
    ss << "2021-03-14T03:35:01-01:45" << std::endl;
    ss << "2011-11-11T11:11:11Z" << std::endl;
    ss << "2020-06-30T19:17Z" << std::endl;
    ss << "2005-01-15T20:00:00.64+03:00" << std::endl;
    ss << "2001-02-10T00:12Z" << std::endl;
    ss << "2010-03-10T" << std::endl;
    ss << "2001-04-19T08:00:00+05:30" << std::endl;
  }
  out = ss.str();
}

void generate_timestamp_to_string_dynamic_csv(std::string& out, size_t size) {
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << "2007T,yyyyMMdd-H:m:s" << std::endl;
    ss << "2019-10-25T01:50:22+04:30,yyyyMMdd-H:m:S" << std::endl;
    ss << "2008-08-08T15:10-00:00,yyyMd-H:m:s qw" << std::endl;
    ss << "2021-03-14T03:35:01-01:45,yydaMMMM h m s.n" << std::endl;
    ss << "2011-11-11T11:11:11Z,yyyyMMdd-H:m:s" << std::endl;
    ss << "2020-06-30T19:17Z,yydaMMMM h m s.n" << std::endl;
    ss << "2005-01-15T20:00:00.64+03:00,yyyyMMdd-H:m:s" << std::endl;
    ss << "2001-02-10T00:12Z,yyyMd-H:m:s qw" << std::endl;
    ss << "2010-03-10T,yyyyMMdd-H:m:S" << std::endl;
    ss << "2001-04-19T08:00:00+05:30,yydaMMMM h m s.n" << std::endl;
  }
  out = ss.str();
}

void generate_data_csv(std::string& out, size_t size) {
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << "1,42926,7334,5.5,Brandise,Letsou,Brandise.Letsou@yopmail.com,worker,2020-10-26T11:21:30.397Z,__" << std::endl;
    ss << "2,21169,3648,9.0,Zaria,Weinreb,Zaria.Weinreb@yopmail.com,worker,2009-12-02T01:22:45.8327+09:45,__" << std::endl;
    ss << "3,35581,9091,2.1,Bibby,Primalia,Bibby.Primalia@yopmail.com,doctor,2001-02-27T23:18:23.446633-12:00,__" << std::endl;
    ss << "4,38388,7345,4.7,Damaris,Arley,Damaris.Arley@yopmail.com,firefighter,1995-08-24T01:40:00+12:30,__" << std::endl;
    ss << "5,42802,6464,7.0,Georgina,Georas,Georgina.Georas@yopmail.com,worker,2013-01-30T05:27:59.2Z,__" << std::endl;
    ss << "6,45582,5863,0.1,Kelly,Hamil,Kelly.Hamil@yopmail.com,police officer,1998-03-31T17:25-01:05,__" << std::endl;
    ss << "7,8548,7665,3.6,Claresta,Flita,Claresta.Flita@yopmail.com,doctor,2007-10-10T22:00:30Z,__" << std::endl;
    ss << "8,22633,528,5.3,Bibby,Virgin,Bibby.Virgin@yopmail.com,developer,2020-06-30T11:07:01.23323-00:30,__" << std::endl;
    ss << "9,38439,5645,2.8,Mahalia,Aldric,Mahalia.Aldric@yopmail.com,doctor,2019-04-20T20:21:22.23+05:15,__" << std::endl;
    ss << "10,6611,7287,1.0,Pamella,Sibyls,Pamella.Sibyls@yopmail.com,police officer,2000-09-13T14:41Z,1_" << std::endl;
  }
  out = ss.str();
}

void generate_single_large_col_csv(std::string& out, size_t size) {
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << "1" << std::string(700, 'q') << std::endl;
    ss << "2" << std::string(700, 'w') << std::endl;
    ss << "3" << std::string(700, 'e') << std::endl;
    ss << "4" << std::string(700, 'r') << std::endl;
    ss << "5" << std::string(700, 't') << std::endl;
    ss << "6" << std::string(700, 'y') << std::endl;
    ss << "7" << std::string(700, 'u') << std::endl;
    ss << "8" << std::string(700, 'i') << std::endl;
    ss << "9" << std::string(700, 'o') << std::endl;
    ss << "10" << std::string(700, 'p') << "mmgg88" << std::endl;
  }
  out = ss.str();
}

TEST(TestS3SelectPerformance, PARSE_WITH_RESULT)
{
  std::string input_csv;
  generate_data_csv(input_csv, COUNT);
  std::string input_query = "select _1, _2, _3, _4, _5, _6, _7, _8, _9 from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, PARSE_WITHOUT_RESULT_MULTI_COL_SMALL)
{
  std::string input_csv;
  generate_data_csv(input_csv, COUNT);
  std::string input_query = "select count(0) from s3object where _2 > _3;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, PARSE_WITHOUT_RESULT_SINGLE_COL_SMALL)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select count(0) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, PARSE_WITHOUT_RESULT_SINGLE_COL_LARGE)
{
  std::string input_csv;
  generate_single_large_col_csv(input_csv, COUNT);
  std::string input_query = "select count(0) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, ADD)
{
  std::string input_csv;
  generate_number_csv(input_csv, COUNT);
  std::string input_query = "select add(int(_1), float(_2)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, SUBTRACT)
{
  std::string input_csv;
  generate_number_csv(input_csv, COUNT);
  std::string input_query = "select int(_1) - float(_2) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, MULTIPLY)
{
  std::string input_csv;
  generate_number_csv(input_csv, COUNT);
  std::string input_query = "select int(_1) * float(_2) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DIVIDE)
{
  std::string input_csv;
  generate_number_csv(input_csv, COUNT);
  std::string input_query = "select int(_1) / float(_2) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, POWER)
{
  std::string input_csv;
  generate_number_csv(input_csv, COUNT);
  std::string input_query = "select int(_1) ^ float(_2) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, TO_TIMESTAMP)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select to_timestamp(_1) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_DIFF_YEAR)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_diff(year, to_timestamp(_1), to_timestamp(\'2009-09-17T17:56:06.234Z\')) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_DIFF_MONTH)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_diff(month, to_timestamp(_1), to_timestamp(\'2009-09-17T17:56:06.234Z\')) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_DIFF_DAY)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_diff(day, to_timestamp(_1), to_timestamp(\'2009-09-17T17:56:06.234Z\')) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_DIFF_HOUR)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_diff(hour, to_timestamp(_1), to_timestamp(\'2009-09-17T17:56:06.234Z\')) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_DIFF_MINUTE)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_diff(minute, to_timestamp(_1), to_timestamp(\'2009-09-17T17:56:06.234Z\')) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_DIFF_SECOND)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_diff(second, to_timestamp(_1), to_timestamp(\'2009-09-17T17:56:06.234Z\')) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_ADD_YEAR)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_add(year, 2, to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_ADD_MONTH)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_add(month, -5, to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_ADD_DAY)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_add(day, 10, to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_ADD_HOUR)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_add(hour, 3, to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_ADD_MINUTE)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_add(minute, -15, to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, DATE_ADD_SECOND)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select date_add(second, 30, to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_YEAR)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(year from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_MONTH)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(month from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_DAY)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(day from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_HOUR)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(hour from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_MINUTE)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(minute from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_SECOND)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(second from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_WEEK)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(week from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_TIMEZONE_HOUR)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(timezone_hour from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, EXTRACT_TIMEZONE_MINUTE)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select extract(timezone_minute from to_timestamp(_1)) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, TIMESTAMP_TO_STRING_CONSTANT)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select to_string(to_timestamp(_1), \'y MMMMM  dTHH : m S n - a XXX xxxxx\') from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, TIMESTAMP_TO_STRING_DYNAMIC)
{
  std::string input_csv;
  generate_timestamp_to_string_dynamic_csv(input_csv, COUNT);
  std::string input_query = "select to_string(to_timestamp(_1), _2) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, SUBSTRING)
{
  std::string input_csv;
  generate_timestamp_csv(input_csv, COUNT);
  std::string input_query = "select substring(_1, 2, 4) from s3object;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);
}

TEST(TestS3SelectPerformance, LIKE_CONSTANT_SMALL)
{
  std::string input_csv;
  generate_data_csv(input_csv, COUNT);
  std::string input_query = "select count(*) from s3object where _1 like \"1_\";" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);

  EXPECT_EQ(s3select_res, "100000");
}

TEST(TestS3SelectPerformance, LIKE_CONSTANT_LARGE)
{
  std::string input_csv;
  generate_single_large_col_csv(input_csv, COUNT);
  std::string input_query = "select count(*) from s3object where _1 like \"10%mg__8\";" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);

  EXPECT_EQ(s3select_res, "100000");
}

TEST(TestS3SelectPerformance, LIKE_DYNAMIC)
{
  std::string input_csv;
  generate_data_csv(input_csv, COUNT);
  std::string input_query = "select count(*) from s3object where _1 like _10;" ;
  std::string s3select_res;

  run_query(input_query, input_csv, "./perf_.txt", s3select_res);

  EXPECT_EQ(s3select_res, "100000");
}
