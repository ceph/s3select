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

TEST(TestS3selectFunctions, nonleapyeartimestamp)
{
    // TODO: support formats listed here:
    // https://docs.aws.amazon.com/AmazonS3/latest/dev/s3-glacier-select-sql-reference-date.html#s3-glacier-select-sql-reference-to-timestamp
    const std::string timestamp = "2007-02-29:14:33:01";
    // TODO: out_simestamp should be the same as timestamp
    const std::string out_timestamp = "2007-Feb-29 14:33:01";
    const std::string input_query = "select timestamp(\"" + timestamp + "\") from stdin;" ;
    const auto s3select_res = run_s3select(input_query);
    ASSERT_NE(s3select_res, out_timestamp);
}

TEST(TestS3selectFunctions, leapyeartimestamp)
{
    // TODO: support formats listed here:
    // https://docs.aws.amazon.com/AmazonS3/latest/dev/s3-glacier-select-sql-reference-date.html#s3-glacier-select-sql-reference-to-timestamp
    const std::string timestamp = "2008-02-29:14:33:01";
    // TODO: out_simestamp should be the same as timestamp
    const std::string out_timestamp = "2008-Feb-29 14:33:01";
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

