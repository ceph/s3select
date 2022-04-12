#ifndef __S3SELECT_FUNCTIONS__
#define __S3SELECT_FUNCTIONS__


#include "s3select_oper.h"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <regex>
#include <boost/regex.hpp>
#include <algorithm>

using namespace std::string_literals;

#define BOOST_BIND_ACTION_PARAM( push_name ,param ) boost::bind( &push_name::operator(), g_ ## push_name , _1 ,_2, param)
namespace s3selectEngine
{

constexpr double sec_scale(int n)
{
    return pow(10, n);
}

struct push_char
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    *n = *a;
  }

};
static push_char g_push_char;

struct push_2dig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    *n = (static_cast<char>(*a) - 48) * 10 + (static_cast<char>(*(a+1)) - 48) ;
  }

};
static push_2dig g_push_2dig;

struct push_4dig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    *n = (static_cast<char>(*a) - 48) * 1000 + (static_cast<char>(*(a+1)) - 48) * 100 + (static_cast<char>(*(a+2)) - 48) * 10 + (static_cast<char>(*(a+3)) - 48);
  }

};
static push_4dig g_push_4dig;

struct push_1fdig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      const double scale = sec_scale(9-1);    //nano-sec
    #else
      const double scale = sec_scale(6-1);    //micro-sec
    #endif

    *n = ((static_cast<char>(*a) - 48)) * scale;
  }

};
static push_1fdig g_push_1fdig;

struct push_2fdig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      const double scale = sec_scale(9-2);    //nano-sec
    #else
      const double scale = sec_scale(6-2);    //micro-sec
    #endif

    *n = ((static_cast<char>(*a) - 48) * 10 + (static_cast<char>(*(a+1)) - 48)) * scale;
  }

};
static push_2fdig g_push_2fdig;

struct push_3fdig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      const double scale = sec_scale(9-3);    //nano-sec
    #else
      const double scale = sec_scale(6-3);    //micro-sec
    #endif

    *n = ((static_cast<char>(*a) - 48) * 100 + (static_cast<char>(*(a+1)) - 48) * 10 + (static_cast<char>(*(a+2)) - 48)) * scale;
  }

};
static push_3fdig g_push_3fdig;

struct push_4fdig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      const double scale = sec_scale(9-4);    //nano-sec
    #else
      const double scale = sec_scale(6-4);    //micro-sec
    #endif

    *n = ((static_cast<char>(*a) - 48) * 1000 + (static_cast<char>(*(a+1)) - 48) * 100 + (static_cast<char>(*(a+2)) - 48) * 10 + (static_cast<char>(*(a+3)) - 48)) * scale;
  }

};
static push_4fdig g_push_4fdig;

struct push_5fdig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      const double scale = sec_scale(9-5);    //nano-sec
    #else
      const double scale = sec_scale(6-5);    //micro-sec
    #endif

    *n = ((static_cast<char>(*a) - 48) * 10000 + (static_cast<char>(*(a+1)) - 48) * 1000 + (static_cast<char>(*(a+2)) - 48) * 100  + (static_cast<char>(*(a+3)) - 48) * 10 + (static_cast<char>(*(a+4)) - 48)) * scale;
  }

};
static push_5fdig g_push_5fdig;

struct push_6fdig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      const double scale = sec_scale(9-6);    //nano-sec
    #else
      const double scale = sec_scale(6-6);    //micro-sec
    #endif

    *n = ((static_cast<char>(*a) - 48) * 100000 + (static_cast<char>(*(a+1)) - 48) * 10000 + (static_cast<char>(*(a+2)) - 48) * 1000 + (static_cast<char>(*(a+3)) - 48) * 100 + (static_cast<char>(*(a+4)) - 48) * 10 + (static_cast<char>(*(a+5)) - 48)) * scale;
  }

};
static push_6fdig g_push_6fdig;

#if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
  struct push_7fdig
  {
    void operator()(const char* a, const char* b, uint32_t* n) const
    {
      const double scale = sec_scale(9-7);    //nano-sec
      *n = ((static_cast<char>(*a) - 48) * 1000000 + (static_cast<char>(*(a+1)) - 48) * 100000 + (static_cast<char>(*(a+2)) - 48) * 10000 + (static_cast<char>(*(a+3)) - 48) * 1000 + (static_cast<char>(*(a+4)) - 48) * 100 + (static_cast<char>(*(a+5)) - 48) * 10 + (static_cast<char>(*(a+6)) - 48)) * scale;
    }

  };
  static push_7fdig g_push_7fdig;

  struct push_8fdig
  {
    void operator()(const char* a, const char* b, uint32_t* n) const
    {
      const double scale = sec_scale(9-8);    //nano-sec
      *n = ((static_cast<char>(*a) - 48) * 10000000 + (static_cast<char>(*(a+1)) - 48) * 1000000 + (static_cast<char>(*(a+2)) - 48) * 100000 + (static_cast<char>(*(a+3)) - 48) * 10000 + (static_cast<char>(*(a+4)) - 48) * 1000 + (static_cast<char>(*(a+5)) - 48) * 100 + (static_cast<char>(*(a+6)) - 48) * 10 + (static_cast<char>(*(a+7)) - 48)) * scale;
    }

  };
  static push_8fdig g_push_8fdig;

  struct push_9fdig
  {
    void operator()(const char* a, const char* b, uint32_t* n) const
    {
      const double scale = sec_scale(9-9);    //nano-sec
      *n = ((static_cast<char>(*a) - 48) * 100000000 + (static_cast<char>(*(a+1)) - 48) * 10000000 + (static_cast<char>(*(a+2)) - 48) * 1000000 + (static_cast<char>(*(a+3)) - 48) * 100000 + (static_cast<char>(*(a+4)) - 48) * 10000 + (static_cast<char>(*(a+5)) - 48) * 1000 + (static_cast<char>(*(a+6)) - 48) * 100 + (static_cast<char>(*(a+7)) - 48) * 10 + (static_cast<char>(*(a+8)) - 48)) * scale;
    }

  };
  static push_9fdig g_push_9fdig;
#endif

enum class s3select_func_En_t {ADD,
                               SUM,
                               AVG,
                               MIN,
                               MAX,
                               COUNT,
                               TO_INT,
                               TO_FLOAT,
                               TO_TIMESTAMP,
                               TO_STRING_CONSTANT,
                               TO_STRING_DYNAMIC,
                               TO_BOOL,
                               SUBSTR,
                               EXTRACT_YEAR,
                               EXTRACT_MONTH,
                               EXTRACT_DAY,
                               EXTRACT_HOUR,
                               EXTRACT_MINUTE,
                               EXTRACT_SECOND,
                               EXTRACT_WEEK,
                               EXTRACT_TIMEZONE_HOUR,
                               EXTRACT_TIMEZONE_MINUTE,
                               DATE_ADD_YEAR,
                               DATE_ADD_MONTH,
                               DATE_ADD_DAY,
                               DATE_ADD_HOUR,
                               DATE_ADD_MINUTE,
                               DATE_ADD_SECOND,
                               DATE_DIFF_YEAR,
                               DATE_DIFF_MONTH,
                               DATE_DIFF_DAY,
                               DATE_DIFF_HOUR,
                               DATE_DIFF_MINUTE,
                               DATE_DIFF_SECOND,
                               UTCNOW,
                               LENGTH,
                               LOWER,
                               UPPER,
                               NULLIF,
                               BETWEEN,
                               NOT_BETWEEN,
                               IS_NULL,
                               IS_NOT_NULL,
                               IN,
                               LIKE,
                               VERSION,
                               CASE_WHEN_ELSE,
                               WHEN_THEN,
                               WHEN_VALUE_THEN,
                               COALESCE,
                               STRING,
                               TRIM,
                               LEADING,
                               TRAILING
                              };


class s3select_functions
{

private:

  using FunctionLibrary = std::map<std::string, s3select_func_En_t>;
  std::list<base_statement*> __all_query_functions;
  s3select_allocator* m_s3select_allocator;

  const FunctionLibrary m_functions_library =
  {
    {"add", s3select_func_En_t::ADD},
    {"sum", s3select_func_En_t::SUM},
    {"avg", s3select_func_En_t::AVG},
    {"count", s3select_func_En_t::COUNT},
    {"min", s3select_func_En_t::MIN},
    {"max", s3select_func_En_t::MAX},
    {"int", s3select_func_En_t::TO_INT},
    {"float", s3select_func_En_t::TO_FLOAT},
    {"substring", s3select_func_En_t::SUBSTR},
    {"to_timestamp", s3select_func_En_t::TO_TIMESTAMP},
    {"#to_string_constant#",s3select_func_En_t::TO_STRING_CONSTANT},
    {"#to_string_dynamic#",s3select_func_En_t::TO_STRING_DYNAMIC},
    {"to_bool", s3select_func_En_t::TO_BOOL},
    {"#extract_year#", s3select_func_En_t::EXTRACT_YEAR},
    {"#extract_month#", s3select_func_En_t::EXTRACT_MONTH},
    {"#extract_day#", s3select_func_En_t::EXTRACT_DAY},
    {"#extract_hour#", s3select_func_En_t::EXTRACT_HOUR},
    {"#extract_minute#", s3select_func_En_t::EXTRACT_MINUTE},
    {"#extract_second#", s3select_func_En_t::EXTRACT_SECOND},
    {"#extract_week#", s3select_func_En_t::EXTRACT_WEEK},
    {"#extract_timezone_hour#", s3select_func_En_t::EXTRACT_TIMEZONE_HOUR},
    {"#extract_timezone_minute#", s3select_func_En_t::EXTRACT_TIMEZONE_MINUTE},
    {"#dateadd_year#", s3select_func_En_t::DATE_ADD_YEAR},
    {"#dateadd_month#", s3select_func_En_t::DATE_ADD_MONTH},
    {"#dateadd_day#", s3select_func_En_t::DATE_ADD_DAY},
    {"#dateadd_hour#", s3select_func_En_t::DATE_ADD_HOUR},
    {"#dateadd_minute#", s3select_func_En_t::DATE_ADD_MINUTE},
    {"#dateadd_second#", s3select_func_En_t::DATE_ADD_SECOND},
    {"#datediff_year#", s3select_func_En_t::DATE_DIFF_YEAR},
    {"#datediff_month#", s3select_func_En_t::DATE_DIFF_MONTH},
    {"#datediff_day#", s3select_func_En_t::DATE_DIFF_DAY},
    {"#datediff_hour#", s3select_func_En_t::DATE_DIFF_HOUR},
    {"#datediff_minute#", s3select_func_En_t::DATE_DIFF_MINUTE},
    {"#datediff_second#", s3select_func_En_t::DATE_DIFF_SECOND},
    {"utcnow", s3select_func_En_t::UTCNOW},
    {"character_length", s3select_func_En_t::LENGTH},
    {"char_length", s3select_func_En_t::LENGTH},
    {"lower", s3select_func_En_t::LOWER},
    {"upper", s3select_func_En_t::UPPER},
    {"nullif", s3select_func_En_t::NULLIF},
    {"#between#", s3select_func_En_t::BETWEEN},
    {"#not_between#", s3select_func_En_t::NOT_BETWEEN},
    {"#is_null#", s3select_func_En_t::IS_NULL},
    {"#is_not_null#", s3select_func_En_t::IS_NOT_NULL},
    {"#in_predicate#", s3select_func_En_t::IN},
    {"#like_predicate#", s3select_func_En_t::LIKE},
    {"version", s3select_func_En_t::VERSION},
    {"#when-then#", s3select_func_En_t::WHEN_THEN},
    {"#when-value-then#", s3select_func_En_t::WHEN_VALUE_THEN},
    {"#case-when-else#", s3select_func_En_t::CASE_WHEN_ELSE},
    {"coalesce", s3select_func_En_t::COALESCE},
    {"string", s3select_func_En_t::STRING},
    {"#trim#", s3select_func_En_t::TRIM},
    {"#leading#", s3select_func_En_t::LEADING},
    {"#trailing#", s3select_func_En_t::TRAILING}
  };

public:

  base_function* create(std::string_view fn_name,const bs_stmt_vec_t&);

  s3select_functions():m_s3select_allocator(nullptr)
  {
  }

  void push_for_cleanup(base_statement* f)
  {
    __all_query_functions.push_back(f);
  }

  void setAllocator(s3select_allocator* alloc)
  {
    m_s3select_allocator = alloc;
  }

  s3select_allocator* getAllocator()
  {
    return m_s3select_allocator;
  }

  void clean();

};

class __function : public base_statement
{

private:
  bs_stmt_vec_t arguments;
  std::basic_string<char,std::char_traits<char>,ChunkAllocator<char,256>> name;
  base_function* m_func_impl;
  s3select_functions* m_s3select_functions;
  variable m_result;
  bool m_is_aggregate_function;

  void _resolve_name()
  {
    if (m_func_impl)
    {
      return;
    }

    auto string_to_lower = [](std::basic_string<char,std::char_traits<char>,ChunkAllocator<char,256>> s)
    {
      std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){ return std::tolower(c); });
      return s;
    };

    //the function name is converted into lowercase to enable case-insensitive 
    base_function* f = m_s3select_functions->create(string_to_lower(name),arguments);
    if (!f)
    {
      throw base_s3select_exception("function not found", base_s3select_exception::s3select_exp_en_t::FATAL);  //should abort query
    }
    m_func_impl = f;
    m_is_aggregate_function= m_func_impl->is_aggregate();
    m_s3select_functions->push_for_cleanup(this);
    //placement new is releasing the main-buffer in which all AST nodes
    //allocating from it. meaning no calls to destructors.
    //the cleanup method will trigger all destructors.
  }

public:

  base_function* impl()
  {
    return m_func_impl;
  }

  void traverse_and_apply(scratch_area* sa, projection_alias* pa) override
  {
    m_scratch = sa;
    m_aliases = pa;
    for (base_statement* ba : arguments)
    {
      ba->traverse_and_apply(sa, pa);
    }
  }

  void set_last_call() override
  {//it cover the use-case where aggregation function is an argument in non-aggregate function.
    is_last_call = true;
    for (auto& ba : arguments)
    {
      ba->set_last_call();
    }
  }

  void set_skip_non_aggregate(bool skip_non_aggregate_op) override
  {//it cover the use-case where aggregation function is an argument in non-aggregate function.
    m_skip_non_aggregate_op = skip_non_aggregate_op;
    for (auto& ba : arguments)
    {
      ba->set_skip_non_aggregate(m_skip_non_aggregate_op);
    }
  }

  bool is_aggregate() const override
  {
    return m_is_aggregate_function;
  }

  bool semantic() override
  {
    return true;
  }

  __function(const char* fname, s3select_functions* s3f) : name(fname), m_func_impl(nullptr), m_s3select_functions(s3f),m_is_aggregate_function(false) 
  {}

  value& eval() override
  {
    return eval_internal();
  }

  value& eval_internal() override
  {

    _resolve_name();//node is "resolved" (function is created) upon first call/first row.

    if (is_last_call == false)
    {//all rows prior to last row
      if(m_skip_non_aggregate_op == false || is_aggregate() == true)
      {
        (*m_func_impl)(&arguments, &m_result);
      }
      else if(m_skip_non_aggregate_op == true)
      {
        for(auto& p : arguments)
        {//evaluating the arguments (not the function itself, which is a non-aggregate function)
	 //i.e. in the following use case substring( , sum(),count() ) ; only sum() and count() are evaluated.
          p->eval();
        }
      }
    }
    else
    {//on the last row, the aggregate function is finalized, 
     //and non-aggregate function is evaluated with the result of aggregate function.
      if(is_aggregate())
        (*m_func_impl).get_aggregate_result(&m_result);
      else
        (*m_func_impl)(&arguments, &m_result);
    }

    return m_result.get_value();
  }

  void resolve_node() override
  {
    _resolve_name();

    for (auto& arg : arguments)
    {
      arg->resolve_node();
    }
  }

  std::string  print(int ident) override
  {
    return std::string(0);
  }

  void push_argument(base_statement* arg)
  {
    arguments.push_back(arg);
  }


  bs_stmt_vec_t& get_arguments()
  {
    return arguments;
  }

  virtual ~__function() = default;
};


  void s3select_functions::clean()
  {
    for(auto d : __all_query_functions)
    {
      if (d->is_function())
      {
        dynamic_cast<__function*>(d)->impl()->dtor();
      }
      d->dtor();
    }

  }

/*
    s3-select function defintions
*/
struct _fn_add : public base_function
{

  value var_result;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();
    base_statement* x =  *iter;
    iter++;
    base_statement* y = *iter;

    var_result = x->eval() + y->eval();

    *result = var_result;

    return true;
  }
};

struct _fn_sum : public base_function
{

  value sum;

  _fn_sum() : sum(0)
  {
    aggregate = true;
  }

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();
    base_statement* x = *iter;

    try
    {
      sum = sum + x->eval();
    }
    catch (base_s3select_exception& e)
    {
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL)
      {
        throw;
      }
    }

    return true;
  }

  void get_aggregate_result(variable* result) override
  {
    *result = sum ;
  }
};

struct _fn_count : public base_function
{

  int64_t count;

  _fn_count():count(0)
  {
    aggregate=true;
  }

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    count += 1;

    return true;
  }

  void get_aggregate_result(variable* result) override
  {
    result->set_value(count);
  }

};

struct _fn_avg : public base_function
{

    value sum;
    value count{0.0};

    _fn_avg() : sum(0) { aggregate = true; }

    bool operator()(bs_stmt_vec_t* args, variable *result) override
    {
        auto iter = args->begin();
        base_statement *x = *iter;

        try
        {
            sum = sum + x->eval();
            count++;
        }
        catch (base_s3select_exception &e)
        {
            throw base_s3select_exception(e.what());
        }

        return true;
    }

    void get_aggregate_result(variable *result) override
    {
        if(count == static_cast<value>(0)) {
            throw base_s3select_exception("count cannot be zero!");
        } else {
            *result = sum/count ;
        }
    }
};

struct _fn_min : public base_function
{

  value min;

  _fn_min():min(__INT64_MAX__)
  {
    aggregate=true;
  }

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();
    base_statement* x =  *iter;

    if(min > x->eval())
    {
      min=x->eval();
    }

    return true;
  }

  void get_aggregate_result(variable* result) override
  {
    *result = min;
  }

};

struct _fn_max : public base_function
{

  value max;

  _fn_max():max(-__INT64_MAX__)
  {
    aggregate=true;
  }

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();
    base_statement* x =  *iter;

    if(max < x->eval())
    {
      max=x->eval();
    }

    return true;
  }

  void get_aggregate_result(variable* result) override
  {
    *result = max;
  }

};

struct _fn_to_int : public base_function
{
  value var_result;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    value v = (*args->begin())->eval();

    switch (v.type) {

    case value::value_En_t::STRING:
    {
      char* pend;
      errno = 0;
      int64_t i= strtol(v.str(), &pend, 10);
      if (errno == ERANGE) {
        throw base_s3select_exception("converted value would fall out of the range of the result type!");
      }
      if (pend == v.str()) {
        // no number found
        throw base_s3select_exception("text cannot be converted to a number");
      }
      if (*pend) {
        throw base_s3select_exception("extra characters after the number");
      }

      var_result = i;
    }
    break;

    case value::value_En_t::FLOAT:
      var_result = static_cast<int64_t>(v.dbl());
      break;

    default:
      var_result = v.i64();
      break;
    }

    *result = var_result;
    return true;
  }

};

struct _fn_to_float : public base_function
{
  value var_result;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    value v = (*args->begin())->eval();

    switch (v.type) {

    case value::value_En_t::STRING:
    {
      char* pend;
      double d = strtod(v.str(), &pend);
      if (errno == ERANGE) {
        throw base_s3select_exception("converted value would fall out of the range of the result type!");
      }
      if (pend == v.str()) {
        // no number found
        throw base_s3select_exception("text cannot be converted to a number");
      }
      if (*pend) {
        throw base_s3select_exception("extra characters after the number");
      }

      var_result = d;
    }
    break;

    case value::value_En_t::FLOAT:
      var_result = v.dbl();
      break;

    default:
      var_result = v.i64();
      break;
    }

    *result = var_result;
    return true;
  }

};

struct _fn_to_timestamp : public base_function
{
  bsc::rule<> date_separator = bsc::ch_p("-");
  bsc::rule<> time_separator = bsc::ch_p(":");
  bsc::rule<> nano_sec_separator = bsc::ch_p(".");
  bsc::rule<> delimiter = bsc::ch_p("T");
  bsc::rule<> zero_timezone = bsc::ch_p("Z");
  bsc::rule<> timezone_sign = bsc::ch_p("-") | bsc::ch_p("+");

  uint32_t yr = 1700, mo = 1, dy = 1;
  bsc::rule<> dig4 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
  bsc::rule<> dig2 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p];

  bsc::rule<> d_yyyy_dig = ((dig4[BOOST_BIND_ACTION_PARAM(push_4dig, &yr)]) >> *(delimiter));
  bsc::rule<> d_yyyymmdd_dig = ((dig4[BOOST_BIND_ACTION_PARAM(push_4dig, &yr)]) >> *(date_separator)
                                >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &mo)]) >> *(date_separator)
                                >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &dy)]) >> *(delimiter));

  uint32_t hr = 0, mn = 0, sc = 0,  frac_sec = 0, tz_hr = 0, tz_mn = 0, sign, tm_zone = '0';

  #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
    bsc::rule<> fdig9 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
    bsc::rule<> fdig8 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
    bsc::rule<> fdig7 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
  #endif

  bsc::rule<> fdig6 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
  bsc::rule<> fdig5 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
  bsc::rule<> fdig4 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
  bsc::rule<> fdig3 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
  bsc::rule<> fdig2 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p];
  bsc::rule<> fdig1 = bsc::lexeme_d[bsc::digit_p];

  bsc::rule<> d_timezone_dig =  ((timezone_sign[BOOST_BIND_ACTION_PARAM(push_char, &sign)]) >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &tz_hr)]) >> *(time_separator)
                                >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &tz_mn)])) | (zero_timezone[BOOST_BIND_ACTION_PARAM(push_char, &tm_zone)]);

  #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
    bsc::rule<> fraction_sec = (fdig9[BOOST_BIND_ACTION_PARAM(push_9fdig, &frac_sec)]) |
                               (fdig8[BOOST_BIND_ACTION_PARAM(push_8fdig, &frac_sec)]) |
                               (fdig7[BOOST_BIND_ACTION_PARAM(push_7fdig, &frac_sec)]) |
                               (fdig6[BOOST_BIND_ACTION_PARAM(push_6fdig, &frac_sec)]) |
                               (fdig5[BOOST_BIND_ACTION_PARAM(push_5fdig, &frac_sec)]) |
                               (fdig4[BOOST_BIND_ACTION_PARAM(push_4fdig, &frac_sec)]) |
                               (fdig3[BOOST_BIND_ACTION_PARAM(push_3fdig, &frac_sec)]) |
                               (fdig2[BOOST_BIND_ACTION_PARAM(push_2fdig, &frac_sec)]) |
                               (fdig1[BOOST_BIND_ACTION_PARAM(push_1fdig, &frac_sec)]);
  #else
    bsc::rule<> fraction_sec = (fdig6[BOOST_BIND_ACTION_PARAM(push_6fdig, &frac_sec)]) |
                               (fdig5[BOOST_BIND_ACTION_PARAM(push_5fdig, &frac_sec)]) |
                               (fdig4[BOOST_BIND_ACTION_PARAM(push_4fdig, &frac_sec)]) |
                               (fdig3[BOOST_BIND_ACTION_PARAM(push_3fdig, &frac_sec)]) |
                               (fdig2[BOOST_BIND_ACTION_PARAM(push_2fdig, &frac_sec)]) |
                               (fdig1[BOOST_BIND_ACTION_PARAM(push_1fdig, &frac_sec)]);
  #endif

  bsc::rule<> d_time_dig = ((dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &hr)]) >> *(time_separator)
                            >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &mn)]) >> *(time_separator)
                            >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &sc)]) >> *(nano_sec_separator)
                            >> (fraction_sec)  >> (d_timezone_dig)) |
                            ((dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &hr)]) >> *(time_separator)
                            >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &mn)]) >> *(time_separator)
                            >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &sc)]) >> (d_timezone_dig)) |
                            ((dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &hr)]) >> *(time_separator)
                            >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &mn)]) >> (d_timezone_dig));

  bsc::rule<> d_date_time = ((d_yyyymmdd_dig) >> (d_time_dig)) | (d_yyyymmdd_dig) | (d_yyyy_dig);

  timestamp_t tmstmp;
  value v_str;
  int tz_hour, tz_min;

  bool datetime_validation()
  {
    if (yr >= 1400 && yr <= 9999 && mo >= 1 && mo <= 12 && dy >= 1 && hr < 24 && mn < 60 && sc < 60 && tz_hour <= 14 && tz_hour >= -12  && tz_mn < 60)
    {
      if ( (tz_hour == -12 || tz_hour == 14) && tz_mn > 0)
        return false;

      switch (mo)
      {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
		if(dy <= 31)
		{
                  return true;
                }
                break;
        case 4:
        case 6:
        case 9:
        case 11:
                if(dy <= 30)
                {
                  return true;
                }
                break;
        case 2:
                if(dy >= 28)
                {
                  if(!(yr % 4) == 0 && dy > 28)
                  {
                    return false;
                  }
                  else if(!(yr % 100) == 0 && dy <= 29)
                  {
                    return true;
                  }
                  else if(!(yr % 400) == 0 && dy > 28)
                  {
                    return false;
                  }
                  else
                  {
                    return true;
                  }
                }
                else
                {
                  return true;
                }
                break;
        default:
		return false;
		break;
      }
    }
    return false;
  }

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {

    hr = 0;
    mn = 0;
    sc = 0;
    frac_sec = 0;
    tz_hr = 0;
    tz_mn = 0;
    tm_zone = '0';

    auto iter = args->begin();
    int args_size = args->size();

    if (args_size != 1)
    {
      throw base_s3select_exception("to_timestamp should have one parameter");
    }

    base_statement* str = *iter;

    v_str = str->eval();

    if (v_str.type != value::value_En_t::STRING)
    {
      throw base_s3select_exception("to_timestamp first argument must be string");  //can skip current row
    }

    bsc::parse_info<> info_dig = bsc::parse(v_str.str(), d_date_time);

    tz_hour = tz_hr;
    tz_min = tz_mn;
    if ((char)sign == '-')
    {
      tz_hour *= -1;
      tz_min *= -1;
    }

    if(datetime_validation()==false or !info_dig.full)
    {
      throw base_s3select_exception("input date-time is illegal");
    }

    boost::posix_time::ptime new_ptime;

    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      new_ptime = boost::posix_time::ptime(boost::gregorian::date(yr, mo, dy),
                          boost::posix_time::hours(hr) +
                          boost::posix_time::minutes(mn) +
                          boost::posix_time::seconds(sc) +
                          boost::posix_time::nanoseconds(frac_sec));
    #else
      new_ptime = boost::posix_time::ptime(boost::gregorian::date(yr, mo, dy),
                          boost::posix_time::hours(hr) +
                          boost::posix_time::minutes(mn) +
                          boost::posix_time::seconds(sc) +
                          boost::posix_time::microseconds(frac_sec));
    #endif

    tmstmp = std::make_tuple(new_ptime, boost::posix_time::time_duration(tz_hour, tz_min, 0), (char)tm_zone == 'Z');

    result->set_value(&tmstmp);

    return true;
  }

};

struct _fn_to_string_constant : public base_timestamp_to_string
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    if (!initialized)
    {
      prepare_to_string_vector(print_vector, para);
      initialized = true;
    }

    std::string result_ = execute_to_string(print_vector, para);

    result->set_value(result_.c_str());
    return true;
  }
};

struct _fn_to_string_dynamic : public base_timestamp_to_string
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    print_vector.clear();
    para.clear();

    prepare_to_string_vector(print_vector, para);

    std::string result_ = execute_to_string(print_vector, para);

    result->set_value(result_.c_str());
    return true;
  }
};

struct _fn_extract_year_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value( (int64_t)new_ptime.date().year());
    return true;
  }
};

struct _fn_extract_month_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value( (int64_t)new_ptime.date().month());
    return true;
  }
};

struct _fn_extract_day_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value( (int64_t)new_ptime.date().day());
    return true;
  }
};

struct _fn_extract_hour_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value( (int64_t)new_ptime.time_of_day().hours());
    return true;
  }
};

struct _fn_extract_minute_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value( (int64_t)new_ptime.time_of_day().minutes());
    return true;
  }
};

struct _fn_extract_second_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value( (int64_t)new_ptime.time_of_day().seconds());
    return true;
  }
};

struct _fn_extract_week_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value( (int64_t)new_ptime.date().week_number());
    return true;
  }
};

struct _fn_extract_tz_hour_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value((int64_t)td.hours());
    return true;
  }
};

struct _fn_extract_tz_minute_from_timestamp : public base_date_extract
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    result->set_value((int64_t)td.minutes());
    return true;
  }
};

struct _fn_diff_year_timestamp : public base_date_diff
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    int year1 = ptime1.date().year();
    int year2 = ptime2.date().year();
    boost::posix_time::time_duration time1 = boost::posix_time::time_duration(
                    ptime1.time_of_day().hours(), ptime1.time_of_day().minutes(),
                    ptime1.time_of_day().seconds());
    boost::posix_time::time_duration time2 = boost::posix_time::time_duration(
                    ptime2.time_of_day().hours(), ptime2.time_of_day().minutes(),
                    ptime2.time_of_day().seconds());

    if (year2 > year1 && ((ptime2.date().day_of_year() < ptime1.date().day_of_year()) ||
        (ptime2.date().day_of_year() == ptime1.date().day_of_year() && time2 < time1)))
    {
        year2 -= 1;
    }
    else if (year2 < year1 && ((ptime2.date().day_of_year() > ptime1.date().day_of_year()) ||
        (ptime2.date().day_of_year() == ptime1.date().day_of_year() && time2 > time1)))
    {
        year2 += 1;
    }

    int64_t yr = year2 - year1;
    result->set_value( yr );
    return true;
  }
};

struct _fn_diff_month_timestamp : public base_date_diff
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    int year1 = ptime1.date().year();
    int year2 = ptime2.date().year();
    int mon1 = ptime1.date().month();
    int mon2 = ptime2.date().month();
    boost::posix_time::time_duration time1 = boost::posix_time::time_duration(
                    ptime1.time_of_day().hours(), ptime1.time_of_day().minutes(),
                    ptime1.time_of_day().seconds());
    boost::posix_time::time_duration time2 = boost::posix_time::time_duration(
                    ptime2.time_of_day().hours(), ptime2.time_of_day().minutes(),
                    ptime2.time_of_day().seconds());

    if (year2 > year1)
    {
        if (ptime2.date().day() < ptime1.date().day() || (ptime2.date().day() == ptime1.date().day() && time2 < time1))
        {
            mon2 -= 1;
        }

	if (ptime2.date().month() < ptime1.date().month())
        {
            mon2 += 12;
            year2 -= 1;
        }
    }
    else if (year2 < year1)
    {
        if (ptime2.date().day() > ptime1.date().day() || (ptime2.date().day() == ptime1.date().day() && time2 > time1))
        {
            mon1 -= 1;
        }

        if (ptime2.date().month() > ptime1.date().month())
        {
            mon1 += 12;
            year1 -= 1;
        }
    }

    int64_t mon_diff =  (year2 - year1) * 12 + mon2 - mon1;

    result->set_value(mon_diff);
    return true;
  }
};

struct _fn_diff_day_timestamp : public base_date_diff
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    boost::posix_time::time_duration td_res = ptime2 - ptime1;
    int total_seconds = (((td_res.hours() * 60) + td_res.minutes()) * 60) + td_res.seconds();
    int64_t days = total_seconds / (24 * 3600);

    result->set_value(days);
    return true;
  }
};

struct _fn_diff_hour_timestamp : public base_date_diff
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    boost::posix_time::time_duration td_res = ptime2 - ptime1;
    result->set_value((int64_t)td_res.hours());
    return true;
  }
};

struct _fn_diff_minute_timestamp : public base_date_diff
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    boost::posix_time::time_duration td_res = ptime2 - ptime1;
    result->set_value((int64_t)((td_res.hours() * 60) + td_res.minutes()));
    return true;
  }
};

struct _fn_diff_second_timestamp : public base_date_diff
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    boost::posix_time::time_duration td_res = ptime2 - ptime1;
    result->set_value((int64_t)((((td_res.hours() * 60) + td_res.minutes()) * 60) + td_res.seconds()));
    return true;
  }
};

struct _fn_add_year_to_timestamp : public base_date_add
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    new_ptime += boost::gregorian::years( val_quantity.i64() );
    new_tmstmp = std::make_tuple(new_ptime, td, flag);
    result->set_value( &new_tmstmp );
    return true;
  }
};

struct _fn_add_month_to_timestamp : public base_date_add
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    int yr, mn, dy, quant;
    quant = val_quantity.i64();
    dy = new_ptime.date().day();

    int temp = quant % 12;
    mn = new_ptime.date().month() + temp;
    temp = quant / 12;
    yr = new_ptime.date().year() + temp;

    if (mn > 12)
    {
      yr += 1;
      temp = mn % 12;
      if (temp == 0)
      {
        temp = 12;
      }
      mn = temp;
    }
    else if (mn < 1)
    {
      yr -= 1;
      if (mn == 0)
      {
        mn = 12;
      }
      else
      {
        mn = 12 + mn;
      }
    }

    if ((mn == 4 || mn == 6 || mn == 9 || mn == 11) && dy > 30)
    {
      dy = 30;
    }
    else if (mn == 2 && dy > 28)
    {
      if (!(yr % 4) == 0 || ((yr % 100) == 0 && !(yr % 400) == 0))
      {
        dy = 28;
      }
      else
      {
        dy = 29;
      }
    }

    #if BOOST_DATE_TIME_POSIX_TIME_STD_CONFIG
      new_ptime =  boost::posix_time::ptime(boost::gregorian::date(yr, mn, dy),
                    boost::posix_time::hours(new_ptime.time_of_day().hours()) +
                    boost::posix_time::minutes(new_ptime.time_of_day().minutes()) +
                    boost::posix_time::seconds(new_ptime.time_of_day().seconds()) +
                    boost::posix_time::nanoseconds(new_ptime.time_of_day().fractional_seconds()));
    #else
      new_ptime =  boost::posix_time::ptime(boost::gregorian::date(yr, mn, dy),
                    boost::posix_time::hours(new_ptime.time_of_day().hours()) +
                    boost::posix_time::minutes(new_ptime.time_of_day().minutes()) +
                    boost::posix_time::seconds(new_ptime.time_of_day().seconds()) +
                    boost::posix_time::microseconds(new_ptime.time_of_day().fractional_seconds()));
    #endif

    new_tmstmp = std::make_tuple(new_ptime, td, flag);
    result->set_value( &new_tmstmp );
    return true;
  }
};

struct _fn_add_day_to_timestamp : public base_date_add
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    new_ptime += boost::gregorian::days( val_quantity.i64() );
    new_tmstmp = std::make_tuple(new_ptime, td, flag);
    result->set_value( &new_tmstmp );
    return true;
  }
};

struct _fn_add_hour_to_timestamp : public base_date_add
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    new_ptime += boost::posix_time::hours( val_quantity.i64() );
    new_tmstmp = std::make_tuple(new_ptime, td, flag);
    result->set_value( &new_tmstmp );
    return true;
  }
};

struct _fn_add_minute_to_timestamp : public base_date_add
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    new_ptime += boost::posix_time::minutes( val_quantity.i64() );
    new_tmstmp = std::make_tuple(new_ptime, td, flag);
    result->set_value( &new_tmstmp );
    return true;
  }
};

struct _fn_add_second_to_timestamp : public base_date_add
{
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    param_validation(args);

    new_ptime += boost::posix_time::seconds( val_quantity.i64() );
    new_tmstmp = std::make_tuple(new_ptime, td, flag);
    result->set_value( &new_tmstmp );
    return true;
  }
};

struct _fn_utcnow : public base_function
{
  timestamp_t now_timestamp;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    int args_size = args->size();

    if (args_size != 0)
    {
      throw base_s3select_exception("utcnow does not expect any parameters");
    }

    boost::posix_time::ptime now_ptime = boost::posix_time::ptime( boost::posix_time::second_clock::universal_time());
    now_timestamp = std::make_tuple(now_ptime, boost::posix_time::time_duration(0, 0, 0), false);
    result->set_value( &now_timestamp );

    return true;
  }
};

struct _fn_between : public base_function
{

  value res;
  
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    int args_size = args->size();


    if (args_size != 3)
    {
      throw base_s3select_exception("between operates on 3 expressions");//TODO FATAL
    }

    auto iter = args->begin();

    base_statement* second_expr = *iter;
    iter++;    
    base_statement* first_expr = *iter;
    iter++;    
    base_statement* main_expr = *iter;
 
    value second_expr_val = second_expr->eval();
    value first_expr_val = first_expr->eval();
    value main_expr_val = main_expr->eval();

    if ((second_expr_val.type == first_expr_val.type && first_expr_val.type == main_expr_val.type) || (second_expr_val.is_number() && first_expr_val.is_number() && main_expr_val.is_number()))
    {
      if((main_expr_val >= first_expr_val) && (main_expr_val <= second_expr_val)) {
        result->set_value(true);
      } else {
        result->set_value(false);
      }
    }
    return true;
  }
};

struct _fn_not_between : public base_function
{

  value res;
  _fn_between between_op;
  
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    between_op(args,result);
   
    if (result->get_value().is_true() == 0) {
      result->set_value(true);
    } else {
      result->set_value(false);
    }
    return true;
  }
};

static char s3select_ver[10]="41.a";

struct _fn_version : public base_function
{
  value val; //TODO use git to generate sha1
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    val = &s3select_ver[0];
    *result = val;
    return true; 
  }
};

struct _fn_isnull : public base_function
{

  value res;
  
  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();
    base_statement* expr = *iter;
    value expr_val = expr->eval();
    if ( expr_val.is_null()) {
      result->set_value(true);
    } else {
      result->set_value(false);
    }
    return true;
  }
};

struct _fn_is_not_null : public base_function
{
  value res;
  _fn_isnull isnull_op;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
   
    isnull_op(args,result);
 
    if (result->get_value().is_true() == 0)
      result->set_value(true);
    else
      result->set_value(false);

    return true;
  } 
};

struct _fn_in : public base_function
{

  value res;

  bool operator()(bs_stmt_vec_t *args, variable *result) override
  {
    int args_size = static_cast<int>(args->size()-1);
    base_statement *main_expr = (*args)[args_size];
    value main_expr_val = main_expr->eval();
    args_size--;
    while (args_size>=0)
    {
      base_statement *expr = (*args)[args_size];
      value expr_val = expr->eval();
      args_size--;
      if ((expr_val.type == main_expr_val.type) || (expr_val.is_number() && main_expr_val.is_number()))
      {
        if (expr_val == main_expr_val)
        {
          result->set_value(true);
          return true;
        }
      }
    }
    result->set_value(false);
    return true;
  }
};

struct _fn_like : public base_function
{
  value res;
  std::regex compiled_regex;
  bool constant_state;
  value like_expr_val;
  value escape_expr_val;


  explicit _fn_like(base_statement* esc, base_statement* like_expr)
  {
    constant_state = false;

    auto is_constant = [&](base_statement* bs) {
      if (dynamic_cast<variable*>(bs) && dynamic_cast<variable*>(bs)->m_var_type == variable::var_t::COL_VALUE) {
        return true;
      } else {
        return false;
      }
    };

    if (is_constant(esc) && is_constant(like_expr)) {
      constant_state = true;
    }

    if(constant_state == true)
    {
      escape_expr_val = esc->eval();
      like_expr_val = like_expr->eval();

      if (like_expr_val.type != value::value_En_t::STRING)  {
        throw base_s3select_exception("like expression must be string");
      }

      if (escape_expr_val.type != value::value_En_t::STRING)  {
        throw base_s3select_exception("escape expression must be string");
      }

      std::vector<char> like_as_regex = transform(like_expr_val.str(), *escape_expr_val.str());
      std::string like_as_regex_str(like_as_regex.begin(), like_as_regex.end());
      compiled_regex = std::regex(like_as_regex_str);
    }

  }

  std::vector<char> transform(const char* s, char escape)
  {   
  enum  state_expr_t {START , ESCAPE, START_STAR_CHAR , START_METACHAR, START_ANYCHAR , METACHAR, STAR_CHAR, ANYCHAR, END };
  state_expr_t st{START};

  const char *p = s;
  size_t size = strlen(s);
  size_t i = 0;
  std::vector<char> v;
  
  while(*p)
  {
    switch (st)
    {
      case START:
          if(*p == escape) {
          st = ESCAPE;
          v.push_back('^');
          } else if(*p == '%') {
        v.push_back('^');
            v.push_back('.');
            v.push_back('*');
            st = START_STAR_CHAR;
            } else if(*p == '_') {
        v.push_back('^');
        v.push_back('.');
        st=START_METACHAR;
          } else {
        v.push_back('^');
        v.push_back(*p);
        st=START_ANYCHAR;
          } break;

      case START_STAR_CHAR:
          if(*p == escape) {
          st = ESCAPE;
          } else if(*p == '%') {
        st = START_STAR_CHAR;
        } else if(*p == '_') {
        v.push_back('.');
        st = METACHAR;
        } else {
        v.push_back(*p);
        st = ANYCHAR;
        } break;

      case START_METACHAR:
          if(*p == escape) {
          st = ESCAPE;
          } else if(*p == '_') {
        v.push_back('.');
        st = METACHAR;
        } else if(*p == '%') {
        v.push_back('.');
        v.push_back('*');
        st = STAR_CHAR;
        } else {
        v.push_back(*p);
        st = ANYCHAR;
        } break;

      case START_ANYCHAR:
          if(*p == escape) {
          st = ESCAPE;
          } else if(*p == '_' && i == size-1) {
        v.push_back('.');
        v.push_back('$');
        st = END;
        } else if(*p == '_') {
        v.push_back('.');
        st = METACHAR;
        } else if(*p == '%' && i == size-1) {
        v.push_back('.');
        v.push_back('*');
        v.push_back('$');
        st = END;
        } else if(*p == '%') {
        v.push_back('.');
        v.push_back('*');
        st = STAR_CHAR;
        } else  if(i == size-1) {
        v.push_back(*p);
        v.push_back('$');
        st = END;
        } else {
        v.push_back(*p);
        st = ANYCHAR;
        } break;

      case METACHAR:
          if(*p == escape) {
          st = ESCAPE;
          } else if(*p == '_' && i == size-1) {
        v.push_back('.');
        v.push_back('$');
        st = END;
        } else if(*p == '_') {
        v.push_back('.');
        st = METACHAR;
        } else if(*p == '%' && i == size-1) {
        v.push_back('.');
        v.push_back('*');
        v.push_back('$');
        st = END;
        } else if(*p == '%') {
        v.push_back('.');
        v.push_back('*');
        st = STAR_CHAR;
        } else if(i == size-1) {
        v.push_back(*p);
        v.push_back('$');
        st = END;
        } else {
        v.push_back(*p);
        st = ANYCHAR;
        } break;

      case ANYCHAR:
          if(*p == escape) {
          st = ESCAPE;
          } else if(*p == '_' && i == size-1) {
        v.push_back('.');
        v.push_back('$');
        st = END;
        } else if(*p == '_') {
        v.push_back('.');
        st = METACHAR;
        } else if(*p == '%' && i == size-1) {
        v.push_back('.');
        v.push_back('*');
        v.push_back('$');
        st = END;
        } else if(*p == '%') {
        v.push_back('.');
        v.push_back('*');
        st = STAR_CHAR;
        } else  if(i == size-1) {
        v.push_back(*p);
        v.push_back('$');
        st = END;
        } else {
        v.push_back(*p);
        st = ANYCHAR;
        } break;

      case STAR_CHAR:
          if(*p == escape) {
          st = ESCAPE;
          } else if(*p == '%' && i == size-1) {
        v.push_back('$');
        st = END;
        } else if(*p == '%') {
        st = STAR_CHAR;
        } else if(*p == '_' && i == size-1) {
        v.push_back('.');
        v.push_back('$');
        st = END;
        } else if(*p == '_') {
        v.push_back('.');
        st = METACHAR;
        } else  if(i == size-1) {
        v.push_back(*p);
        v.push_back('$');
        st = END;
        } else {
        v.push_back(*p);
        st = ANYCHAR;
        } break;

      case ESCAPE:
        if(i == size-1) {
        v.push_back(*p);
        v.push_back('$');
        st = END;
        } else {
        v.push_back(*p);
        st = ANYCHAR;
        } break;

      case END:
        return v;

      default:
        throw base_s3select_exception("missing state!");
        break;
    }
    p++;
    i++;
  }
  return v;
}

bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();

    base_statement* escape_expr = *iter;
    iter++;
    base_statement* like_expr = *iter;
    iter++;
    base_statement* main_expr = *iter;

    if (constant_state == false){
      like_expr_val = like_expr->eval();
      escape_expr_val = escape_expr->eval();

      if (like_expr_val.type != value::value_En_t::STRING)  {
        throw base_s3select_exception("like expression must be string");
      }

      if (escape_expr_val.type != value::value_En_t::STRING)  {
        throw base_s3select_exception("esacpe expression must be string");
      }

      std::vector<char> like_as_regex = transform(like_expr_val.str(), *escape_expr_val.str());
      std::string like_as_regex_str(like_as_regex.begin(), like_as_regex.end());
      compiled_regex = std::regex(like_as_regex_str);
    }

    value main_expr_val = main_expr->eval();
    if (main_expr_val.type != value::value_En_t::STRING)  {
        throw base_s3select_exception("main expression must be string");
    }
         
    if ( std::regex_match(main_expr_val.to_string(), compiled_regex)) {
      result->set_value(true);
    } else {
      result->set_value(false);
    } 
    return true;
    }
};

struct _fn_substr : public base_function
{

  char buff[4096];// this buffer is persist for the query life time, it use for the results per row(only for the specific function call)
  //it prevent from intensive use of malloc/free (fragmentation).
  //should validate result length.
  //TODO may replace by std::string (dynamic) , or to replace with global allocator , in query scope.
  value v_str;
  value v_from;
  value v_to;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();
    int args_size = args->size();


    if (args_size<2)
    {
      throw base_s3select_exception("substr accept 2 arguments or 3");
    }

    base_statement* str =  *iter;
    iter++;
    base_statement* from = *iter;
    base_statement* to;

    if (args_size == 3)
    {
      iter++;
      to = *iter;
      v_to = to->eval();
      if (!v_to.is_number())
      {
        throw base_s3select_exception("substr third argument must be number");  //can skip row
      }
    }

    v_str = str->eval();

    if(v_str.type != value::value_En_t::STRING)
    {
      throw base_s3select_exception("substr first argument must be string");  //can skip current row
    }

    int str_length = strlen(v_str.str());

    v_from = from->eval();
    if(!v_from.is_number())
    {
      throw base_s3select_exception("substr second argument must be number");  //can skip current row
    }

    int64_t f;
    int64_t t;

    if (v_from.type == value::value_En_t::FLOAT)
    {
      f=v_from.dbl();
    }
    else
    {
      f=v_from.i64();
    }

    if (f <= 0 && args_size == 2)
    {
      f = 1;
    }

    if (f>str_length)
    {
    result->set_value("");
    return true;
    }

    if (str_length>(int)sizeof(buff))
    {
      throw base_s3select_exception("string too long for internal buffer");  //can skip row
    }

    if (args_size == 3)
    {
      if (v_to.type == value::value_En_t::FLOAT)
      {
        t = v_to.dbl();
      }
      else
      {
        t = v_to.i64();
      }

      if (f <= 0)
      {
        t = t + f - 1;
        f = 1;
      }

      if (t<0)
      {
        t = 0;
      }

      if (t > str_length)
      {
        t = str_length;
      }

      if( (str_length-(f-1)-t) <0)
      {//in case the requested length is too long, reduce it to exact length.
        t = str_length-(f-1);
      }

      strncpy(buff, v_str.str()+f-1, t);
    }
    else
    {
      strcpy(buff, v_str.str()+f-1);
    }

    result->set_value(buff);

    return true;
  }
};

struct _fn_charlength : public base_function {

    value v_str;
 
    bool operator()(bs_stmt_vec_t* args, variable* result) override
    {
        auto iter = args->begin();
        base_statement* str =  *iter;
        v_str = str->eval();
        if(v_str.type != value::value_En_t::STRING) {
            throw base_s3select_exception("content is not string!");
        } else {
            int64_t str_length = strlen(v_str.str());
            result->set_value(str_length);         
            return true; 
            }
        }
    };

struct _fn_lower : public base_function {

    std::string buff;
    value v_str;

    bool operator()(bs_stmt_vec_t* args, variable* result) override
    {
        auto iter = args->begin();
        base_statement* str = *iter;
        v_str = str->eval();
        if(v_str.type != value::value_En_t::STRING) {
          throw base_s3select_exception("content is not string");
        } else {
            buff = v_str.str();
            boost::algorithm::to_lower(buff);
            result->set_value(buff.c_str());         
            return true;
        }               
    }
};

struct _fn_upper : public base_function {

    std::string buff;
    value v_str;

    bool operator()(bs_stmt_vec_t* args, variable* result) override
    {
        auto iter = args->begin();
        base_statement* str = *iter;
        v_str = str->eval();
        if(v_str.type != value::value_En_t::STRING) {
          throw base_s3select_exception("content is not string");
        } else {
            buff = v_str.str();
            boost::algorithm::to_upper(buff);
            result->set_value(buff.c_str());         
            return true;
        }               
    }
};

struct _fn_nullif : public base_function {

    value x;
    value y;

    bool operator()(bs_stmt_vec_t* args, variable* result) override
    {
        auto iter = args->begin();

        int args_size = args->size();
        if (args_size != 2)
        {
          throw base_s3select_exception("nullif accept only 2 arguments");
        }
        base_statement *first = *iter;
        x = first->eval();
        iter++;
        base_statement *second = *iter;
        y = second->eval();
        if (x.is_null() && y.is_null())
        {
          result->set_null();
          return true;
        }
        if (x.is_null())
        {
          result->set_null();
          return true;
        }
        if (!(x.is_number() && y.is_number())) {
          if (x.type != y.type) {
            *result = x;
            return true;
          }
        }
        if (x != y) {
          *result = x;
        } else {
          result->set_null();
        }
        return true;
      }
    };

struct _fn_when_then : public base_function {

  value when_value;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();

    base_statement* then_expr = *iter;
    iter ++;

    base_statement* when_expr = *iter;

    when_value = when_expr->eval();
    
    if (when_value.is_true())//true
    {
        *result = then_expr->eval();
        return true;
    }

    result->set_null();

    return true;
  }
};

struct _fn_when_value_then : public base_function {

  value when_value;
  value case_value;
  value then_value;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();

    base_statement* then_expr = *iter;
    iter++;

    base_statement* when_expr = *iter;
    iter++;

    base_statement* case_expr = *iter;

    when_value = when_expr->eval();
    case_value = case_expr->eval();
    then_value = then_expr->eval();
    
    if (case_value == when_value)
    {
        *result = then_value;
        return true;
    }
    
    result->set_null();
    return true;
  }
};

struct _fn_case_when_else : public base_function {

  value when_then_value;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    base_statement* else_expr = *(args->begin());

    size_t args_size = args->size() -1;

    for(int ivec=args_size;ivec>0;ivec--)
    {
      when_then_value = (*args)[ivec]->eval();
      
      if(!when_then_value.is_null())
      {
        *result = when_then_value;
        return true;
      }

    }

    *result = else_expr->eval();
    return true;
  }
};

struct _fn_coalesce : public base_function
{

  value res;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter_begin = args->begin();
    int args_size = args->size();
    while (args_size >= 1)
    {
      base_statement* expr = *iter_begin;
      value expr_val = expr->eval();
      iter_begin++;
      if ( !(expr_val.is_null())) {
          *result = expr_val;
          return true;
        } 
      args_size--;
    }
    result->set_null();
    return true;
  }
};

struct _fn_string : public base_function
{

  value res;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    auto iter = args->begin();

    base_statement* expr = *iter;
    value expr_val = expr->eval();
    result->set_value((expr_val.to_string()));
    return true;
  }
};

struct _fn_to_bool : public base_function
{

  value func_arg;

  bool operator()(bs_stmt_vec_t* args, variable* result) override
  {
    int64_t i=0;
    func_arg = (*args->begin())->eval();

    if (func_arg.type == value::value_En_t::FLOAT)
    {
      i = func_arg.dbl();
    }
    else if (func_arg.type == value::value_En_t::DECIMAL || func_arg.type == value::value_En_t::BOOL)
    {
      i = func_arg.i64();
    }
    else
    {
      i = 0;
    }
    if (i == 0) 
    {
      result->set_value(false);
    }
    else
    {
      result->set_value(true);
    }
    return true;
  }
};

struct _fn_trim : public base_function {

    std::string input_string;
    value v_remove;
    value v_input;

    _fn_trim()
    {
    	v_remove = " "; 
    }

    bool operator()(bs_stmt_vec_t* args, variable* result) override
    {
    	auto iter = args->begin();
    	int args_size = args->size();
    	base_statement* str = *iter;
        v_input = str->eval();
        if(v_input.type != value::value_En_t::STRING) {
            throw base_s3select_exception("content is not string");
        }
        input_string = v_input.str();
        if (args_size == 2) {
        	iter++;
            base_statement* next = *iter;
            v_remove = next->eval();
        }
        boost::trim_right_if(input_string,boost::is_any_of(v_remove.str()));
        boost::trim_left_if(input_string,boost::is_any_of(v_remove.str()));
    	result->set_value(input_string.c_str());
      return true;
    }
}; 

struct _fn_leading : public base_function {

    std::string input_string;
    value v_remove;
    value v_input;

    _fn_leading()
    {
    	v_remove = " "; 
    }

    bool operator()(bs_stmt_vec_t* args, variable* result) override
    {
    	auto iter = args->begin();
    	int args_size = args->size();
    	base_statement* str = *iter;
        v_input = str->eval();
        if(v_input.type != value::value_En_t::STRING) {
            throw base_s3select_exception("content is not string");
        }
        input_string = v_input.str();
        if (args_size == 2) {
        	iter++;
            base_statement* next = *iter;
            v_remove = next->eval();
        }
        boost::trim_left_if(input_string,boost::is_any_of(v_remove.str()));
    	result->set_value(input_string.c_str());
      return true;
    }
}; 

struct _fn_trailing : public base_function {

    std::string input_string;
    value v_remove;
    value v_input;

    _fn_trailing()
    {
    	v_remove = " "; 
    }

    bool operator()(bs_stmt_vec_t* args, variable* result) override
    {
    	auto iter = args->begin();
    	int args_size = args->size();
    	base_statement* str = *iter;
        v_input = str->eval();
        if(v_input.type != value::value_En_t::STRING) {
            throw base_s3select_exception("content is not string");
        }
        input_string = v_input.str();
        if (args_size == 2) {
        	iter++;
            base_statement* next = *iter;
            v_remove = next->eval();
        }
        boost::trim_right_if(input_string,boost::is_any_of(v_remove.str()));
    	result->set_value(input_string.c_str());
      return true;
    }
}; 

base_function* s3select_functions::create(std::string_view fn_name,const bs_stmt_vec_t &arguments)
{
  const FunctionLibrary::const_iterator iter = m_functions_library.find(fn_name.data());

  if (iter == m_functions_library.end())
  {
    std::string msg;
    msg = std::string{fn_name} + " " + " function not found";
    throw base_s3select_exception(msg, base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  switch (iter->second)
  {
  case s3select_func_En_t::ADD:
    return S3SELECT_NEW(this,_fn_add);
    break;

  case s3select_func_En_t::SUM:
    return S3SELECT_NEW(this,_fn_sum);
    break;

  case s3select_func_En_t::COUNT:
    return S3SELECT_NEW(this,_fn_count);
    break;

  case s3select_func_En_t::MIN:
    return S3SELECT_NEW(this,_fn_min);
    break;

  case s3select_func_En_t::MAX:
    return S3SELECT_NEW(this,_fn_max);
    break;

  case s3select_func_En_t::TO_INT:
    return S3SELECT_NEW(this,_fn_to_int);
    break;

  case s3select_func_En_t::TO_FLOAT:
    return S3SELECT_NEW(this,_fn_to_float);
    break;

  case s3select_func_En_t::SUBSTR:
    return S3SELECT_NEW(this,_fn_substr);
    break;

  case s3select_func_En_t::TO_TIMESTAMP:
    return S3SELECT_NEW(this,_fn_to_timestamp);
    break;

  case s3select_func_En_t::TO_STRING_CONSTANT:
    return S3SELECT_NEW(this,_fn_to_string_constant);
    break;

  case s3select_func_En_t::TO_STRING_DYNAMIC:
    return S3SELECT_NEW(this,_fn_to_string_dynamic);
    break;

  case s3select_func_En_t::TO_BOOL:
    return S3SELECT_NEW(this,_fn_to_bool);
    break;

  case s3select_func_En_t::EXTRACT_YEAR:
    return S3SELECT_NEW(this,_fn_extract_year_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_MONTH:
    return S3SELECT_NEW(this,_fn_extract_month_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_DAY:
    return S3SELECT_NEW(this,_fn_extract_day_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_HOUR:
    return S3SELECT_NEW(this,_fn_extract_hour_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_MINUTE:
    return S3SELECT_NEW(this,_fn_extract_minute_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_SECOND:
    return S3SELECT_NEW(this,_fn_extract_second_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_WEEK:
    return S3SELECT_NEW(this,_fn_extract_week_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_TIMEZONE_HOUR:
    return S3SELECT_NEW(this,_fn_extract_tz_hour_from_timestamp);
    break;

  case s3select_func_En_t::EXTRACT_TIMEZONE_MINUTE:
    return S3SELECT_NEW(this,_fn_extract_tz_minute_from_timestamp);
    break;

  case s3select_func_En_t::DATE_ADD_YEAR:
    return S3SELECT_NEW(this,_fn_add_year_to_timestamp);
    break;

  case s3select_func_En_t::DATE_ADD_MONTH:
    return S3SELECT_NEW(this,_fn_add_month_to_timestamp);
    break;

  case s3select_func_En_t::DATE_ADD_DAY:
    return S3SELECT_NEW(this,_fn_add_day_to_timestamp);
    break;

  case s3select_func_En_t::DATE_ADD_HOUR:
    return S3SELECT_NEW(this,_fn_add_hour_to_timestamp);
    break;

  case s3select_func_En_t::DATE_ADD_MINUTE:
    return S3SELECT_NEW(this,_fn_add_minute_to_timestamp);
    break;

  case s3select_func_En_t::DATE_ADD_SECOND:
    return S3SELECT_NEW(this,_fn_add_second_to_timestamp);
    break;

  case s3select_func_En_t::DATE_DIFF_YEAR:
    return S3SELECT_NEW(this,_fn_diff_year_timestamp);
    break;

  case s3select_func_En_t::DATE_DIFF_MONTH:
    return S3SELECT_NEW(this,_fn_diff_month_timestamp);
    break;

  case s3select_func_En_t::DATE_DIFF_DAY:
    return S3SELECT_NEW(this,_fn_diff_day_timestamp);
    break;

  case s3select_func_En_t::DATE_DIFF_HOUR:
    return S3SELECT_NEW(this,_fn_diff_hour_timestamp);
    break;

  case s3select_func_En_t::DATE_DIFF_MINUTE:
    return S3SELECT_NEW(this,_fn_diff_minute_timestamp);
    break;

  case s3select_func_En_t::DATE_DIFF_SECOND:
    return S3SELECT_NEW(this,_fn_diff_second_timestamp);
    break;

  case s3select_func_En_t::UTCNOW:
    return S3SELECT_NEW(this,_fn_utcnow);
    break;

  case s3select_func_En_t::AVG:
    return S3SELECT_NEW(this,_fn_avg);
    break;

  case s3select_func_En_t::LOWER:
    return S3SELECT_NEW(this,_fn_lower);
    break;

  case s3select_func_En_t::UPPER:
    return S3SELECT_NEW(this,_fn_upper);
    break;

  case s3select_func_En_t::LENGTH:
    return S3SELECT_NEW(this,_fn_charlength);
    break; 

  case s3select_func_En_t::BETWEEN:
    return S3SELECT_NEW(this,_fn_between);
    break;

  case s3select_func_En_t::NOT_BETWEEN:
    return S3SELECT_NEW(this,_fn_not_between);
    break;

  case s3select_func_En_t::IS_NULL:
    return S3SELECT_NEW(this,_fn_isnull);
    break;

  case s3select_func_En_t::IS_NOT_NULL:
    return S3SELECT_NEW(this,_fn_is_not_null);
    break;

  case s3select_func_En_t::IN:
    return S3SELECT_NEW(this,_fn_in);
    break;

  case s3select_func_En_t::VERSION:
    return S3SELECT_NEW(this,_fn_version);
    break;

  case s3select_func_En_t::NULLIF:
    return S3SELECT_NEW(this,_fn_nullif);
    break;

  case s3select_func_En_t::LIKE:
    return S3SELECT_NEW(this,_fn_like,arguments[0],arguments[1]);
    break;

  case s3select_func_En_t::COALESCE:
    return S3SELECT_NEW(this,_fn_coalesce);
    break;

  case s3select_func_En_t::WHEN_THEN:
    return S3SELECT_NEW(this,_fn_when_then);
    break;

  case s3select_func_En_t::WHEN_VALUE_THEN:
    return S3SELECT_NEW(this,_fn_when_value_then);
    break;

  case s3select_func_En_t::CASE_WHEN_ELSE:
    return S3SELECT_NEW(this,_fn_case_when_else);
    break;

  case s3select_func_En_t::STRING:
    return S3SELECT_NEW(this,_fn_string);
    break;

  case s3select_func_En_t::TRIM:  
    return S3SELECT_NEW(this,_fn_trim);
    break;

  case s3select_func_En_t::LEADING:  
    return S3SELECT_NEW(this,_fn_leading);
    break;

  case s3select_func_En_t::TRAILING:  
    return S3SELECT_NEW(this,_fn_trailing);
    break;

  default:
    throw base_s3select_exception("internal error while resolving function-name");
    break;
  }
}

bool base_statement::is_function() const
{
  if (dynamic_cast<__function*>(const_cast<base_statement*>(this)))
  {
    return true;
  }
  else
  {
    return false;
  }
}

const base_statement* base_statement::get_aggregate() const
{
  //search for aggregation function in AST
  const base_statement* res = 0;

  if (is_aggregate())
  {
    return this;
  }

  if (left() && (res=left()->get_aggregate())!=0)
  {
    return res;
  }

  if (right() && (res=right()->get_aggregate())!=0)
  {
    return res;
  }

  if (is_function())
  {
    for (auto i : dynamic_cast<__function*>(const_cast<base_statement*>(this))->get_arguments())
    {
      const base_statement* b=i->get_aggregate();
      if (b)
      {
        return b;
      }
    }
  }
  return 0;
}

bool base_statement::is_column_reference() const
{
  if(is_column())
    return true;
  
  if(left())
    return left()->is_column_reference();

  if(right())
    return right()->is_column_reference();

  if(is_function())
  {
    for(auto a : dynamic_cast<__function*>(const_cast<base_statement*>(this))->get_arguments())
    {
      if(a->is_column_reference())
        return true;
    }
  }

  return false;
}

bool base_statement::is_nested_aggregate(bool &aggr_flow) const
{
  if (is_aggregate())
  {
      aggr_flow=true;
      for (auto& i : dynamic_cast<__function*>(const_cast<base_statement*>(this))->get_arguments())
      {
        if (i->get_aggregate() != nullptr)
        {
          return true;
        }
      }
  }

  if(left() && left()->is_nested_aggregate(aggr_flow))
    return true;
  
  if(right() && right()->is_nested_aggregate(aggr_flow))
    return true;

  if (is_function())
  {
      for (auto& i : dynamic_cast<__function*>(const_cast<base_statement*>(this))->get_arguments())
      {
        if (i->get_aggregate() != nullptr)
        {
          return i->is_nested_aggregate(aggr_flow);
        }
      }
  }

  return false;
}

bool base_statement::mark_aggreagtion_subtree_to_execute()
{//purpase:: set aggregation subtree as runnable.
 //the function search for aggregation function, and mark its subtree {skip = false}
  if (is_aggregate())
    set_skip_non_aggregate(false);
  
  if (left())
    left()->mark_aggreagtion_subtree_to_execute();
  
  if(right())
    right()->mark_aggreagtion_subtree_to_execute();

  if (is_function())
  {
      for (auto& i : dynamic_cast<__function*>(this)->get_arguments())
      {
          i->mark_aggreagtion_subtree_to_execute();
      }
  }

  return true;
}

#ifdef _ARROW_EXIST
void base_statement::extract_columns(parquet_file_parser::column_pos_t &cols,const uint16_t max_columns)
{// purpose: to extract all column-ids from query
  if(is_column()) //column reference or column position
  {variable* v = dynamic_cast<variable*>(this);
    if(dynamic_cast<variable*>(this)->m_var_type == variable::var_t::VAR)
    {//column reference 

      if (v->getScratchArea()->get_column_pos(v->get_name().c_str())>=0)
      {//column belong to schema
        cols.insert( v->getScratchArea()->get_column_pos(v->get_name().c_str() ));
      }else {
        if(v->getAlias()->search_alias(v->get_name()))
        {//column is an alias --> extract columns belong to alias
	      //TODO cyclic alias to resolve
          v->getAlias()->search_alias(v->get_name())->extract_columns(cols,max_columns);
        }else {
          //column is not alias --> error
          std::stringstream ss;
          ss << "column " + v->get_name() + " is not part of schema nor an alias";
          throw base_s3select_exception(ss.str(),base_s3select_exception::s3select_exp_en_t::FATAL);
        }
      }
    }else if(v->m_var_type == variable::var_t::STAR_OPERATION)
    {
      for(uint16_t i=0;i<max_columns;i++)
      {//push all columns
        cols.insert( i );
      }
    }
    else {
      if (v->get_column_pos()>=max_columns)
      {
        std::stringstream ss;
        ss << "column " + std::to_string( v->get_column_pos()+1 ) + " exceed max number of columns";
        throw base_s3select_exception(ss.str(),base_s3select_exception::s3select_exp_en_t::FATAL);
      }
      cols.insert(v->get_column_pos());//push column positions 
    }
  }else if(is_function())
  {
    __function* f = (dynamic_cast<__function*>(this));
    bs_stmt_vec_t args = f->get_arguments();
    for (auto prm : args)
    {//traverse function args
      prm->extract_columns(cols,max_columns);
    }
    
  }

  //keep traversing down the AST
  if(left())
    left()->extract_columns(cols,max_columns);
  
  if(right())
    right()->extract_columns(cols,max_columns);
}
#endif //_ARROW_EXIST

} //namespace s3selectEngine

#endif
