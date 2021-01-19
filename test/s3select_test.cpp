#include "s3select.h"
#include "gtest/gtest.h"
#include <string>
#include <iomanip>
#include <algorithm>
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

using namespace s3selectEngine;

std::string run_expression_in_C_prog(const char* expression)
{
//purpose: per use-case a c-file is generated, compiles , and finally executed.

// side note: its possible to do the following: cat test_hello.c |  gcc  -pipe -x c - -o /dev/stdout > ./1
// gcc can read and write from/to pipe (use pipe2()) i.e. not using file-system , BUT should also run gcc-output from memory

  const int C_FILE_SIZE=(1024*1024);
  std::string c_test_file = std::string("/tmp/test_s3.c");
  std::string c_run_file = std::string("/tmp/s3test");

  FILE* fp_c_file = fopen(c_test_file.c_str(), "w");

  //contain return result
  char result_buff[100];

  char* prog_c = 0;

  if(fp_c_file)
  {
    prog_c = (char*)malloc(C_FILE_SIZE);

		size_t sz=sprintf(prog_c,"#include <stdio.h>\n \
				#include <float.h>\n \
				int main() \
				{\
				printf(\"%%.*e\\n\",DECIMAL_DIG,(double)(%s));\
				} ", expression);

    fwrite(prog_c, 1, sz, fp_c_file);
    fclose(fp_c_file);
  }

  std::string gcc_and_run_cmd = std::string("gcc ") + c_test_file + " -o " + c_run_file + " -Wall && " + c_run_file;

  FILE* fp_build = popen(gcc_and_run_cmd.c_str(), "r"); //TODO read stderr from pipe

  if(!fp_build)
  {
    if(prog_c)
	free(prog_c);

    return std::string("#ERROR#");
  }

  fgets(result_buff, sizeof(result_buff), fp_build);

  unlink(c_run_file.c_str());
  unlink(c_test_file.c_str());
  fclose(fp_build);

  if(prog_c)
    free(prog_c);

  return std::string(result_buff);
}

#define OPER oper[ rand() % oper.size() ]

class gen_expr
{

private:

  int open = 0;
  std::string oper= {"+-+*/*"};

  std::string gexpr()
  {
    return std::to_string(rand() % 1000) + ".0" + OPER + std::to_string(rand() % 1000) + ".0";
  }

  std::string g_openp()
  {
    if ((rand() % 3) == 0)
    {
      open++;
      return std::string("(");
    }
    return std::string("");
  }

  std::string g_closep()
  {
    if ((rand() % 2) == 0 && open > 0)
    {
      open--;
      return std::string(")");
    }
    return std::string("");
  }

public:

  std::string generate()
  {
    std::string exp = "";
    open = 0;

    for (int i = 0; i < 10; i++)
    {
      exp = (exp.size() > 0 ? exp + OPER : std::string("")) + g_openp() + gexpr() + OPER + gexpr() + g_closep();
    }

    if (open)
      for (; open--;)
      {
        exp += ")";
      }

    return exp;
  }
};

const std::string failure_sign("#failure#");

std::string run_s3select(std::string expression)
{//purpose: run query on single row and return result(single projections).
  s3select s3select_syntax;

  int status = s3select_syntax.parse_query(expression.c_str());

  if(status)
    return failure_sign;

  std::string s3select_result;
  s3selectEngine::csv_object  s3_csv_object(&s3select_syntax);
  std::string in = "1,1,1,1\n";

  s3_csv_object.run_s3select_on_object(s3select_result, in.c_str(), in.size(), false, false, true);

  s3select_result = s3select_result.substr(0, s3select_result.find_first_of(","));

  return s3select_result;
}

std::string run_s3select(std::string expression,std::string input)
{//purpose: run query on multiple rows and return result(multiple projections).
  s3select s3select_syntax;

  int status = s3select_syntax.parse_query(expression.c_str());

  if(status)
    return failure_sign;

  std::string s3select_result;
  s3selectEngine::csv_object  s3_csv_object(&s3select_syntax);

  s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), false, false, true);

  return s3select_result;
}

TEST(TestS3SElect, s3select_vs_C)
{
//purpose: validate correct processing of arithmetical expression, it is done by running the same expression
// in C program.
// the test validate that syntax and execution-tree (including precedence rules) are done correctly

  for(int y=0; y<10; y++)
  {
    gen_expr g;
    std::string exp = g.generate();
    std::string c_result = run_expression_in_C_prog( exp.c_str() );

    char* err=0;
    double  c_dbl_res = strtod(c_result.c_str(), &err);

    std::string input_query = "select " + exp + " from stdin;" ;
    std::string s3select_res = run_s3select(input_query);

    double  s3select_dbl_res = strtod(s3select_res.c_str(), &err);

    //std::cout << exp << " " << s3select_dbl_res << " " << s3select_res << " " << c_dbl_res/s3select_dbl_res << std::endl;
    //std::cout << exp << std::endl;

    ASSERT_EQ(c_dbl_res, s3select_dbl_res);
  }
}

TEST(TestS3SElect, ParseQuery)
{
  //TODO syntax issues ?
  //TODO error messeges ?

  s3select s3select_syntax;

  run_s3select(std::string("select (1+1) from stdin;"));

  ASSERT_EQ(0, 0);
}

TEST(TestS3SElect, int_compare_operator)
{
  value a10(10), b11(11), c10(10);

  ASSERT_EQ( a10 < b11, true );
  ASSERT_EQ( a10 > b11, false );
  ASSERT_EQ( a10 >= c10, true );
  ASSERT_EQ( a10 <= c10, true );
  ASSERT_EQ( a10 != b11, true );
  ASSERT_EQ( a10 == b11, false );
  ASSERT_EQ( a10 == c10, true );
}

TEST(TestS3SElect, float_compare_operator)
{
  value a10(10.1), b11(11.2), c10(10.1);

  ASSERT_EQ( a10 < b11, true );
  ASSERT_EQ( a10 > b11, false );
  ASSERT_EQ( a10 >= c10, true );
  ASSERT_EQ( a10 <= c10, true );
  ASSERT_EQ( a10 != b11, true );
  ASSERT_EQ( a10 == b11, false );
  ASSERT_EQ( a10 == c10, true );

}

TEST(TestS3SElect, string_compare_operator)
{
  value s1("abc"), s2("def"), s3("abc");

  ASSERT_EQ( s1 < s2, true );
  ASSERT_EQ( s1 > s2, false );
  ASSERT_EQ( s1 <= s3, true );
  ASSERT_EQ( s1 >= s3, true );
  ASSERT_EQ( s1 != s2, true );
  ASSERT_EQ( s1 == s3, true );
  ASSERT_EQ( s1 == s2, false );
}

TEST(TestS3SElect, arithmetic_operator)
{
  value a(1), b(2), c(3), d(4);

  ASSERT_EQ( (a+b).i64(), 3 );

  ASSERT_EQ( (value(0)-value(2)*value(4)).i64(), -8 );
  ASSERT_EQ( (value(1.23)-value(0.1)*value(2)).dbl(), 1.03 );

  a=int64_t(1); //a+b modify a
  ASSERT_EQ( ( (a+b) * (c+d) ).i64(), 21 );
}

TEST(TestS3SElect, intnan_compare_operator)
{
  value a10(10), b11(11), c10(10), d, e;
  d.set_nan();
  e.set_nan();
  ASSERT_EQ( d > b11, false );
  ASSERT_EQ( d >= c10, false );
  ASSERT_EQ( d < a10, false );
  ASSERT_EQ( d <= b11, false );
  ASSERT_EQ( d != a10, true );
  ASSERT_EQ( d != e, true );
  ASSERT_EQ( d == a10, false );
}

TEST(TestS3SElect, floatnan_compare_operator)
{
  value a10(10.1), b11(11.2), c10(10.1), d, e;
  d.set_nan();
  e.set_nan();
  ASSERT_EQ( d > b11, false );
  ASSERT_EQ( d >= c10, false );
  ASSERT_EQ( d < a10, false );
  ASSERT_EQ( d <= b11, false );
  ASSERT_EQ( d != a10, true );
  ASSERT_EQ( d != e, true );
  ASSERT_EQ( d == a10, false );
}

TEST(TestS3SElect, null_arithmetic_operator)
{
  value a(7), d, e(0);
  d.setnull();
  ASSERT_EQ((a + d).to_string(), "null" );
  ASSERT_EQ((a - d).to_string(), "null" );
  ASSERT_EQ((a * d).to_string(), "null" );
  ASSERT_EQ((a / d).to_string(), "null" ); 
  ASSERT_EQ((a / e).to_string(), "null" ); 
  ASSERT_EQ((d + a).to_string(), "null" );
  ASSERT_EQ((d - a).to_string(), "null" );
  ASSERT_EQ((d * a).to_string(), "null" );
  ASSERT_EQ((d / a).to_string(), "null" ); 
  ASSERT_EQ((e / a).to_string(), "null" );
}

TEST(TestS3SElect, nan_arithmetic_operator)
{
  value a(7), d, y(0);
  d.set_nan();
  float b = ((a + d).dbl() );
  float c = ((a - d).dbl() );
  float v = ((a * d).dbl() );
  float w = ((a / d).dbl() );
  float x = ((d / y).dbl() );
  float r = ((d + a).dbl() );
  float z = ((d - a).dbl() );
  float u = ((d * a).dbl() );
  float t = ((d / a).dbl() );
  EXPECT_FALSE(b <= b); 
  EXPECT_FALSE(c <= c);
  EXPECT_FALSE(v <= v);
  EXPECT_FALSE(w <= w);
  EXPECT_FALSE(x <= x);
  EXPECT_FALSE(r <= r); 
  EXPECT_FALSE(z <= z);
  EXPECT_FALSE(u <= u);
  EXPECT_FALSE(t <= t);
}

TEST(TestS3selectFunctions, timestamp)
{
    std::string timestamp = "2007T";
    std::string out_timestamp = "2007-01-01T00:00:00";
    std::string input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T";
    out_timestamp = "2007-09-17T00:00:00";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56Z";
    out_timestamp = "2007-09-17T17:56:00";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05Z";
    out_timestamp = "2007-09-17T17:56:05";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05.234Z";
    out_timestamp = "2007-09-17T17:56:05.234000";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56+22:08";
    out_timestamp = "2007-09-17T17:56:00";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05-05:30";
    out_timestamp = "2007-09-17T17:56:05";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05.234+02:44";
    out_timestamp = "2007-09-17T17:56:05.234000";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

}

TEST(TestS3selectFunctions, date_diff)
{
    std::string input_query = "select date_diff(year, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T19:30:05.234Z\')) from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-2");

    input_query = "select date_diff(month, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-24");

    input_query = "select date_diff(day, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-731");

    input_query = "select date_diff(hour, to_timestamp(\'2007-09-17T17:56:06.234Z\'), to_timestamp(\'2009-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "17545");

    input_query = "select date_diff(minute, to_timestamp(\'2007-09-17T17:56:06.234Z\'), to_timestamp(\'2009-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "1052733");

    input_query = "select date_diff(second, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2009-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "5639");
}

TEST(TestS3selectFunctions, date_add)
{
    std::string input_query = "select date_add(year, 2, to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2011-09-17T17:56:06.234567");

    input_query = "select date_add(month, -5, to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2009-04-17T17:56:06.234567");

    input_query = "select date_add(day, 3, to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2009-09-20T17:56:06.234567");

    input_query = "select date_add(hour, 1, to_timestamp(\'2007-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2007-09-17T18:56:06.234567");

    input_query = "select date_add(minute, 14, to_timestamp(\'2007-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2007-09-17T18:10:06.234567");

    input_query = "select date_add(second, -26, to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2009-09-17T17:55:40.234567");
}

TEST(TestS3selectFunctions, extract)
{
    std::string input_query = "select extract(year from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2009");

    input_query = "select extract(month from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "9");

    input_query = "select extract(day from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "260");

    input_query = "select extract(week from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "38");

    input_query = "select extract(hour from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "17");

    input_query = "select extract(minute from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "56");

    input_query = "select extract(second from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "6");
}

TEST(TestS3selectFunctions, utcnow)
{
    const boost::posix_time::ptime now(boost::posix_time::second_clock::universal_time());
    const std::string input_query = "select utcnow() from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    const boost::posix_time::ptime res_now;
    ASSERT_EQ(s3select_res, boost::posix_time::to_iso_extended_string(now));
}

TEST(TestS3selectFunctions, add)
{
    const std::string input_query = "select add(-5, 0.5) from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("-4.5"));
}

void generate_fix_columns_csv(std::string& out, size_t size) {
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << 1 << "," << 2 << "," << 3 << "," << 4 << "," << 5 << std::endl;
  }
  out = ss.str();
}

void generate_rand_csv(std::string& out, size_t size) {
  // schema is: int, float, string, string
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << rand()%1000 << "," << rand()%1000 << "," << rand()%1000 << "," << "foo"+std::to_string(i) << "," << std::to_string(i)+"bar" << std::endl;
  }
  out = ss.str();
}

void generate_csv(std::string& out, size_t size) {
  // schema is: int, float, string, string
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << i << "," << i/10.0 << "," << "foo"+std::to_string(i) << "," << std::to_string(i)+"bar" << std::endl;
  }
  out = ss.str();
}

void generate_rand_columns_csv(std::string& out, size_t size) {
  std::stringstream ss;
  auto r = [](){return rand()%1000;};

  for (auto i = 0U; i < size; ++i) {
    ss << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << std::endl;
  }
  out = ss.str();
}

void generate_rand_columns_csv_with_null(std::string& out, size_t size) {
  std::stringstream ss;
  auto r = [](){ int x=rand()%1000;if (x<100) return std::string(""); else return std::to_string(x);};

  for (auto i = 0U; i < size; ++i) {
    ss << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << std::endl;
  }
  out = ss.str();
}

void generate_csv_trim(std::string& out, size_t size) {
  // schema is: int, float, string, string
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << "     aeiou     " << "," << std::endl;
  }
  out = ss.str();
}

void generate_csv_like(std::string& out, size_t size) {
  // schema is: int, float, string, string
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << "fooaeioubrs" << "," << std::endl;
  }
  out = ss.str();
}

void generate_rand_columns_csv_datetime(std::string& out, size_t size) {
  std::stringstream ss;
  auto year = [](){return rand()%100 + 1900;};
  auto month = [](){return 1 + rand()%12;};
  auto day = [](){return 1 + rand()%28;};
  auto hours = [](){return rand()%24;};
  auto minutes = [](){return rand()%60;};
  auto seconds = [](){return rand()%60;};

  for (auto i = 0U; i < size; ++i) {
    ss << year() << "-" << std::setw(2) << std::setfill('0')<< month() << "-" << std::setw(2) << std::setfill('0')<< day() << "T" <<std::setw(2) << std::setfill('0')<< hours() << ":" << std::setw(2) << std::setfill('0')<< minutes() << ":" << std::setw(2) << std::setfill('0')<<seconds() << "Z" << "," << std::endl;
  }
  out = ss.str();
}

TEST(TestS3selectFunctions, sum)
{
    s3select s3select_syntax;
    const std::string input_query = "select sum(int(_1)), sum(float(_2)) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("8128,812.80000000000007,"));
}

TEST(TestS3selectFunctions, between)
{	
    //purpose: test different filter but with the same results 
    s3select s3select_syntax;
    s3select s3select_syntax_no_between;

    const std::string input_query = "select count(0) from stdin where int(_1) between int(_2) and int(_3) ;";
    const std::string input_query_no_between = "select count(0) from stdin where int(_1) >= int(_2) and int(_1) <= int(_3) ;";

    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    status = s3select_syntax_no_between.parse_query(input_query_no_between.c_str());
    ASSERT_EQ(status, 0);

    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    s3selectEngine::csv_object s3_csv_object_no_between(&s3select_syntax_no_between);

    std::string s3select_result;
    std::string s3select_result_no_between;
    std::string input;
    std::string input_no_bwtween;
    size_t size = 128;
    generate_rand_csv(input, size);
    input_no_bwtween = input;

    status = s3_csv_object_no_between.run_s3select_on_object(s3select_result_no_between, input_no_bwtween.c_str(), input_no_bwtween.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 

    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 

    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, s3select_result_no_between);
}

TEST(TestS3selectFunctions, count)
{
    s3select s3select_syntax;
    const std::string input_query = "select count(*) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("128,"));
}

TEST(TestS3selectFunctions, min)
{
    s3select s3select_syntax;
    const std::string input_query = "select min(int(_1)), min(float(_2)) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("0,0,"));
}

TEST(TestS3selectFunctions, max)
{
    s3select s3select_syntax;
    const std::string input_query = "select max(int(_1)), max(float(_2)) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("127,12.699999999999999,"));
}


int count_string(std::string in,std::string substr)
{
    int count = 0;
    size_t nPos = in.find(substr, 0); // first occurrence
    while(nPos != std::string::npos)
    {
        count++;
        nPos = in.find(substr, nPos + 1);
    }

    return count;
}

TEST(TestS3selectFunctions, binop_constant)
{
    //bug-fix for expresion with constant value on the left side(the bug change the constant values between rows)
    s3select s3select_syntax;
    const std::string input_query = "select 10+1,20-12,2*3,128/2,29%5,2^10 from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);

    int count = count_string(s3select_result,"11,8,6,64,4,1024");
    ASSERT_EQ(count,size);
}

TEST(TestS3selectOperator, add)
{
    const std::string input_query = "select -5 + 0.5 + -0.25 from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("-4.75"));
}

TEST(TestS3selectOperator, sub)
{
    const std::string input_query = "select -5 - 0.5 - -0.25 from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("-5.25"));
}

TEST(TestS3selectOperator, mul)
{
    const std::string input_query = "select -5 * (0.5 - -0.25) from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("-3.75"));
}

TEST(TestS3selectOperator, div)
{
    const std::string input_query = "select -5 / (0.5 - -0.25) from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("-6.666666666666667"));
}

TEST(TestS3selectOperator, pow)
{
    const std::string input_query = "select 5 ^ (0.5 - -0.25) from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("3.34370152488211"));
}

TEST(TestS3selectOperator, not_operator)
{
    const std::string input_query = "select \"true\" from stdin where not ( (1+4) == 2 ) and (not(1 > (5*6)));" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("true"));
}

TEST(TestS3SElect, from_stdin)
{
    s3select s3select_syntax;
    const std::string input_query = "select * from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(),
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
}

TEST(TestS3SElect, from_valid_object)
{
    s3select s3select_syntax;
    const std::string input_query = "select * from /objectname;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
}

TEST(TestS3SElect, from_invalid_object)
{
    s3select s3select_syntax;
    const std::string input_query = "select sum(1) from file.txt;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, -1);
    auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res,failure_sign);
}

TEST(TestS3selectFunctions, avg)
{
    s3select s3select_syntax;
    const std::string input_query = "select avg(int(_1)) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("63.5,"));
}

TEST(TestS3selectFunctions, avgzero)
{
    s3select s3select_syntax;
    const std::string input_query = "select avg(int(_1)) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 0;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, -1);
    ASSERT_EQ(s3select_result, std::string(""));
}

TEST(TestS3selectFunctions, floatavg)
{
    s3select s3select_syntax;
    const std::string input_query = "select avg(float(_1)) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 128;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("63.5,"));
}

TEST(TestS3selectFunctions, charlength)
{
    s3select s3select_syntax;
    const std::string input_query = "select char_length(\"abcde\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("5,\n"));
}

TEST(TestS3selectFunctions, characterlength)
{
    s3select s3select_syntax;
    const std::string input_query = "select character_length(\"abcde\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("5,\n"));
}

TEST(TestS3selectFunctions, emptystring)
{
    s3select s3select_syntax;
    const std::string input_query = "select char_length(\"\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("0,\n"));
}

TEST(TestS3selectFunctions, lower)
{
    s3select s3select_syntax;
    const std::string input_query = "select lower(\"ABcD12#$e\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("abcd12#$e,\n"));
}

TEST(TestS3selectFunctions, upper)
{
    s3select s3select_syntax;
    const std::string input_query = "select upper(\"abCD12#$e\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("ABCD12#$E,\n"));
  }

TEST(TestS3selectFunctions, mod)
{
    s3select s3select_syntax;
    const std::string input_query = "select 5%2 from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("1,\n"));
}

TEST(TestS3selectFunctions, modzero)
{
    s3select s3select_syntax;
    const std::string input_query = "select 0%2 from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("0,\n"));
  }

TEST(TestS3selectFunctions, nullif)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(5,3) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("5,\n"));
}

TEST(TestS3selectFunctions, nullifeq)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(5,5) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("null,\n"));
} 

TEST(TestS3selectFunctions, nullifnull)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(null,null) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("null,\n"));
} 

TEST(TestS3selectFunctions, nullifintnull)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(7, null) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("7,\n"));
} 

TEST(TestS3selectFunctions, nullifintstring)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(5, \"hello\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("5,\n"));
} 

TEST(TestS3selectFunctions, nullifstring)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(\"james\",\"bond\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("james,\n"));
} 

TEST(TestS3selectFunctions, nullifeqstring)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(\"redhat\",\"redhat\") from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("null,\n"));
} 

TEST(TestS3selectFunctions, nullifnumericeq)
{
    s3select s3select_syntax;
    const std::string input_query = "select nullif(1, 1.0) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("null,\n"));
} 

TEST(TestS3selectFunctions, nulladdition)
{
    s3select s3select_syntax;
    const std::string input_query = "select 1 + null from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("null,\n"));
} 

TEST(TestS3selectFunctions, isnull)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where nullif(1,1) is null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnullnot)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not nullif(1,2) is null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, isnull1)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where 7 + null is null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull2)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where null + 7 is null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull3)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (null > 1) is null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull4)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (1 <= null) is null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull5)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (null > 2 and 1 == 0) is not null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull6)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (null>2 and 2>1) is  null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull7)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (null>2 or null<=3) is  null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull8)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (5<4 or null<=3) is  null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, isnull9)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (null<=3 or 5<3) is  null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}  

TEST(TestS3selectFunctions, isnull10)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (null<=3 or 5>3) ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}  

TEST(TestS3selectFunctions, nullnot)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not (null>0 and 7<3) ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}  

TEST(TestS3selectFunctions, nullnot1)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not  (null>0 or 4>3) and (7<1) ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}  

TEST(TestS3selectFunctions, isnull11)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where (5>3 or null<1) ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}  

TEST(TestS3selectFunctions, likeop)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"qwertyabcde\" like \"%abcde\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeopfalse)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not  \"qwertybcde\" like \"%abcde\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop1)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"qwertyabcdeqwerty\" like \"%abcde%\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop1false)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not \"qwertyabcdqwerty\" like \"%abcde%\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop2)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"abcdeqwerty\" like \"abcde%\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop2false)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not  \"abdeqwerty\" like \"abcde%\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop6)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"abqwertyde\" like \"ab%de\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop3false)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not \"aabcde\" like \"_bcde\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop3mix)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where  \"aabbccdef\" like \"_ab%\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop4mix)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"aabbccdef\" like \"%de_\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop4)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"abcde\" like \"abc_e\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop4false)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not  \"abcccddyddyde\" like \"abc_e\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop5)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"ebcde\" like \"[d-f]bcde\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop5false)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not  \"abcde\" like \"[d-f]bcde\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop5not)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"abcde\" like \"[^d-f]bcde\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop7)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"qwertyabcde\" like \"%%%%abcde\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop8beginning)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"abcde\" like \"[abc]%\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, likeop8false)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not \"dabc\" like \"[abc]%\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, likeop8end)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"xyza\" like \"%[abc]\";" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, inoperator)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"a\" in (\"b\", \"a\");" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, inoperatorfalse)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where not \"a\" in (\"b\", \"c\");" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, inoperatormore)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where \"a\" in (\"b\", \"a\", \"d\", \"e\", \"f\");" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, inoperatormixtype)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where 10 in (5.0*2.0, 12+1, 9+1.2, 22/2, 12-3);" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, mix)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where   \"abcde\" like \"abc_e\" and 10 in (5.0*2.0, 12+1) and nullif(2,2) is null;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
}

TEST(TestS3selectFunctions, case_when_then_else)
{
    s3select s3select_syntax;
    const std::string input_query = "select  case when (1+1+1*1==(2+1)*3)  then \"case_1_1\" \
              when ((4*3)==(12)) then \"case_1_2\" else \"case_else_1\" end , \
               case when 1+1*7==(2+1)*3  then \"case_2_1\" \
              when ((4*3)==(12)+1) then \"case_2_2\" else \"case_else_2\" end from stdin where (3*3==9);" ;

    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("case_1_2,case_else_2,\n"));
}

TEST(TestS3selectFunctions, simple_case_when)
{
    s3select s3select_syntax;

    const std::string input_query = "select  case 2+1 when (3+4) then \"case_1_1\" when 3 then \"case_3\" else \"case_else_1\" end from stdin;";

    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("case_3,\n"));
}

TEST(TestS3selectFunctions, nested_case)
{
    s3select s3select_syntax;

    const std::string input_query = "select case when ((3+4) == (7 *1)) then \"case_1_1\" else \"case_2_2\" end, case 1+3 when 2+3 then \"case_1_2\" else \"case_2_1\"  end from stdin where (3*3 == 9);";

    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("case_1_1,case_2_1,\n"));
}

TEST(TestS3selectFunctions, case_when_condition_multiplerows)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select case when cast(_3 as int)>99 and cast(_3 as int)<1000 then \"case_1_1\" else \"case_2_2\" end from s3object;";

  std::string s3select_result = run_s3select(input_query,input);

  const std::string input_query_2 = "select case when char_length(_3)==3 then \"case_1_1\" else \"case_2_2\" end from s3object;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result,s3select_result_2);
}

TEST(TestS3selectFunctions, case_value_multiplerows)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select case cast(_1 as int) when cast(_2 as int) then \"case_1_1\" else \"case_2_2\" end from s3object;";

  std::string s3select_result = run_s3select(input_query,input);

  const std::string input_query_2 = "select case when cast(_1 as int) == cast(_2 as int) then \"case_1_1\" else \"case_2_2\" end from s3object;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result,s3select_result_2);
}

TEST(TestS3selectFunctions, substr11)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"01234567890\",2*0+1,1.53*0+3) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("012,\n"));
} 

TEST(TestS3selectFunctions, substr12)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"01234567890\",2*0+1,1+2.0) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("012,\n"));
} 

TEST(TestS3selectFunctions, substr13)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"01234567890\",2.5*2+1,1+2) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("567,\n"));
} 

TEST(TestS3selectFunctions, substr14)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"123456789\",0) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("123456789,\n"));
} 

TEST(TestS3selectFunctions, substr15)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"123456789\",-4) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("123456789,\n"));
} 

TEST(TestS3selectFunctions, substr16)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"123456789\",0,100) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("123456789,\n"));
} 

TEST(TestS3selectFunctions, substr17)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"12345\",0,5) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("1234,\n"));
} 

TEST(TestS3selectFunctions, substr18)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"12345\",-1,5) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("123,\n"));
} 

TEST(TestS3selectFunctions, substr19)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"123456789\" from 0) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("123456789,\n"));
} 

TEST(TestS3selectFunctions, substr20)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"123456789\" from -4) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("123456789,\n"));
} 

TEST(TestS3selectFunctions, substr21)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(\"123456789\" from 0 for 100) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("123456789,\n"));
} 

TEST(TestS3selectFunctions, substr22)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where 5 == cast(substring(\"523\",1,1) as int);" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 


TEST(TestS3selectFunctions, substr23)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where cast(substring(\"523\",1,1) as int) > cast(substring(\"123\",1,1) as int)  ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 

TEST(TestS3selectFunctions, coalesce)
{
    s3select s3select_syntax;
    const std::string input_query = "select coalesce(5,3) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("5,\n"));
}

TEST(TestS3selectFunctions, coalesceallnull)
{
    s3select s3select_syntax;
    const std::string input_query = "select coalesce(nullif(5,5),nullif(1,1.0)) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("null,\n"));
}

TEST(TestS3selectFunctions, coalesceanull)
{
    s3select s3select_syntax;
    const std::string input_query = "select coalesce(nullif(5,5),nullif(1,1.0),2) from stdin;";
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("2,\n"));
}

TEST(TestS3selectFunctions, coalescewhere)
{
    s3select s3select_syntax;
    const std::string input_query = "select \"true\" from stdin where  coalesce(nullif(7.0,7),nullif(4,4.0),6) == 6;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("true,\n"));
} 


TEST(TestS3selectFunctions, castint)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast(5.123 as int) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("5,\n"));
} 

TEST(TestS3selectFunctions, castfloat)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast(1.234 as float) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("1.234,\n"));
} 

TEST(TestS3selectFunctions, castfloatoperation)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast(1.234 as float) + cast(1.235 as float) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    //ASSERT_EQ(s3select_result, std::string("2.469,\n"));
    ASSERT_EQ(s3select_result,std::string("2.4690000000000003,\n"));
} 

TEST(TestS3selectFunctions, caststring)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast(1234 as string) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("1234,\n"));
} 

TEST(TestS3selectFunctions, caststring1)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast('12hddd' as int) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, -1); 
    ASSERT_EQ(s3select_result, "");
} 

TEST(TestS3selectFunctions, caststring2)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast('124' as int) + 1 from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("125,\n"));
} 

TEST(TestS3selectFunctions, castsubstr)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(cast(cast(\"1234567\" as int) as string),2,2) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("23,\n"));
} 

TEST(TestS3selectFunctions, casttimestamp)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast('2010-01-15T13:30:10Z' as timestamp)  from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("2010-01-15T13:30:10,\n"));
} 

TEST(TestS3selectFunctions, castdateadd)
{
    s3select s3select_syntax;
    const std::string input_query = "select date_add(day, 2, cast('2010-01-15T13:30:10Z' as timestamp)) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("2010-01-17T13:30:10,\n"));
} 

TEST(TestS3selectFunctions, castdatediff)
{
    s3select s3select_syntax;
    const std::string input_query = "select date_diff(year,cast('2010-01-15T13:30:10Z' as timestamp), cast('2020-01-15T13:30:10Z' as timestamp)) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("10,\n"));
} 

TEST(TestS3selectFunctions, trim)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(\"   \twelcome\t   \") from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("\twelcome\t,\n"));
}

TEST(TestS3selectFunctions, trim1)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(\"   foobar   \") from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar,\n"));
} 

TEST(TestS3selectFunctions, trim2)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(trailing from \"   foobar   \") from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("   foobar,\n"));
}

TEST(TestS3selectFunctions, trim3)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(leading from \"   foobar   \") from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar   ,\n"));
}

TEST(TestS3selectFunctions, trim4)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(both from \"   foobar   \") from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar,\n"));
} 

TEST(TestS3selectFunctions, trim5)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(from \"   foobar   \") from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar,\n"));
} 

TEST(TestS3selectFunctions, trim6)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(both \"12\" from  \"1112211foobar22211122\") from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar,\n"));
}

TEST(TestS3selectFunctions, trim7)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(trim(both from '   foobar   '),2,3) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("oob,\n"));
} 

TEST(TestS3selectFunctions, trim8)
{
    s3select s3select_syntax;
    const std::string input_query = "select substring(trim(both '12' from '1112211foobar22211122'),1,6) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar,\n"));
} 

TEST(TestS3selectFunctions, trim9)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast(trim(both \"12\" from \"111221134567822211122\") as int) + 5 from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("345683,\n"));
}

TEST(TestS3selectFunctions, trimefalse)
{
    s3select s3select_syntax;
    const std::string input_query = "select cast(trim(both from \"12\" \"111221134567822211122\") as int) + 5 from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_NE(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_NE(status, 0); 
    ASSERT_EQ(s3select_result, std::string(""));
}

TEST(TestS3selectFunctions, trim10)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(trim(leading from \"   foobar   \")) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar,\n"));
}

TEST(TestS3selectFunctions, trim11)
{
    s3select s3select_syntax;
    const std::string input_query = "select trim(trailing from trim(leading from \"   foobar   \")) from stdin ;" ;
    auto status = s3select_syntax.parse_query(input_query.c_str());
    ASSERT_EQ(status, 0);
    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), 
        false, // dont skip first line 
        false, // dont skip last line
        true   // aggregate call
        ); 
    ASSERT_EQ(status, 0); 
    ASSERT_EQ(s3select_result, std::string("foobar,\n"));
}

TEST(TestS3selectFunctions, nested_call_aggregate_with_non_aggregate )
{
  std::string input;
  size_t size = 128;

  generate_fix_columns_csv(input, size);

  const std::string input_query = "select sum(cast(_1 as int)),max(cast(_3 as int)),substring('abcdefghijklm',(2-1)*3+sum(cast(_1 as int))/sum(cast(_1 as int))+1,(count() + count(0))/count(0)) from stdin;";

  std::string s3select_result = run_s3select(input_query,input);

  ASSERT_EQ(s3select_result,"128,3,ef,");
}

TEST(TestS3selectFunctions, cast_1 )
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select count(*) from s3object where cast(_3 as int)>99 and cast(_3 as int)<1000;";

  std::string s3select_result = run_s3select(input_query,input);

  const std::string input_query_2 = "select count(*) from s3object where char_length(_3)==3;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result,s3select_result_2);
}

TEST(TestS3selectFunctions, null_column )
{
  std::string input;
  size_t size = 10000;

  generate_rand_columns_csv_with_null(input, size);

  const std::string input_query = "select count(*) from s3object where _3 is null;";

  std::string s3select_result = run_s3select(input_query,input);

  ASSERT_NE(s3select_result,failure_sign);

  const std::string input_query_2 = "select count(*) from s3object where nullif(_3,null) is null;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(s3select_result,s3select_result_2);
}

TEST(TestS3selectFunctions, count_operation)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select count(*) from s3object;";

  std::string s3select_result = run_s3select(input_query,input);

  ASSERT_NE(s3select_result,failure_sign);

  ASSERT_EQ(s3select_result,"10000,");
}

TEST(TestS3selectFunctions, nullif_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(*) from s3object where nullif(_1,_2) is null;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(*) from s3object where _1 == _2;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(*) from s3object where not nullif(_1,_2) is null;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(*) from s3object where _1 != _2;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select count(*) from s3object where nullif(_1,_2) == _1 ;";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select count(*) from s3object where _1 != _2;";

  std::string s3select_result_6 = run_s3select(input_query_6,input);

  ASSERT_EQ(s3select_result_5, s3select_result_6); 
}

TEST(TestS3selectFunctions, lower_upper_expressions)
{
  std::string input;
  size_t size = 1;
  generate_csv(input, size);
  const std::string input_query_1 = "select lower(\"AB12cd$$\") from s3object;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  ASSERT_EQ(s3select_result_1, "ab12cd$$,\n");

  const std::string input_query_2 = "select upper(\"ab12CD$$\") from s3object;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(s3select_result_2, "AB12CD$$,\n");
}

TEST(TestS3selectFunctions, in_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select int(_1) from s3object where int(_1) in(1);";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select int(_1) from s3object where int(_1) == 1;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select int(_1) from s3object where int(_1) in(1,0);";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select int(_1) from s3object where int(_1) == 1 or int(_1) == 0;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select int(_2) from s3object where int(_2) in(1,0,2);";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select int(_2) from s3object where int(_2) == 1 or int(_2) == 0 or int(_2) == 2;";

  std::string s3select_result_6 = run_s3select(input_query_6,input);

  ASSERT_EQ(s3select_result_5, s3select_result_6);

  const std::string input_query_7 = "select int(_2) from s3object where int(_2)*2 in(int(_3)*2,int(_4)*3,int(_5)*5);";

  std::string s3select_result_7 = run_s3select(input_query_7,input);

  ASSERT_NE(s3select_result_7,failure_sign);

  const std::string input_query_8 = "select int(_2) from s3object where int(_2)*2 == int(_3)*2 or int(_2)*2 == int(_4)*3 or int(_2)*2 == int(_5)*5;";

  std::string s3select_result_8 = run_s3select(input_query_8,input);

  ASSERT_EQ(s3select_result_7, s3select_result_8);

  const std::string input_query_9 = "select int(_1) from s3object where character_length(_1) == 2 and substring(_1,2,1) in (\"3\");";

  std::string s3select_result_9 = run_s3select(input_query_9,input);

  ASSERT_NE(s3select_result_9,failure_sign);

  const std::string input_query_10 = "select int(_1) from s3object where _1 like \"_3\";";

  std::string s3select_result_10 = run_s3select(input_query_10,input);

  ASSERT_EQ(s3select_result_9, s3select_result_10);
}

TEST(TestS3selectFunctions, test_coalesce_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(*) from s3object where char_length(_3)>2 and char_length(_4)>2 and cast(substring(_3,1,2) as int) == cast(substring(_4,1,2) as int);";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(*) from s3object where cast(_3 as int)>99 and cast(_4 as int)>99 and coalesce(nullif(cast(substring(_3,1,2) as int),cast(substring(_4,1,2) as int)),7) == 7;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select coalesce(nullif(_5,_5),nullif(_1,_1),_2) from s3object;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select coalesce(_2) from s3object;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);
}

TEST(TestS3selectFunctions, test_cast_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(*) from s3object where cast(_3 as int)>999;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(*) from s3object where char_length(_3)>3;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(*) from s3object where char_length(_3)==3;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(*) from s3object where cast(_3 as int)>99 and cast(_3 as int)<1000;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);
}

TEST(TestS3selectFunctions, test_version)
{
  std::string input;
  size_t size = 1;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select version() from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  ASSERT_EQ(s3select_result_1, "41.a,\n");
}

TEST(TestS3selectFunctions, test_date_time_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv_datetime(input, size);
  const std::string input_query_1 = "select count(*) from s3object where extract(year from to_timestamp(_1)) > 1950 and extract(year from to_timestamp(_1)) < 1960;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(*) from s3object where int(substring(_1,1,4))>1950 and int(substring(_1,1,4))<1960;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(*) from s3object where date_diff(month,to_timestamp(_1),date_add(month,2,to_timestamp(_1)) ) == 2;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(*) from s3object;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_NE(s3select_result_4,failure_sign);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select count(0) from  stdin where date_diff(year,to_timestamp(_1),date_add(day, 366 ,to_timestamp(_1))) == 1;";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_EQ(s3select_result_5, s3select_result_4);

  const std::string input_query_6 = "select count(0) from  stdin where date_diff(hour,utcnow(),date_add(day,1,utcnow())) == 24;";

  std::string s3select_result_6 = run_s3select(input_query_6,input);

  ASSERT_EQ(s3select_result_6, s3select_result_4);
}

TEST(TestS3selectFunctions, test_like_expressions)
{
  std::string input, input1;
  size_t size = 10000;
  generate_csv(input, size);
  const std::string input_query_1 = "select count(*) from stdin where _4 like \"%ar\";";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(*) from stdin where substring(_4,char_length(_4),1) == \"r\" and substring(_4,char_length(_4)-1,1) == \"a\";";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  generate_csv_like(input1, size);

  const std::string input_query_3 = "select count(*) from stdin where _1 like \"%aeio%\";";

  std::string s3select_result_3 = run_s3select(input_query_3,input1);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(*) from stdin where substring(_1,4,4) == \"aeio\";";

  std::string s3select_result_4 = run_s3select(input_query_4,input1);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select count(*) from stdin where _1 like \"%r[r-s]\";";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select count(*) from stdin where substring(_1,char_length(_1),1) between \"r\" and \"s\" and substring(_1,char_length(_1)-1,1) == \"r\";";

  std::string s3select_result_6 = run_s3select(input_query_6,input);

  ASSERT_EQ(s3select_result_5, s3select_result_6);

  const std::string input_query_7 = "select count(*) from stdin where _1 like \"%br_\";";

  std::string s3select_result_7 = run_s3select(input_query_7,input);

  ASSERT_NE(s3select_result_7,failure_sign);

  const std::string input_query_8 = "select count(*) from stdin where substring(_1,char_length(_1)-1,1) == \"r\" and substring(_1,char_length(_1)-2,1) == \"b\";";

  std::string s3select_result_8 = run_s3select(input_query_8,input);

  ASSERT_EQ(s3select_result_7, s3select_result_8);

  const std::string input_query_9 = "select count(*) from stdin where _1 like \"f%s\";";

  std::string s3select_result_9 = run_s3select(input_query_9,input);

  ASSERT_NE(s3select_result_9,failure_sign);

  const std::string input_query_10 = "select count(*) from stdin where substring(_1,char_length(_1),1) == \"s\" and substring(_1,1,1) == \"f\";";

  std::string s3select_result_10 = run_s3select(input_query_10,input);

  ASSERT_EQ(s3select_result_9, s3select_result_10);
}

TEST(TestS3selectFunctions, test_when_then_else_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select case when cast(_1 as int)>100 and cast(_1 as int)<200 then \"a\" when cast(_1 as int)>200 and cast(_1 as int)<300 then \"b\" else \"c\" end from s3object;";

  std::string s3select_result_1 = run_s3select(input_query_1,input); 

  ASSERT_NE(s3select_result_1,failure_sign);

  int count1 = std::count(s3select_result_1.begin(), s3select_result_1.end(),'a') ; 
  int count2 = std::count(s3select_result_1.begin(), s3select_result_1.end(), 'b'); 
  int count3 = std::count(s3select_result_1.begin(), s3select_result_1.end(), 'c'); 

  const std::string input_query_2 = "select count(*) from s3object where  cast(_1 as int)>100 and cast(_1 as int)<200;";

  std::string s3select_result_2 = run_s3select(input_query_2,input); 

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(stoi(s3select_result_2), count1);

  const std::string input_query_3 = "select count(*) from s3object where  cast(_1 as int)>200 and cast(_1 as int)<300;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  ASSERT_EQ(stoi(s3select_result_3), count2);

  const std::string input_query_4 = "select count(*) from s3object where  cast(_1 as int)<=100 or cast(_1 as int)>=300 or cast(_1 as int)==200;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_NE(s3select_result_4,failure_sign);

  ASSERT_EQ(stoi(s3select_result_4), count3);
}

TEST(TestS3selectFunctions, test_case_value_when_then_else_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select case cast(_1 as int) + 1 when 2 then \"a\" when 3  then \"b\" else \"c\" end from s3object;";

  std::string s3select_result_1 = run_s3select(input_query_1,input); 

  ASSERT_NE(s3select_result_1,failure_sign);

  int count1 = std::count(s3select_result_1.begin(), s3select_result_1.end(),'a') ; 
  int count2 = std::count(s3select_result_1.begin(), s3select_result_1.end(), 'b'); 
  int count3 = std::count(s3select_result_1.begin(), s3select_result_1.end(), 'c'); 

  const std::string input_query_2 = "select count(*) from s3object where  cast(_1 as int) + 1 == 2;";

  std::string s3select_result_2 = run_s3select(input_query_2,input); 

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(stoi(s3select_result_2), count1);

  const std::string input_query_3 = "select count(*) from s3object where  cast(_1 as int) + 1 == 3;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  ASSERT_EQ(stoi(s3select_result_3), count2);

  const std::string input_query_4 = "select count(*) from s3object where  cast(_1 as int) + 1 < 2 or cast(_1 as int) + 1 > 3;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_NE(s3select_result_4,failure_sign);

  ASSERT_EQ(stoi(s3select_result_4), count3);
}

TEST(TestS3selectFunctions, test_trim_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_csv_trim(input, size);
  const std::string input_query_1 = "select count(*) from stdin where trim(_1) == \"aeiou\";";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(*) from stdin where substring(_1 from 6 for 5) == \"aeiou\";";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(*) from stdin where trim(both from _1) == \"aeiou\";";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(*) from stdin where substring(_1,6,5) == \"aeiou\";";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);
}