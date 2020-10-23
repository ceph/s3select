#include "s3select.h"
#include "gtest/gtest.h"
#include <string>
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

  char* prog_c;

  if(fp_c_file)
  {
    prog_c = (char*)malloc(C_FILE_SIZE);

		size_t sz=sprintf(prog_c,"#include <stdio.h>\n \
				#include <float.h>\n \
				int main() \
				{\
				printf(\"%%.*e\\n\",DECIMAL_DIG,(double)(%s));\
				} ", expression);

    int status = fwrite(prog_c, 1, sz, fp_c_file);
    fclose(fp_c_file);
  }

  std::string gcc_and_run_cmd = std::string("gcc ") + c_test_file + " -o " + c_run_file + " -Wall && " + c_run_file;

  FILE* fp_build = popen(gcc_and_run_cmd.c_str(), "r"); //TODO read stderr from pipe

  if(!fp_build)
  {
    return std::string("#ERROR#");
  }

  fgets(result_buff, sizeof(result_buff), fp_build);

  unlink(c_run_file.c_str());
  unlink(c_test_file.c_str());

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

std::string run_s3select(std::string expression)
{
  s3select s3select_syntax;

  s3select_syntax.parse_query(expression.c_str());

  std::string s3select_result;
  s3selectEngine::csv_object  s3_csv_object(&s3select_syntax);
  std::string in = "1,1,1,1\n";

  s3_csv_object.run_s3select_on_object(s3select_result, in.c_str(), in.size(), false, false, true);

  s3select_result = s3select_result.substr(0, s3select_result.find_first_of(","));

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

TEST(TestS3SElect, intnull_compare_operator)
{
  value a10(10), b11(11), c10(10), d, e;
  d.setnull();
  e.setnull();
  ASSERT_EQ( d > b11, false );
  ASSERT_EQ( d >= c10, false );
  ASSERT_EQ( d < a10, false );
  ASSERT_EQ( d <= b11, false );
  ASSERT_EQ( d != a10, true );
  ASSERT_EQ( d != e, true );
  ASSERT_EQ( d == a10, false );
}

TEST(TestS3SElect, floatnull_compare_operator)
{
  value a10(10.1), b11(11.2), c10(10.1), d, e;
  d.setnull();
  e.setnull();
  ASSERT_EQ( d > b11, false );
  ASSERT_EQ( d >= c10, false );
  ASSERT_EQ( d < a10, false );
  ASSERT_EQ( d <= b11, false );
  ASSERT_EQ( d != a10, true );
  ASSERT_EQ( d != e, true );
  ASSERT_EQ( d == a10, false );
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
  ASSERT_EQ((a + d).to_string(), "nan" );
  ASSERT_EQ((a - d).to_string(), "nan" );
  ASSERT_EQ((a * d).to_string(), "nan" );
  ASSERT_EQ((a / d).to_string(), "nan" ); 
  ASSERT_EQ((a / e).to_string(), "nan" ); 
  ASSERT_EQ((d + a).to_string(), "nan" );
  ASSERT_EQ((d - a).to_string(), "nan" );
  ASSERT_EQ((d * a).to_string(), "nan" );
  ASSERT_EQ((d / a).to_string(), "nan" ); 
  ASSERT_EQ((e / a).to_string(), "nan" );
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
    // TODO: support formats listed here:
    // https://docs.aws.amazon.com/AmazonS3/latest/dev/s3-glacier-select-sql-reference-date.html#s3-glacier-select-sql-reference-to-timestamp
    const std::string timestamp = "2007-02-23:14:33:01";
    // TODO: out_simestamp should be the same as timestamp
    const std::string out_timestamp = "2007-Feb-23 14:33:01";
    const std::string input_query = "select timestamp(\"" + timestamp + "\") from stdin;" ;
	  const auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, out_timestamp);
}

TEST(TestS3selectFunctions, utcnow)
{
    const boost::posix_time::ptime now(boost::posix_time::second_clock::universal_time());
    const std::string input_query = "select utcnow() from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    const boost::posix_time::ptime res_now;
    ASSERT_EQ(s3select_res, boost::posix_time::to_simple_string(now));
}

TEST(TestS3selectFunctions, add)
{
    const std::string input_query = "select add(-5, 0.5) from stdin;" ;
	  auto s3select_res = run_s3select(input_query);
    ASSERT_EQ(s3select_res, std::string("-4.5"));
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
    ASSERT_EQ(s3select_res, "");
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
    ASSERT_EQ(s3select_result, std::string("nan,\n"));
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

TEST(TestS3selectFunctions, case_when_than_else)
{
    s3select s3select_syntax;
    const std::string input_query = "select  case when (1+1+1*1==(2+1)*3)  than \"case_1_1\" \
              when ((4*3)==(12)) than \"case_1_2\" else \"case_else_1\" end , \
               case when 1+1*7==(2+1)*3  than \"case_2_1\" \
              when ((4*3)==(12)+1) than \"case_2_2\" else \"case_else_2\" end from stdin where (3*3==9);" ;

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

TEST(TestS3selectFunctions, substr11)
{
    s3select s3select_syntax;
    const std::string input_query = "select substr(\"01234567890\",2*0+1,1.53*0+3) from stdin ;" ;
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
    const std::string input_query = "select substr(\"01234567890\",2*0+1,1+2.0) from stdin ;" ;
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
    const std::string input_query = "select substr(\"01234567890\",2.5*2+1,1+2) from stdin ;" ;
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
    ASSERT_EQ(status, 0); 
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
    const std::string input_query = "select substr(cast(cast(\"1234567\" as int) as string),2,2) from stdin ;" ;
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
    const std::string input_query = "select cast('2010-01-15:13:30:10' as timestamp)  from stdin ;" ;
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
    ASSERT_EQ(s3select_result, std::string("2010-Jan-15 13:30:10,\n"));
} 

TEST(TestS3selectFunctions, castdateadd)
{
    s3select s3select_syntax;
    const std::string input_query = "select dateadd('day',2,cast('2010-01-15:13:30:10' as timestamp)) from stdin ;" ;
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
    ASSERT_EQ(s3select_result, std::string("2010-Jan-17 13:30:10,\n"));
} 

TEST(TestS3selectFunctions, castdatediff)
{
    s3select s3select_syntax;
    const std::string input_query = "select datediff('year',cast('2010-01-15:13:30:10' as timestamp), cast('2020-01-15:13:30:10' as timestamp)) from stdin ;" ;
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





