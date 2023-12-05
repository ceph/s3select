#include "s3select_test.h"

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
  const char *cnull = "null";
  value a(7), d, e(0);
  d.setnull();

  ASSERT_EQ(*(a - d).to_string(), *cnull );
  ASSERT_EQ(*(a * d).to_string(), *cnull );
  ASSERT_EQ(*(a / d).to_string(), *cnull ); 
  ASSERT_EQ(*(a / e).to_string(), *cnull ); 
  ASSERT_EQ(*(d + a).to_string(), *cnull );
  ASSERT_EQ(*(d - a).to_string(), *cnull );
  ASSERT_EQ(*(d * a).to_string(), *cnull );
  ASSERT_EQ(*(d / a).to_string(), *cnull ); 
  ASSERT_EQ(*(e / a).to_string(), *cnull );
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

TEST(TestS3selectFunctions, to_timestamp)
{
    std::string timestamp = "2007T";
    std::string out_timestamp = "2007-01-01T00:00:00+00:00";
    std::string input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T";
    out_timestamp = "2007-09-17T00:00:00+00:00";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56Z";
    out_timestamp = "2007-09-17T17:56:00Z";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05Z";
    out_timestamp = "2007-09-17T17:56:05Z";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05.234Z";
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        out_timestamp = "2007-09-17T17:56:05.234000000Z";
    #else
        out_timestamp = "2007-09-17T17:56:05.234000Z";
    #endif
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56+12:08";
    out_timestamp = "2007-09-17T17:56:00+12:08";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05-05:30";
    out_timestamp = "2007-09-17T17:56:05-05:30";
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05.234+02:44";
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        out_timestamp = "2007-09-17T17:56:05.234000000+02:44";
    #else
        out_timestamp = "2007-09-17T17:56:05.234000+02:44";
    #endif
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    timestamp = "2007-09-17T17:56:05.00234+02:44";
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        out_timestamp = "2007-09-17T17:56:05.002340000+02:44";
    #else
        out_timestamp = "2007-09-17T17:56:05.002340+02:44";
    #endif
    input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, out_timestamp);

    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        timestamp = "2007-09-17T17:56:05.012345678-00:45";
        out_timestamp = "2007-09-17T17:56:05.012345678-00:45";
        input_query = "select to_timestamp(\'" + timestamp + "\') from stdin;" ;
        s3select_res = run_s3select(input_query);
        EXPECT_EQ(s3select_res, out_timestamp);
    #endif

}

TEST(TestS3selectFunctions, date_diff)
{
    std::string input_query = "select date_diff(year, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T19:30:05.234Z\')) from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-1");

    input_query = "select date_diff(month, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-23");

    input_query = "select date_diff(day, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-730");

    input_query = "select date_diff(hour, to_timestamp(\'2007-09-17T17:56:06.234Z\'), to_timestamp(\'2009-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "17545");

    input_query = "select date_diff(hour, to_timestamp(\'2009-09-17T19:30:05.234Z\'), to_timestamp(\'2007-09-17T17:56:06.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-17545");

    input_query = "select date_diff(minute, to_timestamp(\'2007-09-17T17:56:06.234Z\'), to_timestamp(\'2009-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "1052733");

    input_query = "select date_diff(minute, to_timestamp(\'2009-09-17T19:30:05.234Z\'), to_timestamp(\'2007-09-17T17:56:06.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-1052733");

    input_query = "select date_diff(second, to_timestamp(\'2009-09-17T17:56:06.234Z\'), to_timestamp(\'2009-09-17T19:30:05.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "5639");

    input_query = "select date_diff(hour, to_timestamp(\'2007-09-17T17:56:06.234-03:45\'), to_timestamp(\'2007-09-17T17:56:06.234+13:30\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-17");

    input_query = "select date_diff(hour, to_timestamp(\'2007-09-17T17:56:06.234+03:45\'), to_timestamp(\'2007-09-17T17:56:06.234+13:30\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-9");

    input_query = "select date_diff(hour, to_timestamp(\'2007-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T17:56:06.234+13:30\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-13");

    input_query = "select date_diff(hour, to_timestamp(\'2007-09-17T17:56:06.234+14:00\'), to_timestamp(\'2007-09-17T17:56:06.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "14");

    input_query = "select date_diff(minute, to_timestamp(\'2007-09-17T17:56:06.234-03:45\'), to_timestamp(\'2007-09-17T17:56:06.234+13:30\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-1035");

    input_query = "select date_diff(minute, to_timestamp(\'2007-09-17T17:56:06.234+03:45\'), to_timestamp(\'2007-09-17T17:56:06.234+13:30\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-585");

    input_query = "select date_diff(minute, to_timestamp(\'2007-09-17T17:56:06.234Z\'), to_timestamp(\'2007-09-17T17:56:06.234+13:30\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-810");

    input_query = "select date_diff(minute, to_timestamp(\'2007-09-17T17:56:06.234+14:00\'), to_timestamp(\'2007-09-17T17:56:06.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "840");

    input_query = "select date_diff(hour, to_timestamp(\'2007-09-17T17:56:06.234+14:00\'), to_timestamp(\'2007-09-17T03:56:06.234Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "0");
}

TEST(TestS3selectFunctions, date_add)
{
    std::string input_query = "select date_add(year, 2, to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        std::string expected_res = "2011-09-17T17:56:06.234567000Z";
    #else
        std::string expected_res = "2011-09-17T17:56:06.234567Z";
    #endif
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, -5, to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        expected_res = "2009-04-17T17:56:06.234567000Z";
    #else
        expected_res = "2009-04-17T17:56:06.234567Z";
    #endif
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(day, 3, to_timestamp(\'2009-09-17T17:56:06.234567-09:15\')) from stdin;" ;
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        expected_res = "2009-09-20T17:56:06.234567000-09:15";
    #else
        expected_res = "2009-09-20T17:56:06.234567-09:15";
    #endif
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(hour, 1, to_timestamp(\'2007-09-17T17:56:06.234567Z\')) from stdin;" ;
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        expected_res = "2007-09-17T18:56:06.234567000Z";
    #else
        expected_res = "2007-09-17T18:56:06.234567Z";
    #endif
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(minute, 14, to_timestamp(\'2007-09-17T17:56:06.234567+11:00\')) from stdin;" ;
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        expected_res = "2007-09-17T18:10:06.234567000+11:00";
    #else
        expected_res = "2007-09-17T18:10:06.234567+11:00";
    #endif
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(second, -26, to_timestamp(\'2009-09-17T17:56:06.234567-00:30\')) from stdin;" ;
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
        expected_res = "2009-09-17T17:55:40.234567000-00:30";
    #else
        expected_res = "2009-09-17T17:55:40.234567-00:30";
    #endif
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, 1,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2007-10-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, 3,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2007-12-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, 4,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2008-01-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, 15,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2008-12-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, -1,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2007-08-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, -8,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2007-01-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, -9,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2006-12-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, -10,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2006-11-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);

    input_query = "select date_add(month, -15,to_timestamp(\'2007-09-17T17:57:06Z\')) from stdin;" ;
    expected_res = "2006-06-17T17:57:06Z";
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, expected_res);
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
    EXPECT_EQ(s3select_res, "17");

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

    input_query = "select extract(timezone_hour from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "0");

    input_query = "select extract(timezone_hour from to_timestamp(\'2009-09-17T17:56:06.234567-07:45\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-7");

    input_query = "select extract(timezone_hour from to_timestamp(\'2009-09-17T17:56:06.234567+07:45\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "7");

    input_query = "select extract(timezone_minute from to_timestamp(\'2009-09-17T17:56:06.234567Z\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "0");

    input_query = "select extract(timezone_minute from to_timestamp(\'2009-09-17T17:56:06.234567-07:45\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-45");

    input_query = "select extract(timezone_minute from to_timestamp(\'2009-09-17T17:56:06.234567+07:45\')) from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "45");
}

TEST(TestS3selectFunctions, to_string)
{
    std::string input_query = "select to_string(to_timestamp(\'2009-09-17T17:56:06.234567Z\'), \'yyyyMMdd-H:m:s\') from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "20090917-17:56:6");

    input_query = "select to_string(to_timestamp(\'2009-03-17T17:56:06.234567Z\'), \'yydaMMMM h m s.n\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "0917PMMarch 5 56 6.234567000");

    input_query = "select to_string(to_timestamp(\'2009-03-07T01:08:06.234567Z\'), \'yyyyyy yyyy yyy yy y MMMMM MMMM MMM MM M dd dTHH H hh h : mm m ss s SSSSSSSSSS SSSSSS SSS SS S n - a X XX XXX XXXX XXXXX x xx xxx xxxx xxxxx\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "002009 2009 2009 09 2009 M March Mar 03 3 07 7T01 1 01 1 : 08 8 06 6 2345670000 234567 234 23 2 234567000 - AM Z Z Z Z Z +00 +0000 +00:00 +0000 +00:00");

    input_query = "select to_string(to_timestamp(\'2009-03-07T01:08:06.234567-04:25\'), \'X XX XXX XXXX XXXXX x xx xxx xxxx xxxxx\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "-0425 -0425 -04:25 -0425 -04:25 -0425 -0425 -04:25 -0425 -04:25");

    input_query = "select to_string(to_timestamp(\'2009-03-07T01:08:06.234567+12:05\'), \'X XX XXX XXXX XXXXX x xx xxx xxxx xxxxx\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "+1205 +1205 +12:05 +1205 +12:05 +1205 +1205 +12:05 +1205 +12:05");

    input_query = "select to_string(to_timestamp(\'2009-03-07T01:08:06.2345+00:00\'), \'n SSS SSSSSS SSSSSSSSS SSSSSSSSSSS\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "234500000 234 234500 234500000 23450000000");

    input_query = "select to_string(to_timestamp(\'2009-03-07T01:08:06.002345Z\'), \'n SSS SSSSSS SSSSSSSSS SSSSSSSSSSS\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "2345000 002 002345 002345000 00234500000");

    input_query = "select to_string(to_timestamp(\'2009-03-07T01:08:06Z\'), \'n SSS SSSSSS SSSSSSSSS SSSSSSSSSSS\') from stdin;" ;
    s3select_res = run_s3select(input_query);
    EXPECT_EQ(s3select_res, "0 000 000000 000000000 00000000000");
}

TEST(TestS3selectFunctions, utcnow)
{
    const boost::posix_time::ptime now(boost::posix_time::second_clock::universal_time());
    const std::string input_query = "select utcnow() from stdin;" ;
    auto s3select_res = run_s3select(input_query);
    const boost::posix_time::ptime res_now;
    ASSERT_EQ(s3select_res, boost::posix_time::to_iso_extended_string(now) + "+00:00");
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

void generate_csv_escape(std::string& out, size_t size) {
  // schema is: int, float, string, string
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << "_ar" << "," << "aeio_" << "," << "foo"+std::to_string(i) << "," << std::to_string(i)+"bar" << std::endl;
  }
  out = ss.str();
}

void generate_columns_csv(std::string& out, size_t size) {
  std::stringstream ss;

  for (auto i = 0U; i < size; ++i) {
    ss << i << "," << i+1 << "," << i << "," << i << "," << i << "," << i << "," << i << "," << i << "," << i << "," << i << std::endl;
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
  auto r = [](){ int x=rand()%1000;if (x<500) return std::string("hai"); else return std::string("fooaeioubrs");};
  for (auto i = 0U; i < size; ++i) {
    ss << r() << "," << std::endl;
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

void generate_rand_csv_datetime_to_string(std::string& out, std::string& result, size_t size, bool const_frmt = true) {
  std::stringstream ss_out, ss_res;
  std::string format = "yyyysMMMMMdddSSSSSSSSSSSMMMM HHa:m -:-";
  std::string months[12] = {"January", "February", "March","April", "May", "June", "July", "August", "September", "October", "November", "December"};
  auto year = [](){return rand()%100 + 1900;};
  auto month = [](){return 1 + rand()%12;};
  auto day = [](){return 1 + rand()%28;};
  auto hours = [](){return rand()%24;};
  auto minutes = [](){return rand()%60;};
  auto seconds = [](){return rand()%60;};
  auto fracation_sec = [](){return rand()%1000000;};

  for (auto i = 0U; i < size; ++i)
  {
    auto yr = year();
    auto mnth = month();
    auto dy = day();
    auto hr = hours();
    auto mint = minutes();
    auto sec = seconds();
    auto frac_sec = fracation_sec();

    if (const_frmt)
    {
      ss_out << yr << "-" << std::setw(2) << std::setfill('0') << mnth << "-" << std::setw(2) << std::setfill('0') << dy << "T" <<std::setw(2) << std::setfill('0') << hr << ":" << std::setw(2) << std::setfill('0') << mint << ":" << std::setw(2) << std::setfill('0') <<sec << "." << frac_sec << "Z" << std::endl;

      ss_res << yr << sec << months[mnth-1].substr(0, 1) << std::setw(2) << std::setfill('0') << dy << dy << frac_sec << std::string(11 - std::to_string(frac_sec).length(), '0') << months[mnth-1] << " " << std::setw(2) << std::setfill('0') << hr << (hr < 12 ? "AM" : "PM") << ":" << mint << " -:-" << std::endl;
    }
    else
    {
      switch(rand()%5)
      {
        case 0:
            format = "yyyysMMMMMdddSSSSSSSSSSSMMMM HHa:m -:-";
            ss_res << yr << sec << months[mnth-1].substr(0, 1) << std::setw(2) << std::setfill('0') << dy << dy << frac_sec << std::string(11 - std::to_string(frac_sec).length(), '0') << months[mnth-1] << " " << std::setw(2) << std::setfill('0') << hr << (hr < 12 ? "AM" : "PM") << ":" << mint << " -:-" << std::endl;
            break;
        case 1:
            format = "aMMhh";
            ss_res << (hr < 12 ? "AM" : "PM") << std::setw(2) << std::setfill('0') << mnth << std::setw(2) << std::setfill('0') << (hr%12 == 0 ? 12 : hr%12) << std::endl;
            break;
        case 2:
            format = "y M d ABCDEF";
            ss_res << yr << " " << mnth << " " << dy << " ABCDEF" << std::endl;
            break;
        case 3:
            format = "W h:MMMM";
            ss_res << "W " << (hr%12 == 0 ? 12 : hr%12) << ":" << months[mnth-1] << std::endl;
            break;
        case 4:
            format = "H:m:s";
            ss_res << hr << ":" << mint << ":" << sec << std::endl;
            break;
      }

      ss_out << yr << "-" << std::setw(2) << std::setfill('0') << mnth << "-" << std::setw(2) << std::setfill('0') << dy << "T" <<std::setw(2) << std::setfill('0') << hr << ":" << std::setw(2) << std::setfill('0') << mint << ":" << std::setw(2) << std::setfill('0') <<sec << "." << frac_sec << "Z" << "," << format << std::endl;
    }
  }
  out = ss_out.str();
  result = ss_res.str();
}

TEST(TestS3selectFunctions, sum)
{
  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);
  const std::string input_query_1 = "select sum(int(_1)), sum(float(_2)) from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"8128,8256");
}

TEST(TestS3selectFunctions, between)
{
  std::string input;
  size_t size = 128;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(0) from stdin where int(_1) between int(_2) and int(_3);";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  const std::string input_query_2 = "select count(0) from stdin where int(_1) >= int(_2) and int(_1) <= int(_3);";

  std::string s3select_result_2 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,s3select_result_2);
}


TEST(TestS3selectFunctions, not_between)
{
  std::string input;
  size_t size = 128;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(0) from stdin where int(_1) not  between int(_2) and int(_3);";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  const std::string input_query_2 = "select count(0) from stdin where int(_1) < int(_2) or int(_1) > int(_3);";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1,s3select_result_2);
}

TEST(TestS3selectFunctions, count)
{
  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);
  const std::string input_query_1 = "select count(0) from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"128"); 
}

TEST(TestS3selectFunctions, no_args)
{//note: engine throw an exception(and description), currently it is not catch in this test-app
#if 0
  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);
  std::string input_query_1 = "select min() from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,""); 

  input_query_1 = "select max() from stdin;";

  s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,""); 

  input_query_1 = "select avg() from stdin;";

  s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,""); 

  input_query_1 = "select sum() from stdin;";

  s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,""); 
#endif
}

TEST(TestS3selectFunctions, empty_group_upon_aggtegation)
{

  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);
  std::string input_query_1 = "select min(cast(_1 as int)) from stdin where 1 = 0;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"null"); 

  input_query_1 = "select max(cast(_1 as int)) from stdin where 1 = 0;";

  s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"null"); 

  input_query_1 = "select sum(cast(_1 as int)) from stdin where 1 = 0;";

  s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"null"); 

  input_query_1 = "select avg(cast(_1 as int)) from stdin where 1 = 0;";

  s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"null"); 
}

TEST(TestS3selectFunctions, min)
{
  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);
  const std::string input_query_1 = "select min(int(_1)), min(float(_2)) from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"0,1"); 
}

TEST(TestS3selectFunctions, max)
{
  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);
  const std::string input_query_1 = "select max(int(_1)), max(float(_2)) from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"127,128"); 
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

void test_single_column_single_row(const char* input_query,const char* expected_result,const char * error_description = 0)
{
    s3select s3select_syntax;
    auto status = s3select_syntax.parse_query(input_query);
    if(strcmp(expected_result,"#failure#") == 0 && status != 0)
    {
	ASSERT_TRUE(true);
	return; 
    }
    else if (status != 0)
    {
	EXPECT_TRUE(false) << "fail to parse query: " << input_query;
	return;
    }

    s3selectEngine::csv_object s3_csv_object(&s3select_syntax);
    std::string s3select_result;
    std::string json_result;
    std::string input;
    size_t size = 1;
    generate_csv(input, size);

    std::string js = convert_to_json(input.c_str(), input.size());
    std::string query = input_query;
    const char* json_query = convert_query(query);
    run_json_query(json_query, js, json_result);

#ifdef _ARROW_EXIST
    csv_to_parquet(input);
    std::string parquet_result;
    run_query_on_parquet_file(input_query,PARQUET_FILENAME,parquet_result);
#endif

    s3_csv_object.m_csv_defintion.redundant_column = false; 
    status = s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(),
        false, // dont skip first line
        false, // dont skip last line
        true   // aggregate call
        );

    if(strcmp(expected_result,"#failure#") == 0)
    {
      if (status==0 && s3select_result.compare("#failure#")==0)
      {
	  ASSERT_TRUE(false);
      }
      if(s3_csv_object.get_error_description().find(error_description) == std::string::npos )
      {	
	FAIL() << "getting error: " << s3_csv_object.get_error_description() << " instead of: " << error_description << std::endl;
      }
      return;
    }

    ASSERT_EQ(status, 0);
#ifdef _ARROW_EXIST
    parquet_csv_report_error(parquet_result,s3select_result);
#endif
    json_csv_report_error(json_result, s3select_result);
    ASSERT_EQ(s3select_result, std::string(expected_result));
}

TEST(TestS3selectFunctions, syntax_1)
{
    //where not not (1<11) is not null;  syntax failure ; with parentheses it pass syntax i.e. /not (not (1<11)) is not null;/
    //where not 1<11  is null; syntax failure ; with parentheses it pass syntax i.e. not (1<11) is null;
    //where not (1); AST failure , expression result,any result implictly define true/false result
    //where not (1+1); AST failure
    //where not(not (1<11)) ; OK
    //where (not (1<11)) ; OK
    //where not (1<11) ; OK
  test_single_column_single_row("select count(0) from stdin where not (not (1<11)) is not null;","0");
  test_single_column_single_row("select count(0) from stdin where ((not (1<11)) is not null);","1");
  test_single_column_single_row("select count(0) from stdin where not(not (1<11));","1");
  test_single_column_single_row("select count(0) from stdin where not (1<11);","0");
  test_single_column_single_row("select count(0) from stdin where 1=1 or 2=2 and 4=4 and 2=4;","1");
  test_single_column_single_row("select count(0) from stdin where 2=2 and 4=4 and 2=4 or 1=1;","1");
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
    const std::string input_query = "select \"true\" from stdin where not ( (1+4) = 2 ) and (not(1 > (5*6)));" ;
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
    std::string input_copy = input;
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
  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);
  const std::string input_query_1 = "select avg(int(_1)) from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"63.5");
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
    ASSERT_EQ(status, 0);
    ASSERT_EQ(s3select_result, std::string("null"));
}

TEST(TestS3selectFunctions, floatavg)
{
  std::string input;
  size_t size = 128;
  generate_columns_csv(input, size);

  const std::string input_query_1 = "select avg(float(_1)) from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_EQ(s3select_result_1,"63.5");
}

TEST(TestS3selectFunctions, case_when_condition_multiplerows)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select case when cast(_3 as int)>99 and cast(_3 as int)<1000 then \"case-1-1\" else \"case-2-2\" end from s3object;";

  std::string s3select_result = run_s3select(input_query,input);

  const std::string input_query_2 = "select case when char_length(_3)=3 then \"case-1-1\" else \"case-2-2\" end from s3object;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result,s3select_result_2);
}

TEST(TestS3selectFunctions, case_value_multiplerows)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  std::string input_query = "select case cast(_1 as int) when cast(_2 as int) then \"case-1-1\" else \"case-2-2\" end from s3object;";

  std::string s3select_result = run_s3select(input_query,input);

  const std::string input_query_2 = "select case when cast(_1 as int) = cast(_2 as int) then \"case-1-1\" else \"case-2-2\" end from s3object;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result,s3select_result_2);

  //the following test query, validates correct build of the AST.
  //the query contain various combinations, such as nested case-when-else, aggregation for case-when
  //binary-operation with aggregation function.  
  input_query.assign("select 2+ sum(case when int(_1)>100 then (case when int(_1)>100 then 1 else 0 end) else 0 end) + \
					  sum(case when int(_1)<=100 then (case when int(_1)<=100 then 1 else 0 end) else 0 end) , \
					  count(0) , \
					  sum(case when int(_2)>100 then (case when int(_2)>100 then 1 else 0 end) else 0 end) + \
					  sum(case when int(_2)<=100 then (case when int(_2)<=100 then 1 else 0 end) else 0 end) + 1  from s3object;");

  s3select_result = run_s3select(input_query,input);

  std::string expected_result = std::to_string(size+2) + "," + std::to_string(size) + "," + std::to_string(size+1);
  ASSERT_EQ(s3select_result,expected_result);

  
  //aggregation function on top of nested case-when, case-when statement contains binary operation.
  input_query.assign("select sum(case when int(_2)>100 then (case when int(_2)>100 then 1 else 0 end)*2 else 0 end+1) + \
		  sum(case when int(_2)<=100 then (case when int(_2)<=100 then 1 else 0 end)*2 else 0 end) from s3object;");

  s3select_result = run_s3select(input_query,input);

  expected_result = std::to_string(size*3);
  ASSERT_EQ(s3select_result,expected_result);
}

TEST(TestS3selectFunctions, nested_call_aggregate_with_non_aggregate )
{
  std::string input;
  size_t size = 128;

  generate_fix_columns_csv(input, size);

  const std::string input_query = "select sum(cast(_1 as int)),max(cast(_3 as int)),substring('abcdefghijklm',(2-1)*3+sum(cast(_1 as int))/sum(cast(_1 as int))+1,(count(0) + count(0))/count(0)) from stdin;";

  std::string s3select_result = run_s3select(input_query,input);

  ASSERT_EQ(s3select_result,"128,3,ef");
}

TEST(TestS3selectFunctions, cast_1 )
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select count(0) from s3object where cast(_3 as int)>99 and cast(_3 as int)<1000;";

  std::string s3select_result = run_s3select(input_query,input);

  const std::string input_query_2 = "select count(0) from s3object where char_length(_3)=3;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result,s3select_result_2);
}

TEST(TestS3selectFunctions, null_column )
{
  std::string input;
  size_t size = 10000;

  generate_rand_columns_csv_with_null(input, size);

  const std::string input_query = "select count(0) from s3object where _3 is null;";

  std::string s3select_result = run_s3select(input_query,input);

  ASSERT_NE(s3select_result,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where nullif(_3,null) is null;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(s3select_result,s3select_result_2);
}

TEST(TestS3selectFunctions, count_operation)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select count(0) from s3object;";

  std::string s3select_result = run_s3select(input_query,input);

  ASSERT_NE(s3select_result,failure_sign);

  ASSERT_EQ(s3select_result,"10000");
}

TEST(TestS3selectFunctions, nullif_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(0) from s3object where nullif(_1,_2) is null;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where _1 = _2;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(0) from s3object where not nullif(_1,_2) is null;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from s3object where _1 != _2;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select count(0) from s3object where nullif(_1,_2) = _1 ;";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select count(0) from s3object where _1 != _2;";

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

  ASSERT_EQ(s3select_result_1, "ab12cd$$\n");

  const std::string input_query_2 = "select upper(\"ab12CD$$\") from s3object;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(s3select_result_2, "AB12CD$$\n");
}

TEST(TestS3selectFunctions, in_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select int(_1) from s3object where int(_1) in(1);";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select int(_1) from s3object where int(_1) = 1;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select int(_1) from s3object where int(_1) in(1,0);";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select int(_1) from s3object where int(_1) = 1 or int(_1) = 0;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select int(_2) from s3object where int(_2) in(1,0,2);";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select int(_2) from s3object where int(_2) = 1 or int(_2) = 0 or int(_2) = 2;";

  std::string s3select_result_6 = run_s3select(input_query_6,input);

  ASSERT_EQ(s3select_result_5, s3select_result_6);

  const std::string input_query_7 = "select int(_2) from s3object where int(_2)*2 in(int(_3)*2,int(_4)*3,int(_5)*5);";

  std::string s3select_result_7 = run_s3select(input_query_7,input);

  ASSERT_NE(s3select_result_7,failure_sign);

  const std::string input_query_8 = "select int(_2) from s3object where int(_2)*2 = int(_3)*2 or int(_2)*2 = int(_4)*3 or int(_2)*2 = int(_5)*5;";

  std::string s3select_result_8 = run_s3select(input_query_8,input);

  ASSERT_EQ(s3select_result_7, s3select_result_8);

  const std::string input_query_9 = "select int(_1) from s3object where character_length(_1) = 2 and substring(_1,2,1) in (\"3\");";

  std::string s3select_result_9 = run_s3select(input_query_9,input);

  ASSERT_NE(s3select_result_9,failure_sign);

  const std::string input_query_10 = "select int(_1) from s3object where _1 like \"_3\";";

  const char* json_query_10 = "select int(_1.c1) from s3object[*].root where _1.c1 like \"_3\";";

  std::string s3select_result_10 = run_s3select(input_query_10,input,json_query_10);

  ASSERT_EQ(s3select_result_9, s3select_result_10);
}

TEST(TestS3selectFunctions, test_coalesce_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
    
  const std::string input_query_1 = "select count(0) from s3object where char_length(_3)>2 and char_length(_4)>2 and cast(substring(_3,1,2) as int) = cast(substring(_4,1,2) as int);";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where cast(_3 as int)>99 and cast(_4 as int)>99 and coalesce(nullif(cast(substring(_3,1,2) as int),cast(substring(_4,1,2) as int)),7) = 7;";

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
  const std::string input_query_1 = "select count(0) from s3object where cast(_3 as int)>999;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where char_length(_3)>3;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(0) from s3object where char_length(_3)=3;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from s3object where cast(_3 as int)>99 and cast(_3 as int)<1000;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  //testing the decimal operator for precision setting
  const std::string input_query_5 = "select cast(1.123456789 as decimal(9,1)) from s3object limit 1;";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_EQ(s3select_result_5, "1.123456789\n");
}

TEST(TestS3selectFunctions, test_version)
{
  std::string input;
  size_t size = 1;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select version() from stdin;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  ASSERT_EQ(s3select_result_1, "41.a\n");
}

TEST(TestS3selectFunctions, multirow_datetime_to_string_constant)
{
  std::string input, expected_res;
  std::string format = "yyyysMMMMMdddSSSSSSSSSSSMMMM HHa:m -:-";
  size_t size = 100;

  generate_rand_csv_datetime_to_string(input, expected_res, size);

  const std::string input_query = "select to_string(to_timestamp(_1), \'" + format + "\') from s3object;";
  std::string s3select_result = run_s3select(input_query, input);
  EXPECT_EQ(s3select_result, expected_res);
}

TEST(TestS3selectFunctions, multirow_datetime_to_string_dynamic)
{
  std::string input, expected_res;
  size_t size = 100;

  generate_rand_csv_datetime_to_string(input, expected_res, size, false);

  const std::string input_query = "select to_string(to_timestamp(_1), _2) from s3object;";
  std::string s3select_result = run_s3select(input_query, input);
  EXPECT_EQ(s3select_result, expected_res);
}

TEST(TestS3selectFunctions, backtick_on_timestamp)
{
  const std::string input = "1994-11-21T11:49:23Z\n";
  const std::string input_query = "select count(0) from s3object where cast(_1 as timestamp) = `1994-11-21T11:49:23Z`;";
  std::string s3select_result = run_s3select(input_query, input);
  EXPECT_EQ(s3select_result, "1");
}

TEST(TestS3selectFunctions, test_date_time_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv_datetime(input, size);
  const std::string input_query_1 = "select count(0) from s3object where extract(year from to_timestamp(_1)) > 1950 and extract(year from to_timestamp(_1)) < 1960;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where int(substring(_1,1,4))>1950 and int(substring(_1,1,4))<1960;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(0) from s3object where date_diff(month,to_timestamp(_1),date_add(month,2,to_timestamp(_1)) ) = 2;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from s3object;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_NE(s3select_result_4,failure_sign);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select count(0) from  stdin where date_diff(year,to_timestamp(_1),date_add(day, 366 ,to_timestamp(_1))) = 1;";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_EQ(s3select_result_5, s3select_result_4);

  const std::string input_query_6 = "select count(0) from  stdin where date_diff(hour,utcnow(),date_add(day,1,utcnow())) = 24;";

  std::string s3select_result_6 = run_s3select(input_query_6,input);

  ASSERT_EQ(s3select_result_6, s3select_result_4);

  std::string input_query_7 = "select extract(year from to_timestamp(_1)) from stdin;";
  std::string s3select_result_7 = run_s3select(input_query_7, input);
  ASSERT_NE(s3select_result_7, failure_sign);
  std::string input_query_8 = "select substring(_1, 1, 4) from stdin;";
  std::string s3select_result_8 = run_s3select(input_query_8, input);
  ASSERT_NE(s3select_result_8, failure_sign);
  EXPECT_EQ(s3select_result_7, s3select_result_8);

  std::string input_query_9 = "select to_timestamp(_1) from stdin where extract(month from to_timestamp(_1)) = 5;";
  std::string s3select_result_9 = run_s3select(input_query_9, input);
  ASSERT_NE(s3select_result_9, failure_sign);
  std::string input_query_10 = "select substring(_1, 1, char_length(_1)) from stdin where _1 like \'____-05%\';";
  std::string s3select_result_10 = run_s3select(input_query_10, input);
  ASSERT_NE(s3select_result_10, failure_sign);
  EXPECT_EQ(s3select_result_9, s3select_result_10);

  std::string input_query_11 = "select _1 from stdin where extract(month from to_timestamp(_1)) = 5 or extract(month from to_timestamp(_1)) = 6;";
  std::string s3select_result_11 = run_s3select(input_query_11,input);
  ASSERT_NE(s3select_result_11, failure_sign);
  std::string input_query_12 = "select _1 from stdin where to_string(to_timestamp(_1), 'MMMM') in ('May', 'June');";
  std::string s3select_result_12 = run_s3select(input_query_12,input);
  ASSERT_NE(s3select_result_12, failure_sign);
  EXPECT_EQ(s3select_result_11, s3select_result_12);

  std::string input_query_13 = "select to_string(to_timestamp(_1), 'y,M,H,m') from stdin where cast(to_string(to_timestamp(_1), 'd') as int) >= 1 and cast(to_string(to_timestamp(_1), 'd') as int) <= 10;";
  std::string s3select_result_13 = run_s3select(input_query_13, input);
  ASSERT_NE(s3select_result_13, failure_sign);
  std::string input_query_14 = "select extract(year from to_timestamp(_1)), extract(month from to_timestamp(_1)), extract(hour from to_timestamp(_1)), extract(minute from to_timestamp(_1)) from stdin where  int(substring(_1, 9, 2)) between 1 and 10;";
  std::string s3select_result_14 = run_s3select(input_query_14, input);
  ASSERT_NE(s3select_result_14, failure_sign);
  EXPECT_EQ(s3select_result_13, s3select_result_14);

  std::string input_query_15 = "select to_string(to_timestamp(_1), 'y,M,H,m') from stdin where cast(to_string(to_timestamp(_1), 'd') as int) < 1 or cast(to_string(to_timestamp(_1), 'd') as int) > 10;";
  std::string s3select_result_15 = run_s3select(input_query_15, input);
  ASSERT_NE(s3select_result_15, failure_sign);
  std::string input_query_16 = "select extract(year from to_timestamp(_1)), extract(month from to_timestamp(_1)), extract(hour from to_timestamp(_1)), extract(minute from to_timestamp(_1)) from stdin where  int(substring(_1, 9, 2)) not between 1 and 10;";
  std::string s3select_result_16 = run_s3select(input_query_16, input);
  ASSERT_NE(s3select_result_16, failure_sign);
  EXPECT_EQ(s3select_result_15, s3select_result_16);
}

TEST(TestS3selectFunctions, test_like_expressions)
{
  std::string input, input1;
  size_t size = 10000;
  generate_csv(input, size);
  const std::string input_query_1 = "select count(0) from stdin where _4 like \"%ar\";";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from stdin where substring(_4,char_length(_4),1) = \"r\" and substring(_4,char_length(_4)-1,1) = \"a\";";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  generate_csv_like(input1, size);

  const std::string input_query_3 = "select count(0) from stdin where _1 like \"%aeio%\";";

  std::string s3select_result_3 = run_s3select(input_query_3,input1);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from stdin where substring(_1,4,4) = \"aeio\";";

  std::string s3select_result_4 = run_s3select(input_query_4,input1);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select count(0) from stdin where _1 like \"%r[r-s]\";";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select count(0) from stdin where substring(_1,char_length(_1),1) between \"r\" and \"s\" and substring(_1,char_length(_1)-1,1) = \"r\";";

  std::string s3select_result_6 = run_s3select(input_query_6,input);

  ASSERT_EQ(s3select_result_5, s3select_result_6);

  const std::string input_query_7 = "select count(0) from stdin where _1 like \"%br_\";";

  std::string s3select_result_7 = run_s3select(input_query_7,input);

  ASSERT_NE(s3select_result_7,failure_sign);

  const std::string input_query_8 = "select count(0) from stdin where substring(_1,char_length(_1)-1,1) = \"r\" and substring(_1,char_length(_1)-2,1) = \"b\";";

  std::string s3select_result_8 = run_s3select(input_query_8,input);

  ASSERT_EQ(s3select_result_7, s3select_result_8);

  const std::string input_query_9 = "select count(0) from stdin where _1 like \"f%s\";";

  std::string s3select_result_9 = run_s3select(input_query_9,input);

  ASSERT_NE(s3select_result_9,failure_sign);

  const std::string input_query_10 = "select count(0) from stdin where substring(_1,char_length(_1),1) = \"s\" and substring(_1,1,1) = \"f\";";

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

  const std::string input_query_2 = "select count(0) from s3object where  cast(_1 as int)>100 and cast(_1 as int)<200;";

  std::string s3select_result_2 = run_s3select(input_query_2,input); 

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(stoi(s3select_result_2), count1);

  const std::string input_query_3 = "select count(0) from s3object where  cast(_1 as int)>200 and cast(_1 as int)<300;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  ASSERT_EQ(stoi(s3select_result_3), count2);

  const std::string input_query_4 = "select count(0) from s3object where  cast(_1 as int)<=100 or cast(_1 as int)>=300 or cast(_1 as int)=200;";

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

  const std::string input_query_2 = "select count(0) from s3object where  cast(_1 as int) + 1 = 2;";

  std::string s3select_result_2 = run_s3select(input_query_2,input); 

  ASSERT_NE(s3select_result_2,failure_sign);

  ASSERT_EQ(stoi(s3select_result_2), count1);

  const std::string input_query_3 = "select count(0) from s3object where  cast(_1 as int) + 1 = 3;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  ASSERT_EQ(stoi(s3select_result_3), count2);

  const std::string input_query_4 = "select count(0) from s3object where  cast(_1 as int) + 1 < 2 or cast(_1 as int) + 1 > 3;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_NE(s3select_result_4,failure_sign);

  ASSERT_EQ(stoi(s3select_result_4), count3);
}

TEST(TestS3selectFunctions, test_trim_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_csv_trim(input, size);
  const std::string input_query_1 = "select count(0) from stdin where trim(_1) = \"aeiou\";";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from stdin where substring(_1 from 6 for 5) = \"aeiou\";";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(0) from stdin where trim(both from _1) = \"aeiou\";";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from stdin where substring(_1,6,5) = \"aeiou\";";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);
}

TEST(TestS3selectFunctions, truefalse)
{
  test_single_column_single_row("select 2 from s3object where true or false;","2\n");
  test_single_column_single_row("select 2 from s3object where true or true;","2\n");
  test_single_column_single_row("select 2 from s3object where null or true ;","2\n");
  test_single_column_single_row("select 2 from s3object where true and true;","2\n");
  test_single_column_single_row("select 2 from s3object where true = true ;","2\n");
  test_single_column_single_row("select 2 from stdin where 1<2 = true;","2\n");
  test_single_column_single_row("select 2 from stdin where 1=1 = true;","2\n");
  test_single_column_single_row("select 2 from stdin where false=false = true;","2\n");
  test_single_column_single_row("select 2 from s3object where false or true;","2\n");
  test_single_column_single_row("select true,false from s3object where false = false;","true,false\n");
  test_single_column_single_row("select count(0) from s3object where not (1>2) = true;","1");
  test_single_column_single_row("select count(0) from s3object where not (1>2) = (not false);","1");
  test_single_column_single_row("select (true or false) from s3object;","true\n");
  test_single_column_single_row("select (true and true) from s3object;","true\n");
  test_single_column_single_row("select (true and null) from s3object;","null\n");
  test_single_column_single_row("select (false or false) from s3object;","false\n");
  test_single_column_single_row("select (not true) from s3object;","false\n");
  test_single_column_single_row("select (not 1 > 2) from s3object;","true\n");
  test_single_column_single_row("select (not 1 > 2) as a1,cast(a1 as int)*4 from s3object;","true,4\n");
  test_single_column_single_row("select (1 > 2) from s3object;","false\n");
  test_single_column_single_row("select case when (nullif(3,3) is null) = true then \"case-1-1\" else \"case-2-2\"  end, case when (\"a\" in (\"a\",\"b\")) = true then \"case-3-3\" else \"case-4-4\" end, case when 1>3 then \"case_5_5\" else \"case-6-6\" end from s3object where (3*3 = 9);","case-1-1,case-3-3,case-6-6\n");
}

TEST(TestS3selectFunctions, boolcast)
{
  test_single_column_single_row("select cast(5 as bool) from s3object;","true\n");
  test_single_column_single_row("select cast(0 as bool) from s3object;","false\n");
  test_single_column_single_row("select cast(true as bool) from s3object;","true\n");
  test_single_column_single_row("select cast('a' as bool) from s3object;","false\n");
  test_single_column_single_row("select cast(null as bool) from s3object;","null\n");
}

TEST(TestS3selectFunctions, floatcast)
{
  test_single_column_single_row("select cast('1234a' as float) from s3object;","#failure#","extra characters after the number");
  test_single_column_single_row("select cast('a1234' as float) from s3object;","#failure#","text cannot be converted to a number");
  test_single_column_single_row("select cast('999e+999' as float) from s3object;","#failure#","converted value would fall out of the range of the result type!");
  test_single_column_single_row("select cast(null as float) from s3object;","null\n");
}

TEST(TestS3selectFunctions, intcast)
{
  test_single_column_single_row("select cast('1234a' as int) from s3object;","#failure#","extra characters after the number");
  test_single_column_single_row("select cast('a1234' as int) from s3object;","#failure#","text cannot be converted to a number");
  test_single_column_single_row("select cast('9223372036854775808' as int) from s3object;","#failure#","converted value would fall out of the range of the result type!");
  test_single_column_single_row("select cast('-9223372036854775809' as int) from s3object;","#failure#","converted value would fall out of the range of the result type!");
  test_single_column_single_row("select cast(null as int) from s3object;","null\n");
}

TEST(TestS3selectFunctions, predicate_as_projection_column)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query = "select (int(_2) between int(_3) and int(_4)) from s3object where int(_2)>int(_3) and int(_2)<int(_4);";

  std::string s3select_result = run_s3select(input_query,input);

  ASSERT_NE(s3select_result,failure_sign);

  auto count = std::count(s3select_result.begin(), s3select_result.end(), '0');

  ASSERT_EQ(count,0);

  const std::string input_query_1 = "select (nullif(_1,_2) is null) from s3object where _1 = _2;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  auto count_1 = std::count(s3select_result_1.begin(), s3select_result_1.end(), '0');

  ASSERT_EQ(count_1,0);

  const std::string input_query_2 = "select (nullif(_1,_2) is not null) from s3object where _1 != _2;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_NE(s3select_result_2,failure_sign);

  auto count_2 = std::count(s3select_result_2.begin(), s3select_result_2.end(), '0');

  ASSERT_EQ(count_2,0);

  const std::string input_query_3 = "select (_1 like \"_3\") from s3object where character_length(_1) = 2 and substring(_1,2,1) in (\"3\");";

  const char* json_query_3 = "select (_1.c1 like \"_3\") from s3object[*].root where character_length(_1.c1) = 2 and substring(_1.c1,2,1) in (\"3\");";

  std::string s3select_result_3 = run_s3select(input_query_3,input, json_query_3);

  ASSERT_NE(s3select_result_3,failure_sign);

  auto count_3 = std::count(s3select_result_3.begin(), s3select_result_3.end(), '0');

  ASSERT_EQ(count_3,0);

  const std::string input_query_4 = "select (int(_1) in (1)) from s3object where int(_1) = 1;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_NE(s3select_result_4,failure_sign);

  auto count_4 = std::count(s3select_result_4.begin(), s3select_result_4.end(), '0');

  ASSERT_EQ(count_4,0);
}

TEST(TestS3selectFunctions, truefalse_multirows_expressions)
{
  std::string input, input1;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(0) from s3object where cast(_3 as int)>999 = true;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where char_length(_3)>3 = true;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(0) from s3object where char_length(_3)=3 = true;";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from s3object where cast(_3 as int)>99 = true and cast(_3 as int)<1000 = true;";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  generate_rand_columns_csv_with_null(input1, size);

  const std::string input_query_5 = "select count(0) from s3object where (_3 is null) = true;";

  std::string s3select_result_5 = run_s3select(input_query_5,input1);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select count(0) from s3object where (nullif(_3,null) is null) = true;";

  std::string s3select_result_6 = run_s3select(input_query_6,input1);

  ASSERT_NE(s3select_result_6,failure_sign);

  ASSERT_EQ(s3select_result_5,s3select_result_6);
}

TEST(TestS3selectFunctions, truefalse_date_time_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv_datetime(input, size);
  const std::string input_query_1 = "select count(0) from s3object where extract(year from to_timestamp(_1)) > 1950 = true and extract(year from to_timestamp(_1)) < 1960 = true;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where int(substring(_1,1,4))>1950 = true and int(substring(_1,1,4))<1960 = true;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);
}

TEST(TestS3selectFunctions, truefalse_trim_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_csv_trim(input, size);
  const std::string input_query_1 = "select count(0) from stdin where trim(_1) = \"aeiou\" = true;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from stdin where substring(_1 from 6 for 5) = \"aeiou\" = true;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);
}

TEST(TestS3selectFunctions, tuefalse_like_expressions)
{
  std::string input, input1;
  size_t size = 10000;
  generate_csv(input, size);
  const std::string input_query_1 = "select count(0) from stdin where (_4 like \"%ar\") = true;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from stdin where (substring(_4,char_length(_4),1) = \"r\") = true and (substring(_4,char_length(_4)-1,1) = \"a\") = true;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  generate_csv_like(input1, size);

  const std::string input_query_3 = "select count(0) from stdin where (_1 like \"%aeio%\") = true;";

  std::string s3select_result_3 = run_s3select(input_query_3,input1);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from stdin where (substring(_1,4,4) = \"aeio\") = true;";

  std::string s3select_result_4 = run_s3select(input_query_4,input1);

  ASSERT_EQ(s3select_result_3, s3select_result_4);

  const std::string input_query_5 = "select count(0) from stdin where (_1 like \"%r[r-s]\") = true;";

  std::string s3select_result_5 = run_s3select(input_query_5,input1);

  ASSERT_NE(s3select_result_5,failure_sign);

  const std::string input_query_6 = "select count(0) from stdin where (substring(_1,char_length(_1),1) between \"r\" and \"s\") = true and (substring(_1,char_length(_1)-1,1) = \"r\") = true;";

  std::string s3select_result_6 = run_s3select(input_query_6,input1);

  ASSERT_EQ(s3select_result_5, s3select_result_6);

  const std::string input_query_7 = "select count(0) from stdin where (_1 like \"%br_\") = true;";

  std::string s3select_result_7 = run_s3select(input_query_7,input);

  ASSERT_NE(s3select_result_7,failure_sign);

  const std::string input_query_8 = "select count(0) from stdin where (substring(_1,char_length(_1)-1,1) = \"r\") = true and (substring(_1,char_length(_1)-2,1) = \"b\") = true;";

  std::string s3select_result_8 = run_s3select(input_query_8,input);

  ASSERT_EQ(s3select_result_7, s3select_result_8);

  const std::string input_query_9 = "select count(0) from stdin where (_1 like \"%r[r-s]\") = false;";

  std::string s3select_result_9 = run_s3select(input_query_9,input1);

  ASSERT_NE(s3select_result_9,failure_sign);

  const std::string input_query_10 = "select count(0) from stdin where (substring(_1,char_length(_1),1) not between \"r\" and \"s\") = true or (substring(_1,char_length(_1)-1,1) = \"r\") = false;";

  std::string s3select_result_10 = run_s3select(input_query_10,input1);

  ASSERT_EQ(s3select_result_9, s3select_result_10);
}

TEST(TestS3selectFunctions, truefalse_coalesce_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select count(0) from s3object where char_length(_3)>2 and char_length(_4)>2 = true and cast(substring(_3,1,2) as int) = cast(substring(_4,1,2) as int) = true;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from s3object where cast(_3 as int)>99 = true and cast(_4 as int)>99 = true and (coalesce(nullif(cast(substring(_3,1,2) as int),cast(substring(_4,1,2) as int)),7) = 7) = true;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);
}

TEST(TestS3selectFunctions, truefalse_in_expressions)
{
  std::string input;
  size_t size = 10000;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select int(_1) from s3object where (int(_1) in(1)) = true;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select int(_1) from s3object where int(_1) = 1 = true;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_7 = "select int(_2) from s3object where (int(_2)*2 in(int(_3)*2,int(_4)*3,int(_5)*5)) = true;";

  std::string s3select_result_7 = run_s3select(input_query_7,input);

  ASSERT_NE(s3select_result_7,failure_sign);

  const std::string input_query_8 = "select int(_2) from s3object where int(_2)*2 = int(_3)*2 = true or int(_2)*2 = int(_4)*3 = true or int(_2)*2 = int(_5)*5 = true;";

  std::string s3select_result_8 = run_s3select(input_query_8,input);

  ASSERT_EQ(s3select_result_7, s3select_result_8);

  const std::string input_query_9 = "select int(_1) from s3object where character_length(_1) = 2 = true and (substring(_1,2,1) in (\"3\")) = true;";

  std::string s3select_result_9 = run_s3select(input_query_9,input);

  ASSERT_NE(s3select_result_9,failure_sign);

  const std::string input_query_10 = "select int(_1) from s3object where (_1 like \"_3\") = true;";

  const char* json_query_10 = "select int(_1.c1) from s3object[*].root where (_1.c1 like \"_3\") = true;";

  std::string s3select_result_10 = run_s3select(input_query_10,input,json_query_10);

  ASSERT_EQ(s3select_result_9, s3select_result_10);
}

TEST(TestS3selectFunctions, truefalse_alias_expressions)
{
  std::string input;
  size_t size = 100;
  generate_rand_columns_csv(input, size);
  const std::string input_query_1 = "select (int(_1) > int(_2)) as a1 from s3object where a1 = true ;";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select (int(_1) > int(_2)) from s3object where int(_1) > int(_2) = true;";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);
}
TEST(TestS3selectFunctions, charlength)
{
test_single_column_single_row( "select char_length(\"abcde\") from stdin;","5\n");
}

TEST(TestS3selectFunctions, characterlength)
{
test_single_column_single_row( "select character_length(\"abcde\") from stdin;","5\n");
}

TEST(TestS3selectFunctions, emptystring)
{
test_single_column_single_row( "select char_length(\"\") from stdin;","0\n");
}

TEST(TestS3selectFunctions, lower)
{
test_single_column_single_row( "select lower(\"ABcD12#$e\") from stdin;","abcd12#$e\n");
}

TEST(TestS3selectFunctions, upper)
{
test_single_column_single_row( "select upper(\"abCD12#$e\") from stdin;","ABCD12#$E\n");
}

TEST(TestS3selectFunctions, mod)
{
test_single_column_single_row( "select 5%2 from stdin;","1\n");
}

TEST(TestS3selectFunctions, modzero)
{
test_single_column_single_row( "select 0%2 from stdin;","0\n");
}

TEST(TestS3selectFunctions, nullif)
{
test_single_column_single_row( "select nullif(5,3) from stdin;","5\n");
}

TEST(TestS3selectFunctions, nullifeq)
{
test_single_column_single_row( "select nullif(5,5) from stdin;","null\n");
}

TEST(TestS3selectFunctions, nullifnull)
{
test_single_column_single_row( "select nullif(null,null) from stdin;","null\n");
}

TEST(TestS3selectFunctions, nullifintnull)
{
test_single_column_single_row( "select nullif(7, null) from stdin;","7\n");
}

TEST(TestS3selectFunctions, nullifintstring)
{
test_single_column_single_row( "select nullif(5, \"hello\") from stdin;","5\n");
}

TEST(TestS3selectFunctions, nullifstring)
{
test_single_column_single_row( "select nullif(\"james\",\"bond\") from stdin;","james\n");
}

TEST(TestS3selectFunctions, nullifeqstring)
{
test_single_column_single_row( "select nullif(\"redhat\",\"redhat\") from stdin;","null\n");
}

TEST(TestS3selectFunctions, nullifnumericeq)
{
test_single_column_single_row( "select nullif(1, 1.0) from stdin;","null\n");
}

TEST(TestS3selectFunctions, nulladdition)
{
test_single_column_single_row( "select 1 + null from stdin;","null\n");
}

TEST(TestS3selectFunctions, isnull)
{
test_single_column_single_row( "select \"true\" from stdin where nullif(1,1) is null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnullnot)
{
test_single_column_single_row( "select \"true\" from stdin where not nullif(1,2) is null;" ,"true\n");
}

TEST(TestS3selectFunctions, case_insensitive_not_null)
{
test_single_column_single_row( "select \"false\" from stdin where nullif(1,1) is NOT null;" ,"");
test_single_column_single_row( "select \"false\" from stdin where nullif(1,1) is not Null;" ,"");
test_single_column_single_row( "select \"true\" from stdin where nullif(1,1) is  Null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull1)
{
test_single_column_single_row( "select \"true\" from stdin where 7 + null is null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull2)
{
test_single_column_single_row( "select \"true\" from stdin where null + 7 is null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull3)
{
test_single_column_single_row( "select \"true\" from stdin where (null > 1) is null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull4)
{
test_single_column_single_row( "select \"true\" from stdin where (1 <= null) is null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull5)
{
test_single_column_single_row( "select \"true\" from stdin where (null > 2 and 1 = 0) is not null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull6)
{
test_single_column_single_row( "select \"true\" from stdin where (null>2 and 2>1) is  null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull7)
{
test_single_column_single_row( "select \"true\" from stdin where (null>2 or null<=3) is  null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull8)
{
test_single_column_single_row( "select \"true\" from stdin where (5<4 or null<=3) is  null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull9)
{
test_single_column_single_row( "select \"true\" from stdin where (null<=3 or 5<3) is  null;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull10)
{
test_single_column_single_row( "select \"true\" from stdin where (null<=3 or 5>3) ;" ,"true\n");
}

TEST(TestS3selectFunctions, nullnot)
{
test_single_column_single_row( "select \"true\" from stdin where not (null>0 and 7<3) ;" ,"true\n");
}

TEST(TestS3selectFunctions, nullnot1)
{
test_single_column_single_row( "select \"true\" from stdin where not  (null>0 or 4>3) and (7<1) ;" ,"true\n");
}

TEST(TestS3selectFunctions, isnull11)
{
test_single_column_single_row( "select \"true\" from stdin where (5>3 or null<1) ;" ,"true\n");
}

TEST(TestS3selectFunctions, likeop)
{
test_single_column_single_row( "select \"true\" from stdin where \"qwertyabcde\" like \"%abcde\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeopfalse)
{
test_single_column_single_row( "select \"true\" from stdin where not  \"qwertybcde\" like \"%abcde\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop1)
{
test_single_column_single_row( "select \"true\" from stdin where \"qwertyabcdeqwerty\" like \"%abcde%\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop1false)
{
test_single_column_single_row( "select \"true\" from stdin where not \"qwertyabcdqwerty\" like \"%abcde%\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop2)
{
test_single_column_single_row( "select \"true\" from stdin where \"abcdeqwerty\" like \"abcde%\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop2false)
{
test_single_column_single_row( "select \"true\" from stdin where not  \"abdeqwerty\" like \"abcde%\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop6)
{
test_single_column_single_row( "select \"true\" from stdin where \"abqwertyde\" like \"ab%de\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop3false)
{
test_single_column_single_row( "select \"true\" from stdin where not \"aabcde\" like \"_bcde\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop3mix)
{
test_single_column_single_row( "select \"true\" from stdin where  \"aabbccdef\" like \"_ab%\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop4mix)
{
test_single_column_single_row( "select \"true\" from stdin where \"aabbccdef\" like \"%de_\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop4)
{
test_single_column_single_row( "select \"true\" from stdin where \"abcde\" like \"abc_e\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop4false)
{
test_single_column_single_row( "select \"true\" from stdin where not  \"abcccddyddyde\" like \"abc_e\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop5)
{
test_single_column_single_row( "select \"true\" from stdin where \"ebcde\" like \"[d-f]bcde\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop5false)
{
test_single_column_single_row( "select \"true\" from stdin where not  \"abcde\" like \"[d-f]bcde\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeopdynamic)
{
test_single_column_single_row( "select \"true\" from stdin where \"abcde\" like substring(\"abcdefg\",1,5);" ,"true\n");
}

TEST(TestS3selectFunctions, likeop5not)
{
test_single_column_single_row( "select \"true\" from stdin where \"abcde\" like \"[^d-f]bcde\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop7)
{
test_single_column_single_row( "select \"true\" from stdin where \"qwertyabcde\" like \"%%%%abcde\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop8beginning)
{
test_single_column_single_row( "select \"true\" from stdin where \"abcde\" like \"[abc]%\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop8false)
{
test_single_column_single_row( "select \"true\" from stdin where not \"dabc\" like \"[abc]%\";" ,"true\n");
}

TEST(TestS3selectFunctions, likeop8end)
{
test_single_column_single_row( "select \"true\" from stdin where \"xyza\" like \"%[abc]\";" ,"true\n");
}

TEST(TestS3selectFunctions, inoperator)
{
test_single_column_single_row( "select \"true\" from stdin where \"a\" in (\"b\", \"a\");" ,"true\n");
}

TEST(TestS3selectFunctions, inoperatorfalse)
{
test_single_column_single_row( "select \"true\" from stdin where not \"a\" in (\"b\", \"c\");" ,"true\n");
}

TEST(TestS3selectFunctions, inoperatormore)
{
test_single_column_single_row( "select \"true\" from stdin where \"a\" in (\"b\", \"a\", \"d\", \"e\", \"f\");" ,"true\n");
}

TEST(TestS3selectFunctions, inoperatormixtype)
{
test_single_column_single_row( "select \"true\" from stdin where 10 in (5.0*2.0, 12+1, 9+1.2, 22/2, 12-3);" ,"true\n");
}

TEST(TestS3selectFunctions, mix)
{
test_single_column_single_row( "select \"true\" from stdin where   \"abcde\" like \"abc_e\" and 10 in (5.0*2.0, 12+1) and nullif(2,2) is null;" ,"true\n");
}

TEST(TestS3selectFunctions, case_when_then_else)
{
test_single_column_single_row( "select  case when (1+1+1*1=(2+1)*3)  then \"case-1-1\" when ((4*3)=(12)) then \"case-1-2\" else \"case-else-1\" end , case when 1+1*7=(2+1)*3  then \"case-2-1\" when ((4*3)=(12)+1) then \"case-2-2\" else \"case-else-2\" end from stdin where (3*3=9);" ,"case-1-2,case-else-2\n");
}

TEST(TestS3selectFunctions, simple_case_when)
{
test_single_column_single_row( "select  case 2+1 when (3+4) then \"case-1-1\" when 3 then \"case-3\" else \"case-else-1\" end from stdin;","case-3\n");
}

TEST(TestS3selectFunctions, nested_case)
{
test_single_column_single_row( "select case when ((3+4) = (7 *1)) then \"case-1-1\" else \"case-2-2\" end, case 1+3 when 2+3 then \"case-1-2\" else \"case-2-1\"  end from stdin where (3*3 = 9);","case-1-1,case-2-1\n");
}

TEST(TestS3selectFunctions, substr11)
{
test_single_column_single_row( "select substring(\"01234567890\",2*0+1,1.53*0+3) from stdin ;" ,"012\n");
}

TEST(TestS3selectFunctions, substr12)
{
test_single_column_single_row( "select substring(\"01234567890\",2*0+1,1+2.0) from stdin ;" ,"012\n");
}

TEST(TestS3selectFunctions, substr13)
{
test_single_column_single_row( "select substring(\"01234567890\",2.5*2+1,1+2) from stdin ;" ,"567\n");
}

TEST(TestS3selectFunctions, substr14)
{
test_single_column_single_row( "select substring(\"123456789\",0) from stdin ;" ,"123456789\n");
}

TEST(TestS3selectFunctions, substr15)
{
test_single_column_single_row( "select substring(\"123456789\",-4) from stdin ;" ,"123456789\n");
}

TEST(TestS3selectFunctions, substr16)
{
test_single_column_single_row( "select substring(\"123456789\",0,100) from stdin ;" ,"123456789\n");
}

TEST(TestS3selectFunctions, substr17)
{
test_single_column_single_row( "select substring(\"12345\",0,5) from stdin ;" ,"1234\n");
}

TEST(TestS3selectFunctions, substr18)
{
test_single_column_single_row( "select substring(\"12345\",-1,5) from stdin ;" ,"123\n");
}

TEST(TestS3selectFunctions, substr19)
{
test_single_column_single_row( "select substring(\"123456789\" from 0) from stdin ;" ,"123456789\n");
}

TEST(TestS3selectFunctions, substr20)
{
test_single_column_single_row( "select substring(\"123456789\" from -4) from stdin ;" ,"123456789\n");
}

TEST(TestS3selectFunctions, substr21)
{
test_single_column_single_row( "select substring(\"123456789\" from 0 for 100) from stdin ;" ,"123456789\n");
}

TEST(TestS3selectFunctions, substr22)
{
test_single_column_single_row( "select \"true\" from stdin where 5 = cast(substring(\"523\",1,1) as int);" ,"true\n");
}

TEST(TestS3selectFunctions, substr23)
{
test_single_column_single_row( "select \"true\" from stdin where cast(substring(\"523\",1,1) as int) > cast(substring(\"123\",1,1) as int)  ;" ,"true\n");
}

TEST(TestS3selectFunctions, coalesce)
{
test_single_column_single_row( "select coalesce(5,3) from stdin;","5\n");
}

TEST(TestS3selectFunctions, coalesceallnull)
{
test_single_column_single_row( "select coalesce(nullif(5,5),nullif(1,1.0)) from stdin;","null\n");
}

TEST(TestS3selectFunctions, coalesceanull)
{
test_single_column_single_row( "select coalesce(nullif(5,5),nullif(1,1.0),2) from stdin;","2\n");
}

TEST(TestS3selectFunctions, coalescewhere)
{
test_single_column_single_row( "select \"true\" from stdin where  coalesce(nullif(7.0,7),nullif(4,4.0),6) = 6;" ,"true\n");
}

TEST(TestS3selectFunctions, castint)
{
test_single_column_single_row( "select cast(5.123 as int) from stdin ;" ,"5\n");
}

TEST(TestS3selectFunctions, castfloat)
{
test_single_column_single_row( "select cast(1.234 as FLOAT) from stdin ;" ,"1.234\n");
}

TEST(TestS3selectFunctions, castfloatoperation)
{
test_single_column_single_row( "select cast(1.234 as float) + cast(1.235 as float) from stdin ;" ,"2.4690000000000003\n");
}

TEST(TestS3selectFunctions, caststring)
{
test_single_column_single_row( "select cast(1234 as string) from stdin ;" ,"1234\n");
}

TEST(TestS3selectFunctions, caststring1)
{
test_single_column_single_row( "select cast('12hddd' as int) from stdin ;" ,"#failure#","extra characters after the number");
}

TEST(TestS3selectFunctions, caststring2)
{
test_single_column_single_row( "select cast('124' as int) + 1 from stdin ;" ,"125\n");
}

TEST(TestS3selectFunctions, castsubstr)
{
test_single_column_single_row( "select substring(cast(cast(\"1234567\" as int) as string),2,2) from stdin ;" ,"23\n");
}

TEST(TestS3selectFunctions, casttimestamp)
{
test_single_column_single_row( "select cast('2010-01-15T13:30:10Z' as timestamp)  from stdin ;" ,"2010-01-15T13:30:10Z\n");
}

TEST(TestS3selectFunctions, castdateadd)
{
test_single_column_single_row( "select date_add(day, 2, cast('2010-01-15T13:30:10Z' as timestamp)) from stdin ;" ,"2010-01-17T13:30:10Z\n");
}

TEST(TestS3selectFunctions, castdatediff)
{
test_single_column_single_row( "select date_diff(year,cast('2010-01-15T13:30:10Z' as timestamp), cast('2020-01-15T13:30:10Z' as timestamp)) from stdin ;" ,"10\n");
}

TEST(TestS3selectFunctions, trim)
{
test_single_column_single_row( "select trim(\"   \twelcome\t   \") from stdin ;" ,"\twelcome\t\n");
}

TEST(TestS3selectFunctions, trim1)
{
test_single_column_single_row( "select trim(\"   foobar   \") from stdin ;" ,"foobar\n");
}

TEST(TestS3selectFunctions, trim2)
{
test_single_column_single_row( "select trim(trailing from \"   foobar   \") from stdin ;" ,"   foobar\n");
}

TEST(TestS3selectFunctions, trim3)
{
test_single_column_single_row( "select trim(leading from \"   foobar   \") from stdin ;" ,"foobar   \n");
}

TEST(TestS3selectFunctions, trim4)
{
test_single_column_single_row( "select trim(both from \"   foobar   \") from stdin ;" ,"foobar\n");
}

TEST(TestS3selectFunctions, trim5)
{
test_single_column_single_row( "select trim(from \"   foobar   \") from stdin ;" ,"foobar\n");
}

TEST(TestS3selectFunctions, trim6)
{
test_single_column_single_row( "select trim(both \"12\" from  \"1112211foobar22211122\") from stdin ;" ,"foobar\n");
}

TEST(TestS3selectFunctions, trim7)
{
test_single_column_single_row( "select substring(trim(both from '   foobar   '),2,3) from stdin ;" ,"oob\n");
}

TEST(TestS3selectFunctions, trim8)
{
test_single_column_single_row( "select substring(trim(both '12' from '1112211foobar22211122'),1,6) from stdin ;" ,"foobar\n");
}

TEST(TestS3selectFunctions, trim9)
{
test_single_column_single_row( "select cast(trim(both \"12\" from \"111221134567822211122\") as int) + 5 from stdin ;" ,"345683\n");
}

TEST(TestS3selectFunctions, trimefalse)
{
test_single_column_single_row( "select cast(trim(both from \"12\" \"111221134567822211122\") as int) + 5 from stdin ;" ,"#failure#","");
}

TEST(TestS3selectFunctions, trim10)
{
test_single_column_single_row( "select trim(trim(leading from \"   foobar   \")) from stdin ;" ,"foobar\n");
}

TEST(TestS3selectFunctions, trim11)
{
test_single_column_single_row( "select trim(trailing from trim(leading from \"   foobar   \")) from stdin ;" ,"foobar\n");
}

TEST(TestS3selectFunctions, trim12)
{
test_single_column_single_row( "select trim(LEADING '1' from '111abcdef111')  from s3object ;" ,"abcdef111\n");
}

TEST(TestS3selectFunctions, trim13)
{
test_single_column_single_row( "select trim(TRAILING '1' from '111abcdef111')  from s3object ;" ,"111abcdef\n");
}

TEST(TestS3selectFunctions, likescape)
{
  test_single_column_single_row("select \"true\" from stdin where  \"abc_defgh\" like \"abc$_defgh\" escape \"$\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"j_kerhai\" like \"j#_%\" escape \"#\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"jok_ai\" like \"%#_ai\" escape \"#\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"jo_aibc\" like \"%#_ai%\" escape \"#\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"jok%abc\" like \"jok$%abc\" escape \"$\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"ab%%a\" like \"ab$%%a\" escape \"$\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"_a_\" like \"=_a=_\" escape \"=\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"abc#efgh\" like \"abc##efgh\" escape \"#\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"%abs%\" like \"#%abs#%\" escape \"#\";","true\n");
  test_single_column_single_row("select \"true\" from s3object where  \"abc##efgh\" like \"abc####efgh\" escape \"#\";","true\n");
}

TEST(TestS3selectFunctions, likescapedynamic)
{
test_single_column_single_row( "select \"true\" from s3object where  \"abc#efgh\" like substring(\"abc##efghi\",1,9) escape \"#\";" ,"true\n");
test_single_column_single_row( "select \"true\" from s3object where  \"abcdefgh\" like substring(\"abcd%abc\",1,5);" ,"true\n");
test_single_column_single_row( "select \"true\" from s3object where  substring(\"abcde\",1,5) like \"abcd_\" ;" ,"true\n");
test_single_column_single_row( "select \"true\" from s3object where  substring(\"abcde\",1,5) like substring(\"abcd_ab\",1,5) ;" ,"true\n");
}

TEST(TestS3selectFunctions, test_escape_expressions)
{
  std::string input, input1;
  size_t size = 10000;
  generate_csv_escape(input, size);
  const std::string input_query_1 = "select count(0) from stdin where _1 like \"%_ar\" escape \"%\";";

  std::string s3select_result_1 = run_s3select(input_query_1,input);

  ASSERT_NE(s3select_result_1,failure_sign);

  const std::string input_query_2 = "select count(0) from stdin where substring(_1,char_length(_1),1) = \"r\" and substring(_1,char_length(_1)-1,1) = \"a\" and substring(_1,char_length(_1)-2,1) = \"_\";";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  ASSERT_EQ(s3select_result_1, s3select_result_2);

  const std::string input_query_3 = "select count(0) from stdin where _2 like \"%aeio$_\" escape \"$\";";

  std::string s3select_result_3 = run_s3select(input_query_3,input);

  ASSERT_NE(s3select_result_3,failure_sign);

  const std::string input_query_4 = "select count(0) from stdin where substring(_2,1,5) = \"aeio_\";";

  std::string s3select_result_4 = run_s3select(input_query_4,input);

  ASSERT_EQ(s3select_result_3, s3select_result_4);
}

void generate_csv_multirow(std::string& out, int loop = 1) {
  // schema is: int, float, string, string
  std::stringstream ss;
  for(int i = 0; i < loop; i++)
  {
    ss << "1,42926,7334,5.5,Brandise,Letsou,Brandise.Letsou@yopmail.com,worker,2020-10-26T11:21:30.397Z" << std::endl;
    ss << "2,21169,3648,9.0,Zaria,Weinreb,Zaria.Weinreb@yopmail.com,worker,2009-12-02T01:22:45.8327+09:45" << std::endl;
    ss << "3,35581,9091,2.1,Bibby,Primalia,Bibby.Primalia@yopmail.com,doctor,2001-02-27T23:18:23.446633-12:00" << std::endl;
    ss << "4,38388,7345,4.7,Damaris,Arley,Damaris.Arley@yopmail.com,firefighter,1995-08-24T01:40:00+12:30" << std::endl;
    ss << "5,42802,6464,7.0,Georgina,Georas,Georgina.Georas@yopmail.com,worker,2013-01-30T05:27:59.2Z" << std::endl;
    ss << "6,45582,52863,0.1,Kelly,Hamil,Kelly.Hamil@yopmail.com,police officer,1998-03-31T17:25-01:05" << std::endl;
    ss << "7,8548,7665,3.6,Claresta,Flita,Claresta.Flita@yopmail.com,doctor,2007-10-10T22:00:30Z" << std::endl;
    ss << "8,22633,528,5.3,Bibby,Virgin,Bibby.Virgin@yopmail.com,developer,2020-06-30T11:07:01.23323-00:30" << std::endl;
    ss << "9,38439,5645,2.8,Mahalia,Aldric,Mahalia.Aldric@yopmail.com,doctor,2019-04-20T20:21:22.23+05:15" << std::endl;
    ss << "10,6611,7287,1.0,Pamella,Sibyls,Pamella.Sibyls@yopmail.com,police officer,2000-09-13T14:41Z" << std::endl;
  }
  out = ss.str();
}

TEST(TestS3selectFunctions, limit)
{
  std::string input_csv, input_query, expected_res;
  generate_csv_multirow(input_csv, 2);

  input_query = "select _1 from stdin limit 0;";
  expected_res = "";
  std::cout << "Running query: 1 (when limit is zero)" << std::endl;
  auto s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _1 from stdin limit 8;";
  expected_res = "1\n2\n3\n4\n5\n6\n7\n8\n";
  std::cout << "Running query: 2 (non-aggregate query, limit clause only)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _1 from stdin where _2 > _3 limit 8;";
  expected_res = "7\n";
  std::cout << "Running query: 3 (non-aggregate_query, where + limit clause)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _1 from stdin where _2 > _3 limit 7;";
  expected_res = "7\n";
  std::cout << "Running query: 4 (non-aggregate_query, where + limit clause)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _1 from stdin where _2 > _3 limit 6;";
  expected_res = "";
  std::cout << "Running query: 5 (non-aggregate_query, where + limit clause)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select count(0) from stdin limit 9;";
  expected_res = "9";
  std::cout << "Running query: 6 (aggregate query, limit clause only)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select count(0) from stdin where _2 > _3 limit 8;";
  expected_res = "1";
  std::cout << "Running query: 7 (aggregate_query, where + limit clause)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select count(0) from stdin where _2 > _3 limit 7;";
  expected_res = "1";
  std::cout << "Running query: 8 (aggregate_query, where + limit clause)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select count(0) from stdin where _2 > _3 limit 6;";
  expected_res = "0";
  std::cout << "Running query: 9 (aggregate_query, where + limit clause)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  generate_csv_multirow(input_csv, 10000);

  input_query = "select count(0) from stdin limit 90000;";
  expected_res = "90000";
  std::cout << "Running query: 10 (aggregate_query, limit clause only, with Large input)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select count(0) from stdin where _2 > _3 limit 90000;";
  expected_res = "9000";
  std::cout << "Running query: 11 (aggregate_query, where + limit clause, with Large input)" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  std::string json_input=R"(
{
"firstName": "Joe",
"lastName": "Jackson",
"gender": "male",
"age": 20,
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},

"phoneNumbers": [
{ "type": "home1", "number": "734928_1","addr": 11 },
{ "type": "home2", "number": "734928_2","addr": 22 },
{ "type": "home3", "number": "734928_3","addr": 33 },
{ "type": "home4", "number": "734928_4","addr": 44 },
{ "type": "home5", "number": "734928_5","addr": 55 },
{ "type": "home6", "number": "734928_6","addr": 66 },
{ "type": "home7", "number": "734928_7","addr": 77 },
{ "type": "home8", "number": "734928_8","addr": 88 },
{ "type": "home9", "number": "734928_9","addr": 99 },
{ "type": "home10", "number": "734928_10","addr": 100 },
{ "type": "home11", "number": "734928_11","addr": 101 },
{ "type": "home12", "number": "734928_12","addr": 102 },
{ "type": "home13", "number": "734928_13","addr": 103 },
{ "type": "home14", "number": "734928_14","addr": 104 },
{ "type": "home15", "number": "734928_15","addr": 105 }
],

"key_after_array": "XXX",

"description" : {
  "main_desc" : "value_1",
  "second_desc" : "value_2"
}

}
)";

  const char* input_query_json = "select _1.addr from s3object[*].phoneNumbers limit 0;";
  expected_res = "";
  std::cout << "Running query: 12 (json, limit is zero)" << std::endl;
  run_json_query(input_query_json, json_input, s3select_res);
  EXPECT_EQ(s3select_res, expected_res);

  input_query_json = "select _1.addr from s3object[*].phoneNumbers limit 5;";
  expected_res = "11\n22\n33\n44\n55\n";
  std::cout << "Running query: 13 (json, non-aggregate query, limit clause only)" << std::endl;
  run_json_query(input_query_json, json_input, s3select_res);
  EXPECT_EQ(s3select_res, expected_res);

  input_query_json = "select _1.addr from s3object[*].phoneNumbers where _1.type like \"%1%\" limit 12;";
  expected_res = "11\n100\n101\n102\n";
  std::cout << "Running query: 14 (json, non-aggregate query, where + limit clause)" << std::endl;
  run_json_query(input_query_json, json_input, s3select_res);
  EXPECT_EQ(s3select_res, expected_res);

  input_query_json = "select count(0) from s3object[*].phoneNumbers limit 9;";
  expected_res = "9";
  std::cout << "Running query: 15 (json, aggregate query, limit clause only, limit reached)" << std::endl;
  run_json_query(input_query_json, json_input, s3select_res);
  EXPECT_EQ(s3select_res, expected_res);

  input_query_json = "select count(0) from s3object[*].phoneNumbers limit 18;";
  expected_res = "15";
  std::cout << "Running query: 16 (json, aggregate query, limit clause only, end of stream)" << std::endl;
  run_json_query(input_query_json, json_input, s3select_res);
  EXPECT_EQ(s3select_res, expected_res);

  input_query_json = "select count(0) from s3object[*].phoneNumbers where _1.type like \"%1_\" limit 10;";
  expected_res = "1";
  std::cout << "Running query: 17 (json, aggregate query, where + limit clause)" << std::endl;
  run_json_query(input_query_json, json_input, s3select_res);
  EXPECT_EQ(s3select_res, expected_res);
}

TEST(TestS3selectFunctions, nested_query_single_row_result)
{
  std::string input_csv, input_query, expected_res;
  generate_csv_multirow(input_csv);

  input_query = "select to_string(to_timestamp(\'2009-09-17T17:56:06.234567Z\'), substring(\' athmywopgss-nghjkl\', 3, 10)) from stdin;";
  expected_res = "t5562009wopg06";
  std::cout << "Running query: 1" << std::endl;
  auto s3select_res = run_s3select(input_query);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select to_timestamp(upper(\'2009-09-17t17:56:06.234567z\')) from stdin;";
  #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
    expected_res = "2009-09-17T17:56:06.234567000Z";
  #else
    expected_res = "2009-09-17T17:56:06.234567Z";
  #endif
  std::cout << "Running query: 2" << std::endl;
  s3select_res = run_s3select(input_query);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select count(0) from stdin where extract( year from to_timestamp(_9)) < 2010;";
  expected_res = "6";
  std::cout << "Running query: 3" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

}

TEST(TestS3selectFunctions, nested_query_multirow_result)
{
  std::string input_csv, input_query, expected_res;
  generate_csv_multirow(input_csv);

  input_query = "select to_string(to_timestamp(_9), substring(\' athmywopgssMMMMdXXXXX-nghjkl\', 2, 25)) from stdin;";
  expected_res = "AMt11212020wopg30October26Z-397000000g11\nAMt1222009wopg45December2+09:45-832700000g1\nPMt11182001wopg23February27-12:00-446633000g11\nAMt1401995wopg00August24+12:30-0g1\nAMt5272013wopg59January30Z-200000000g5\nPMt5251998wopg00March31-01:05-0g5\nPMt1002007wopg30October10Z-0g10\nAMt1172020wopg01June30-00:30-233230000g11\nPMt8212019wopg22April20+05:15-230000000g8\nPMt2412000wopg00September13Z-0g2\n";
  std::cout << "Running query: 1" << std::endl;
  auto s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select to_timestamp(upper(lower(_9))) from stdin;";
  #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
    expected_res = "2020-10-26T11:21:30.397000000Z\n2009-12-02T01:22:45.832700000+09:45\n2001-02-27T23:18:23.446633000-12:00\n1995-08-24T01:40:00+12:30\n2013-01-30T05:27:59.200000000Z\n1998-03-31T17:25:00-01:05\n2007-10-10T22:00:30Z\n2020-06-30T11:07:01.233230000-00:30\n2019-04-20T20:21:22.230000000+05:15\n2000-09-13T14:41:00Z\n";
  #else
    expected_res = "2020-10-26T11:21:30.397000Z\n2009-12-02T01:22:45.832700+09:45\n2001-02-27T23:18:23.446633-12:00\n1995-08-24T01:40:00+12:30\n2013-01-30T05:27:59.200000Z\n1998-03-31T17:25:00-01:05\n2007-10-10T22:00:30Z\n2020-06-30T11:07:01.233230-00:30\n2019-04-20T20:21:22.230000+05:15\n2000-09-13T14:41:00Z\n";
  #endif
  std::cout << "Running query: 2" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select count(0) from s3object where extract( year from to_timestamp(_9)) > 2010;";
  expected_res = "4";
  std::cout << "Running query: 3" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _9 from s3object where extract( year from to_timestamp(_9)) > 2010;";
  expected_res = "2020-10-26T11:21:30.397Z\n2013-01-30T05:27:59.2Z\n2020-06-30T11:07:01.23323-00:30\n2019-04-20T20:21:22.23+05:15\n";
  std::cout << "Running query: 4" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _2 from s3object where _2 like \"%11%\";";
  expected_res = "21169\n6611\n";
  std::cout << "Running query: 5" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _5 from s3object where _3 like \"__8\";";
  expected_res = "Bibby\n";
  std::cout << "Running query: 6" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);

  input_query = "select _2 from s3object where _2 like \"%11\";";
  expected_res = "6611\n";
  std::cout << "Running query: 7" << std::endl;
  s3select_res = run_s3select(input_query, input_csv);
  EXPECT_EQ(s3select_res, expected_res);
}

TEST(TestS3selectFunctions, opserialization_expressions)
{
  std::string input;
  size_t size = 10;
  generate_rand_columns_csv(input, size);

  char a[5] = {'@', '#', '$', '%'};
  char b[4] = {'!', '^', '&', '*'};
  char x = a[rand() % 4];
  char y = b[rand() % 4];

  const std::string input_query = "select * from s3object ;";

  run_s3select_test_opserialization(input_query, input, &x, &y);

  const std::string input_query_1 = "select int(_1) from s3object where  nullif(_1, _2) is not null;";

  std::string s3select_result_1 = run_s3select_opserialization_quot(input_query_1,input, true);

  const std::string input_query_2 = "select int(_1) from s3object where int(_1) != int(_2);";

  std::string s3select_result_2 = run_s3select(input_query_2,input);

  std::string s3select_result_2_final = string_to_quot(s3select_result_2);

  ASSERT_EQ(s3select_result_1, s3select_result_2_final);

  const std::string input_query_3 = "select int(_1) from s3object where int(_1) != int(_2);";

  std::string s3select_result_3 = run_s3select_opserialization_quot(input_query_3,input);

  ASSERT_NE(s3select_result_1, s3select_result_3);

  const std::string input_query_4 = "select int(_1) from s3object where  nullif(_1, _2) is not null;";

  std::string s3select_result_4 = run_s3select_opserialization_quot(input_query_4,input, true, x);

  const std::string input_query_5 = "select int(_1) from s3object where int(_1) != int(_2);";

  std::string s3select_result_5 = run_s3select(input_query_5,input);

  std::string s3select_result_5_final = string_to_quot(s3select_result_5, x);

  ASSERT_EQ(s3select_result_4, s3select_result_5_final);

  ASSERT_NE(s3select_result_4, s3select_result_1);
}

void generate_csv_quote_and_escape(std::string& out, char quote = '"', char escp_ch = '\\') {
  std::stringstream ss;

  ss << "1" << "," << "   1   " << "," << quote << "Apple" << "," << ":" << "," << "fruit" << quote << "," << "Apple" << "," << ":" << "," << "fruit" << std::endl;
  ss << std::endl;
  ss << "2" << "," << "	2" << "," << "Apple" << quote << "," << ":" << ","<< quote << "fruit" << "," << "Apple" << "," << ":" << "," << "fruit" << std::endl;
  ss << "			     			" << std::endl;
  ss << "#3" << "," << "#3	" << "," << "Apple" << quote << "," << quote << ":" << quote << "," <<  quote << "fruit" << "," << "Apple" << "," << ":" << "," << "fruit" << std::endl;
  ss << "4" << "," << " 	4 " << "," << quote << quote << "Apple" << quote << "," << ":" << quote << quote << "," << quote << "fruit" << "," << "Apple" << "," << ":" << "," << "fruit" << std::endl;
  ss << "5" << "," << "5	" << "," << "Apple" << escp_ch <<"," << ":" << escp_ch << "," << "fruit" << "," << "Apple" << "," << ":" << "," << "fruit" << std::endl;

  out = ss.str();
}

TEST(TestS3selectFunctions, csv_quote_string_and_escape_char)
{
  std::string input, s3select_result_1, s3select_result_2, s3select_result_3;
  csv_object::csv_defintions csv;
  generate_csv_quote_and_escape(input);
  s3select s3select_syntax1, s3select_syntax2, s3select_syntax3;

  const std::string input_query_1 = "select _3 from s3object;";
  int status = s3select_syntax1.parse_query(input_query_1.c_str());
  ASSERT_EQ(status, 0);

  s3selectEngine::csv_object s3_csv_object_first(&s3select_syntax1, csv);
  s3_csv_object_first.run_s3select_on_object(s3select_result_1, input.c_str(), input.size(), false, false, true);

  const std::string input_query_2 = "select _4,_5,_6 from s3object;";
  status = s3select_syntax2.parse_query(input_query_2.c_str());
  ASSERT_EQ(status, 0);

  s3selectEngine::csv_object s3_csv_object_second(&s3select_syntax2, csv);
  s3_csv_object_second.run_s3select_on_object(s3select_result_2, input.c_str(), input.size(), false, false, true);

  EXPECT_EQ(s3select_result_1, s3select_result_2);

  csv.escape_char = '\0';
  csv.quot_char = '\0';

  const std::string input_query_3 = "select * from s3object;";
  status = s3select_syntax3.parse_query(input_query_3.c_str());
  ASSERT_EQ(status, 0);

  s3selectEngine::csv_object s3_csv_object_third(&s3select_syntax3, csv);
  s3_csv_object_third.run_s3select_on_object(s3select_result_3, input.c_str(), input.size(), false, false, true);

  EXPECT_EQ(s3select_result_3, input);
}

TEST(TestS3selectFunctions, csv_comment_line_and_trim_char)
{
  std::string input;
  std::string s3select_result_1, s3select_result_2;
  generate_csv_quote_and_escape(input);
  s3select s3select_syntax;

  csv_object::csv_defintions csv;
  csv.comment_empty_lines = true;
  csv.comment_chars.push_back('#');
  csv.trim_chars.push_back(' ');
  csv.trim_chars.push_back('\t');

  const std::string input_query_1 = "select _1 from s3object;";
  int status = s3select_syntax.parse_query(input_query_1.c_str());
  ASSERT_EQ(status, 0);

  s3selectEngine::csv_object s3_csv_object_first(&s3select_syntax, csv);
  s3_csv_object_first.run_s3select_on_object(s3select_result_1, input.c_str(), input.size(), false, false, true);

  const std::string input_query_2 = "select _2 from s3object;";
  status = s3select_syntax.parse_query(input_query_2.c_str());
  ASSERT_EQ(status, 0);

  s3selectEngine::csv_object s3_csv_object_second(&s3select_syntax, csv);
  s3_csv_object_second.run_s3select_on_object(s3select_result_2, input.c_str(), input.size(), false, false, true);

  EXPECT_EQ(s3select_result_1, s3select_result_2);
}

TEST(TestS3selectFunctions, presto_syntax_alignments)
{
/*
 * the purpose of this test is to compare 2 queries with different syntax but with the same semantics
 * differences are case-insensitive, table-alias, semicolon at the end-of-statement
 */

  std::string input;
  size_t size = 10000;

  generate_rand_csv(input, size);
  std::string input_for_presto = input;

  const std::string input_query = "select _1,_2 from s3object where _1 = _2;";

  auto s3select_res = run_s3select(input_query, input);

  const std::string input_presto_query = "Select t._1,t._2 fRom s3OBJECT t whEre _1 = _2";

  const char* json_query = "select _1.c1, _1.c2 from s3object[*].root where _1.c1 = _1.c2;";

  auto s3select_presto_res = run_s3select(input_presto_query, input_for_presto, json_query);

  ASSERT_EQ(s3select_res, s3select_presto_res);

}

TEST(TestS3selectFunctions, csv_chunk_processing)
{
//purpose: s3select processes chunk after chunk, doing so it needs to handle the "broken" line issue.
//i.e to identify partial line, save it and later merge it into with the other part on the next chunk.
//
#define STREAM_SIZE 1234
  std::string input_object, input_stream, s3select_result;
  size_t input_off = 0,input_sz = 0;
  int status;
  s3selectEngine::csv_object::csv_defintions csv;
  csv.use_header_info = false;
  s3select s3select_syntax;


  std::string input_query = "select * from stdin;";
  status = s3select_syntax.parse_query(input_query.c_str());
  s3selectEngine::csv_object  s3_csv_object(&s3select_syntax, csv);
  
  generate_rand_csv(input_object, 10000);
  size_t size_sum = 0;
  std::string result_aggr;

	while(input_off<input_object.size())
	{
		if ((input_object.size() - input_off) < STREAM_SIZE)
		{
			input_sz = (input_object.size() - input_off);
		}
		else
		{
			input_sz = STREAM_SIZE;
		}

		size_sum += input_sz;
		input_stream.assign(input_object.data() + input_off, input_object.data() + input_off + input_sz);
		input_off += (input_sz);

    		status = s3_csv_object.run_s3select_on_stream(s3select_result, input_stream.data(), input_stream.size(), input_object.size());

    		if(status<0)
    		{
      			std::cout << "failure on execution " << std::endl << s3_csv_object.get_error_description() <<  std::endl;
      			break;
    		}

    		if(s3select_result.size()>0)
    		{
			result_aggr.append(s3select_result);
			s3select_result.clear();
    		}
    		s3select_result.clear();
  	}//while
	 
	 ASSERT_EQ(result_aggr,input_object);
}

// JSON tests

TEST(TestS3selectFunctions, json_queries)
{
  std::string json_input=R"(
{
"firstName": "Joe",
"lastName": "Jackson",
"gender": "male",
"age": "twenty",
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},

"firstName": "Joe_2",
"lastName": "Jackson_2",
"gender": "male",
"age": 21,
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},

"phoneNumbers": [
{ "type": "home1", "number": "734928_1","addr": 11 },
{ "type": "home2", "number": "734928_2","addr": 22 },
{ "type": "home3", "number": "734928_3","addr": 33 },
{ "type": "home4", "number": "734928_4","addr": 44 },
{ "type": "home5", "number": "734928_5","addr": 55 },
{ "type": "home6", "number": "734928_6","addr": 66 },
{ "type": "home7", "number": "734928_7","addr": 77 },
{ "type": "home8", "number": "734928_8","addr": 88 },
{ "type": "home9", "number": "734928_9","addr": 99 },
{ "type": "home10", "number": "734928_10","addr": 100 }
],

"key_after_array": "XXX",

"description" : {
  "main_desc" : "value_1",
  "second_desc" : "value_2"
}

}
)";

  //count JSON structure, from-clause is empty.
  std::string result;
  const char* input_query = "select count(0) from s3object[*];";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,"1");

  //count JSON structure, from-clause points an array of objects.
  input_query = "select count(0) from s3object[*].phoneNumbers;";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,"10");

  //select specific key in array, from-clause points an array of objects.
  std::string expected_result=R"(11
22
33
44
55
66
77
88
99
100
)";
  input_query = "select _1.addr from s3object[*].phoneNumbers;";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,expected_result);

  //select specific keys in array, operation on fetched value, from-clause is empty.
  expected_result=R"(Joe_2,XXX,25,value_1,value_2
)";
  input_query = "select _1.firstname,_1.key_after_array,_1.age+4,_1.description.main_desc,_1.description.second_desc from s3object[*];";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(null,null,null
null,null,null
null,null,null
null,null,null
null,null,null
null,null,null
null,null,null
null,null,null
null,null,null
null,null,null
)";
  //select non-exists keys, from-clause points on array.
  input_query = "select _1.firstname,_1.key_after_array,_1.age+4 from s3object[*].phonenumbers;";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(7349280
)";

  //select key, operation on value, with predicate(where-clause), from-clause points on array.
  input_query = "select cast(substring(_1.number,1,6) as int) *10 from s3object[*].phonenumbers where _1.type='home2';";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(firstName. : Joe
lastName. : Jackson
gender. : male
age. : twenty
address.streetAddress. : 101
address.city. : San Diego
address.state. : CA
firstName. : Joe_2
lastName. : Jackson_2
gender. : male
age. : 21
address.streetAddress. : 101
address.city. : San Diego
address.state. : CA
phoneNumbers.type. : home1
phoneNumbers.number. : 734928_1
phoneNumbers.addr. : 11
phoneNumbers.type. : home2
phoneNumbers.number. : 734928_2
phoneNumbers.addr. : 22
phoneNumbers.type. : home3
phoneNumbers.number. : 734928_3
phoneNumbers.addr. : 33
phoneNumbers.type. : home4
phoneNumbers.number. : 734928_4
phoneNumbers.addr. : 44
phoneNumbers.type. : home5
phoneNumbers.number. : 734928_5
phoneNumbers.addr. : 55
phoneNumbers.type. : home6
phoneNumbers.number. : 734928_6
phoneNumbers.addr. : 66
phoneNumbers.type. : home7
phoneNumbers.number. : 734928_7
phoneNumbers.addr. : 77
phoneNumbers.type. : home8
phoneNumbers.number. : 734928_8
phoneNumbers.addr. : 88
phoneNumbers.type. : home9
phoneNumbers.number. : 734928_9
phoneNumbers.addr. : 99
phoneNumbers.type. : home10
phoneNumbers.number. : 734928_10
phoneNumbers.addr. : 100
key_after_array. : XXX
description.main_desc. : value_1
description.second_desc. : value_2
#=== 0 ===#
)";

  // star-operation on object, empty from-clause
  input_query = "select * from s3object[*];";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(phoneNumbers.type. : home2
phoneNumbers.number. : 734928_2
phoneNumbers.addr. : 22
#=== 0 ===#
phoneNumbers.type. : home3
phoneNumbers.number. : 734928_3
phoneNumbers.addr. : 33
#=== 1 ===#
phoneNumbers.type. : home4
phoneNumbers.number. : 734928_4
phoneNumbers.addr. : 44
#=== 2 ===#
phoneNumbers.type. : home5
phoneNumbers.number. : 734928_5
phoneNumbers.addr. : 55
#=== 3 ===#
phoneNumbers.type. : home6
phoneNumbers.number. : 734928_6
phoneNumbers.addr. : 66
#=== 4 ===#
phoneNumbers.type. : home7
phoneNumbers.number. : 734928_7
phoneNumbers.addr. : 77
#=== 5 ===#
phoneNumbers.type. : home8
phoneNumbers.number. : 734928_8
phoneNumbers.addr. : 88
#=== 6 ===#
)";

  // star-operation on object, from-clause points on array, with where-clause
  input_query = "select * from s3object[*].phonenumbers where _1.addr between 20 and 89;";
  run_json_query(input_query, json_input,result);
  ASSERT_EQ(result,expected_result);

}

TEST(TestS3selectFunctions, json_queries_with_array)
{
  std::string result;
  std::string expected_result;
  std::string input_query;

  std::string INPUT_TEST_ARRAY_NEDICATIONS = R"(
{
"problems": [{
    "Diabetes":[{
        "medications":[{
            "medicationsClasses":[{
                "className":[{
                    "associatedDrug":[{
                        "name":"asprin",
                        "dose":"",
                        "strength":"500 mg"
                    },
		    { "name":"acamol" } 
		    ],
                    "associatedDrug2":[{
                        "name":"somethingElse",
                        "dose":"",
                        "strength":"500 mg"
                    }]
                }],
                "className2":[{
                    "associatedDrug":[{
                        "name":"asprin",
                        "dose":"",
                        "strength":"500 mg"
                    }],
                    "associatedDrug2":[{
                        "name":"somethingElse",
                        "dose":"",
                        "strength":"500 mg"
                    }]
                }]
            }]
        }],
        "labs":[{
            "missing_field": "missing_value"
        }]
    }],
    "Asthma":[{}]
}]
})";

  expected_result=R"(acamol
)";

  //access a JSON document containing a complex and nested arrays. this query accesses the object="name" at the second element of associatedDrug (within nested arrays)
  input_query = "select _1.problems[0].Diabetes[0].medications[0].medicationsClasses[0].className[0].associatedDrug[1].name from s3object[*];";
  run_json_query(input_query.c_str(), INPUT_TEST_ARRAY_NEDICATIONS, result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(asprin
)";

  //access a JSON document containing a complex and nested arrays. this query accesses the object="name" at the first element of associatedDrug (within nested arrays)
  input_query = "select _1.problems[0].Diabetes[0].medications[0].medicationsClasses[0].className[0].associatedDrug[0].name from s3object[*];";
  run_json_query(input_query.c_str(), INPUT_TEST_ARRAY_NEDICATIONS, result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(somethingElse
)";
  //access a JSON document containing a complex and nested arrays. this query accesses the object="name" at the first element of associatedDrug2 (within nested arrays)
  input_query = "select _1.problems[0].Diabetes[0].medications[0].medicationsClasses[0].className[0].associatedDrug2[0].name from s3object[*];";
  run_json_query(input_query.c_str(), INPUT_TEST_ARRAY_NEDICATIONS, result);
  ASSERT_EQ(result,expected_result);

}

TEST(TestS3selectFunctions, json_queries_with_multi_dimensional_array)
{
  std::string result;
  std::string expected_result;
  std::string input_query;

  //return; //the syntax parser should be modified to accept array[1][2][3] 

std::string input_json_data = R"(
{
"firstName": "Joe",
"lastName": "Jackson",
"gender": "male",
"age": "twenty",
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},

"firstName": "Joe_2",
"lastName": "Jackson_2",
"gender": "male",
"age": 21,
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},

"phoneNumbers": [
{ "type": "home0", "number": "734928_0", "addr": 0 },
{ "type": "home1", "number": "734928_1", "addr": 11 },
{ "type": "home2", "number": "734928_2", "addr": 22 },
{ "type": "home3", "number": "734928_3", "addr": 33 },
{ "type": "home4", "number": "734928_4", "addr": 44 },
{ "type": "home5", "number": "734928_5", "addr": 55 },
{ "type": "home6", "number": "734928_6", "addr": 66 },
{ "type": "home7", "number": "734928_7", "addr": 77 },
{ "type": "home8", "number": "734928_8", "addr": 88 },
{ "type": "home9", "number": "734928_9", "addr": 99 },
{ "type": "home10", "number": "734928_10", "addr": 100 },
"element-11",
  [ 11 , 22 , 
    [ 44, 55] ,"post 3D" , 
    { 
      "first_key_in_object_in_array" : "value_for_irst_key_in_object_in_array", 
      "key_in_array" : "value_per_key_in_array" 
    } 
  ],
  {"classname" : "stam"},
  { "associatedDrug":[{
                        "name":"asprin",
                        "dose":"",
                        "strength":"500 mg"
                    }],
                    "associatedDrug#2":[{
                        "name":"somethingElse",
                        "dose":"",
                        "strength":"500 mg"
                    }]
}
],
"key_after_array": "XXX"
}
)";

  expected_result=R"(null
)";
  //phoneNumbers[12][2][2] is not a discrete value, should return null
  input_query = "select _1.phoneNumbers[12][2][2] from s3object[*];";
  run_json_query(input_query.c_str(), input_json_data, result);
  ASSERT_EQ(result,expected_result);

  //the following tests ia about accessing multi-dimension array
  expected_result=R"(55
)";
  input_query = "select _1.phoneNumbers[12][2][1] from s3object[*];";
  run_json_query(input_query.c_str(), input_json_data, result);
  ASSERT_EQ(result,expected_result);


  expected_result=R"(post 3D
)";
  input_query = "select _1.phoneNumbers[12][3] from s3object[*];";
  run_json_query(input_query.c_str(), input_json_data, result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(11
)";
  input_query = "select _1.phoneNumbers[12][0] from s3object[*];";
  run_json_query(input_query.c_str(), input_json_data, result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(element-11
)";
  input_query = "select _1.phoneNumbers[11] from s3object[*];";
  run_json_query(input_query.c_str(), input_json_data, result);
  ASSERT_EQ(result,expected_result);

input_json_data = R"(
[
  {
    "authors": [
      {
        "id": 2312688602
      },
      {
        "id": 123
      }
    ],
    "wrong" : {"id" : "it-is-wrong"}
  }
]
)";

  expected_result=R"(2312688602
)";
  input_query = "select _1.authors[0].id from s3object[*];";
  run_json_query(input_query.c_str(), input_json_data, result);
  ASSERT_EQ(result,expected_result);

  expected_result=R"(123
)";
  input_query = "select _1.authors[1].id from s3object[*];";
  run_json_query(input_query.c_str(), input_json_data, result);
  ASSERT_EQ(result,expected_result);


 }
