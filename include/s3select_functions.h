#ifndef __S3SELECT_FUNCTIONS__
#define __S3SELECT_FUNCTIONS__


#include "s3select_oper.h"
#include <boost/algorithm/string.hpp>

#define BOOST_BIND_ACTION_PARAM( push_name ,param ) boost::bind( &push_name::operator(), g_ ## push_name , _1 ,_2, param)
namespace s3selectEngine
{

struct push_2dig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    *n = ((char)(*a) - 48) *10 + ((char)*(a+1)-48) ;
  }

};
static push_2dig g_push_2dig;

struct push_4dig
{
  void operator()(const char* a, const char* b, uint32_t* n) const
  {
    *n = ((char)(*a) - 48) *1000 + ((char)*(a+1)-48)*100 + ((char)*(a+2)-48)*10  + ((char)*(a+3)-48);
  }

};
static push_4dig g_push_4dig;

enum class s3select_func_En_t {ADD,
                               SUM,
                               AVG,
                               MIN,
                               MAX,
                               COUNT,
                               TO_INT,
                               TO_FLOAT,
                               TO_TIMESTAMP,
                               SUBSTR,
                               EXTRACT,
                               DATE_ADD,
                               DATE_DIFF,
                               UTCNOW,
                               LENGTH,
                               LOWER,
                               UPPER,
                               VERSION,
                               NULLIF
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
    {"substr", s3select_func_En_t::SUBSTR},
    {"timestamp", s3select_func_En_t::TO_TIMESTAMP},
    {"extract", s3select_func_En_t::EXTRACT},
    {"dateadd", s3select_func_En_t::DATE_ADD},
    {"datediff", s3select_func_En_t::DATE_DIFF},
    {"utcnow", s3select_func_En_t::UTCNOW},
    {"charlength", s3select_func_En_t::LENGTH},
    {"characterlength", s3select_func_En_t::LENGTH},
    {"lower", s3select_func_En_t::LOWER},
    {"upper", s3select_func_En_t::UPPER},
    {"version", s3select_func_En_t::VERSION},
    {"nullif", s3select_func_En_t::NULLIF}
  };

public:

  base_function* create(std::string fn_name);

  s3select_functions():m_s3select_allocator(0)
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
  std::string name;
  base_function* m_func_impl;
  s3select_functions* m_s3select_functions;
  variable m_result;

  void _resolve_name()
  {
    if (m_func_impl)
    {
      return;
    }

    base_function* f = m_s3select_functions->create(name);
    if (!f)
    {
      throw base_s3select_exception("function not found", base_s3select_exception::s3select_exp_en_t::FATAL);  //should abort query
    }
    m_func_impl = f;
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

  virtual void traverse_and_apply(scratch_area* sa, projection_alias* pa)
  {
    m_scratch = sa;
    m_aliases = pa;
    for (base_statement* ba : arguments)
    {
      ba->traverse_and_apply(sa, pa);
    }
  }

  virtual bool is_aggregate() // TODO under semantic flow
  {
    _resolve_name();

    return m_func_impl->is_aggregate();
  }

  virtual bool semantic()
  {
    return true;
  }

  __function(const char* fname, s3select_functions* s3f) : name(fname), m_func_impl(0), m_s3select_functions(s3f) {}

  virtual value& eval()
  {

    _resolve_name();

    if (is_last_call == false)
    {
      (*m_func_impl)(&arguments, &m_result);
    }
    else
    {
      (*m_func_impl).get_aggregate_result(&m_result);
    }

    return m_result.get_value();
  }



  virtual std::string  print(int ident)
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

  virtual ~__function() {}
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

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
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

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
    base_statement* x = *iter;

    try
    {
      sum = sum + x->eval();
    }
    catch (base_s3select_exception& e)
    {
      std::cout << "illegal value for aggregation(sum). skipping." << std::endl;
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL)
      {
        throw;
      }
    }

    return true;
  }

  virtual void get_aggregate_result(variable* result)
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

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    count += 1;

    return true;
  }

  virtual void get_aggregate_result(variable* result)
  {
    result->set_value(count);
  }

};

struct _fn_avg : public base_function
{

    value sum;
    value count{0.0};

    _fn_avg() : sum(0) { aggregate = true; }

    bool operator()(bs_stmt_vec_t* args, variable *result)
    {
        bs_stmt_vec_t::iterator iter = args->begin();
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

    virtual void get_aggregate_result(variable *result)
    {
        if(count == 0) {
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

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
    base_statement* x =  *iter;

    if(min > x->eval())
    {
      min=x->eval();
    }

    return true;
  }

  virtual void get_aggregate_result(variable* result)
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

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
    base_statement* x =  *iter;

    if(max < x->eval())
    {
      max=x->eval();
    }

    return true;
  }

  virtual void get_aggregate_result(variable* result)
  {
    *result = max;
  }

};

struct _fn_to_int : public base_function
{

  value var_result;
  value func_arg;

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    char* perr;
    int64_t i=0;
    func_arg = (*args->begin())->eval();

    if (func_arg.type == value::value_En_t::STRING)
    {
      i = strtol(func_arg.str(), &perr, 10) ;  //TODO check error before constructor
    }
    else if (func_arg.type == value::value_En_t::FLOAT)
    {
      i = func_arg.dbl();
    }
    else
    {
      i = func_arg.i64();
    }

    var_result =  i ;
    *result =  var_result;

    return true;
  }

};

struct _fn_to_float : public base_function
{

  value var_result;
  value v_from;

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    char* perr;
    double d=0;
    value v = (*args->begin())->eval();

    if (v.type == value::value_En_t::STRING)
    {
      d = strtod(v.str(), &perr) ;  //TODO check error before constructor
    }
    else if (v.type == value::value_En_t::FLOAT)
    {
      d = v.dbl();
    }
    else
    {
      d = v.i64();
    }

    var_result = d;
    *result = var_result;

    return true;
  }

};

struct _fn_to_timestamp : public base_function
{
  bsc::rule<> separator = bsc::ch_p(":") | bsc::ch_p("-");

  uint32_t yr = 1700, mo = 1, dy = 1;
  bsc::rule<> dig4 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p >> bsc::digit_p >> bsc::digit_p];
  bsc::rule<> dig2 = bsc::lexeme_d[bsc::digit_p >> bsc::digit_p];

  bsc::rule<> d_yyyymmdd_dig = ((dig4[BOOST_BIND_ACTION_PARAM(push_4dig, &yr)]) >> *(separator)
                                >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &mo)]) >> *(separator)
                                >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &dy)]) >> *(separator));

  uint32_t hr = 0, mn = 0, sc = 0;
  bsc::rule<> d_time_dig = ((dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &hr)]) >> *(separator)
                            >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &mn)]) >> *(separator)
                            >> (dig2[BOOST_BIND_ACTION_PARAM(push_2dig, &sc)]) >> *(separator));

  boost::posix_time::ptime new_ptime;

  value v_str;


  bool datetime_validation()
  {
    //TODO temporary , should check for leap year

    if(yr<1700 || yr>2050)
    {
      return false;
    }
    if (mo<1 || mo>12)
    {
      return false;
    }
    if (dy<1 || dy>31)
    {
      return false;
    }
    if (hr>23)
    {
      return false;
    }
    if (dy>59)
    {
      return false;
    }
    if (sc>59)
    {
      return false;
    }

    return true;
  }

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {

    hr = 0;
    mn = 0;
    sc = 0;

    bs_stmt_vec_t::iterator iter = args->begin();
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

    bsc::parse_info<> info_dig = bsc::parse(v_str.str(), d_yyyymmdd_dig >> *(separator) >> d_time_dig);

    if(datetime_validation()==false or !info_dig.full)
    {
      throw base_s3select_exception("input date-time is illegal");
    }

    new_ptime = boost::posix_time::ptime(boost::gregorian::date(yr, mo, dy),
                                         boost::posix_time::hours(hr) + boost::posix_time::minutes(mn) + boost::posix_time::seconds(sc));

    result->set_value(&new_ptime);

    return true;
  }

};

struct _fn_extact_from_timestamp : public base_function
{

  boost::posix_time::ptime new_ptime;

  value val_date_part;

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
    int args_size = args->size();

    if (args_size < 2)
    {
      throw base_s3select_exception("to_timestamp should have 2 parameters");
    }

    base_statement* date_part = *iter;

    val_date_part = date_part->eval();//TODO could be done once?

    if(val_date_part.is_string()== false)
    {
      throw base_s3select_exception("first parameter should be string");
    }

    iter++;

    base_statement* ts = *iter;

    if(ts->eval().is_timestamp()== false)
    {
      throw base_s3select_exception("second parameter is not timestamp");
    }

    new_ptime = *ts->eval().timestamp();

    if( strcmp(val_date_part.str(), "year")==0 )
    {
      result->set_value( (int64_t)new_ptime.date().year() );
    }
    else if( strcmp(val_date_part.str(), "month")==0 )
    {
      result->set_value( (int64_t)new_ptime.date().month() );
    }
    else if( strcmp(val_date_part.str(), "day")==0 )
    {
      result->set_value( (int64_t)new_ptime.date().day_of_year() );
    }
    else if( strcmp(val_date_part.str(), "week")==0 )
    {
      result->set_value( (int64_t)new_ptime.date().week_number() );
    }
    else
    {
      throw base_s3select_exception(std::string( val_date_part.str() + std::string("  is not supported ") ).c_str() );
    }

    return true;
  }

};

struct _fn_diff_timestamp : public base_function
{

  value val_date_part;
  value val_dt1;
  value val_dt2;

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
    int args_size = args->size();

    if (args_size < 3)
    {
      throw base_s3select_exception("datediff need 3 parameters");
    }

    base_statement* date_part = *iter;

    val_date_part = date_part->eval();

    iter++;
    base_statement* dt1_param = *iter;
    val_dt1 = dt1_param->eval();
    if (val_dt1.is_timestamp() == false)
    {
      throw base_s3select_exception("second parameter should be timestamp");
    }

    iter++;
    base_statement* dt2_param = *iter;
    val_dt2 = dt2_param->eval();
    if (val_dt2.is_timestamp() == false)
    {
      throw base_s3select_exception("third parameter should be timestamp");
    }

    if (strcmp(val_date_part.str(), "year") == 0)
    {
      int64_t yr = val_dt2.timestamp()->date().year() - val_dt1.timestamp()->date().year() ;
      result->set_value( yr );
    }
    else if (strcmp(val_date_part.str(), "month") == 0)
    {
      int64_t yr = val_dt2.timestamp()->date().year() - val_dt1.timestamp()->date().year() ;
      int64_t mon = val_dt2.timestamp()->date().month() - val_dt1.timestamp()->date().month() ;

      result->set_value( yr*12 + mon );
    }
    else if (strcmp(val_date_part.str(), "day") == 0)
    {
      boost::gregorian::date_period dp =
        boost::gregorian::date_period( val_dt1.timestamp()->date(), val_dt2.timestamp()->date());
      result->set_value( dp.length().days() );
    }
    else if (strcmp(val_date_part.str(), "hours") == 0)
    {
      boost::posix_time::time_duration td_res = (*val_dt2.timestamp() - *val_dt1.timestamp());
      result->set_value( td_res.hours());
    }
    else
    {
      throw base_s3select_exception("first parameter should be string: year,month,hours,day");
    }


    return true;
  }
};

struct _fn_add_to_timestamp : public base_function
{

  boost::posix_time::ptime new_ptime;

  value val_date_part;
  value val_quantity;
  value val_timestamp;

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
    int args_size = args->size();

    if (args_size < 3)
    {
      throw base_s3select_exception("add_to_timestamp should have 3 parameters");
    }

    base_statement* date_part = *iter;
    val_date_part = date_part->eval();//TODO could be done once?

    if(val_date_part.is_string()== false)
    {
      throw base_s3select_exception("first parameter should be string");
    }

    iter++;
    base_statement* quan = *iter;
    val_quantity = quan->eval();

    if (val_quantity.is_number() == false)
    {
      throw base_s3select_exception("second parameter should be number");  //TODO what about double?
    }

    iter++;
    base_statement* ts = *iter;
    val_timestamp = ts->eval();

    if(val_timestamp.is_timestamp() == false)
    {
      throw base_s3select_exception("third parameter should be time-stamp");
    }

    new_ptime = *val_timestamp.timestamp();

    if( strcmp(val_date_part.str(), "year")==0 )
    {
      new_ptime += boost::gregorian::years( val_quantity.i64() );
      result->set_value( &new_ptime );
    }
    else if( strcmp(val_date_part.str(), "month")==0 )
    {
      new_ptime += boost::gregorian::months( val_quantity.i64() );
      result->set_value( &new_ptime );
    }
    else if( strcmp(val_date_part.str(), "day")==0 )
    {
      new_ptime += boost::gregorian::days( val_quantity.i64() );
      result->set_value( &new_ptime );
    }
    else
    {
      throw base_s3select_exception( std::string(val_date_part.str() + std::string(" is not supported for add")).c_str());
    }

    return true;
  }

};

struct _fn_utcnow : public base_function
{

  boost::posix_time::ptime now_ptime;

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    int args_size = args->size();

    if (args_size != 0)
    {
      throw base_s3select_exception("utcnow does not expect any parameters");
    }

    now_ptime = boost::posix_time::ptime( boost::posix_time::second_clock::universal_time());
    result->set_value( &now_ptime );

    return true;
  }
};

static char s3select_ver[10]="41.a";

struct _fn_version : public base_function
{
  value val; //TODO use git to generate sha1
  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    val = &s3select_ver[0];
    *result = val;
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

  bool operator()(bs_stmt_vec_t* args, variable* result)
  {
    bs_stmt_vec_t::iterator iter = args->begin();
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
    }

    v_str = str->eval();

    if(v_str.type != value::value_En_t::STRING)
    {
      throw base_s3select_exception("substr first argument must be string");  //can skip current row
    }

    int str_length = strlen(v_str.str());

    v_from = from->eval();
    if(v_from.is_string())
    {
      throw base_s3select_exception("substr second argument must be number");  //can skip current row
    }

    int64_t f;
    int64_t t;

    if (args_size==3)
    {
      v_to = to->eval();
      if (v_to.is_string())
      {
        throw base_s3select_exception("substr third argument must be number");  //can skip row
      }
    }

    if (v_from.type == value::value_En_t::FLOAT)
    {
      f=v_from.dbl();
    }
    else
    {
      f=v_from.i64();
    }

    if (f>str_length)
    {
      throw base_s3select_exception("substr start position is too far");  //can skip row
    }

    if (str_length>(int)sizeof(buff))
    {
      throw base_s3select_exception("string too long for internal buffer");  //can skip row
    }

    if (args_size == 3)
    {
      if (v_from.type == value::value_En_t::FLOAT)
      {
        t = v_to.dbl();
      }
      else
      {
        t = v_to.i64();
      }

      if( (str_length-(f-1)-t) <0)
      {
        throw base_s3select_exception("substr length parameter beyond bounderies");  //can skip row
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
 
    bool operator()(bs_stmt_vec_t* args, variable* result)
    {
        bs_stmt_vec_t::iterator iter = args->begin();
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

    bool operator()(bs_stmt_vec_t* args, variable* result)
    {
        bs_stmt_vec_t::iterator iter = args->begin();
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

    bool operator()(bs_stmt_vec_t* args, variable* result)
    {
        bs_stmt_vec_t::iterator iter = args->begin();
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

    bool operator()(bs_stmt_vec_t* args, variable* result)
    {
        bs_stmt_vec_t::iterator iter = args->begin();

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

base_function* s3select_functions::create(std::string fn_name)
{
  const FunctionLibrary::const_iterator iter = m_functions_library.find(fn_name);

  if (iter == m_functions_library.end())
  {
    std::string msg;
    msg = fn_name + " " + " function not found";
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

  case s3select_func_En_t::EXTRACT:
    return S3SELECT_NEW(this,_fn_extact_from_timestamp);
    break;

  case s3select_func_En_t::DATE_ADD:
    return S3SELECT_NEW(this,_fn_add_to_timestamp);
    break;

  case s3select_func_En_t::DATE_DIFF:
    return S3SELECT_NEW(this,_fn_diff_timestamp);
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

  case s3select_func_En_t::VERSION:
    return S3SELECT_NEW(this,_fn_version);
    break;

  case s3select_func_En_t::NULLIF:
    return S3SELECT_NEW(this,_fn_nullif);
    break;

  default:
    throw base_s3select_exception("internal error while resolving function-name");
    break;
  }
}

bool base_statement::is_function()
{
  if (dynamic_cast<__function*>(this))
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool base_statement::is_aggregate_exist_in_expression(base_statement* e) //TODO obsolete ?
{
  if (e->is_aggregate())
  {
    return true;
  }

  if (e->left() && e->left()->is_aggregate_exist_in_expression(e->left()))
  {
    return true;
  }

  if (e->right() && e->right()->is_aggregate_exist_in_expression(e->right()))
  {
    return true;
  }

  if (e->is_function())
  {
    for (auto i : dynamic_cast<__function*>(e)->get_arguments())
      if (e->is_aggregate_exist_in_expression(i))
      {
        return true;
      }
  }

  return false;
}

base_statement* base_statement::get_aggregate()
{
  //search for aggregation function in AST
  base_statement* res = 0;

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
    for (auto i : dynamic_cast<__function*>(this)->get_arguments())
    {
      base_statement* b=i->get_aggregate();
      if (b)
      {
        return b;
      }
    }
  }
  return 0;
}

bool base_statement::is_nested_aggregate(base_statement* e)
{
  //validate for non nested calls for aggregation function, i.e. sum ( min ( ))
  if (e->is_aggregate())
  {
    if (e->left())
    {
      if (e->left()->is_aggregate_exist_in_expression(e->left()))
      {
        return true;
      }
    }
    else if (e->right())
    {
      if (e->right()->is_aggregate_exist_in_expression(e->right()))
      {
        return true;
      }
    }
    else if (e->is_function())
    {
      for (auto i : dynamic_cast<__function*>(e)->get_arguments())
      {
        if (i->is_aggregate_exist_in_expression(i))
        {
          return true;
        }
      }
    }
    return false;
  }
  return false;
}

// select sum(c2) ... + c1 ... is not allowed. a binary operation with scalar is OK. i.e. select sum() + 1
bool base_statement::is_binop_aggregate_and_column(base_statement* skip_expression)
{
  if (left() && left() != skip_expression) //can traverse to left
  {
    if (left()->is_column())
    {
      return true;
    }
    else if (left()->is_binop_aggregate_and_column(skip_expression) == true)
    {
      return true;
    }
  }

  if (right() && right() != skip_expression) //can traverse right
  {
    if (right()->is_column())
    {
      return true;
    }
    else if (right()->is_binop_aggregate_and_column(skip_expression) == true)
    {
      return true;
    }
  }

  if (this != skip_expression && is_function())
  {

    __function* f = (dynamic_cast<__function*>(this));
    bs_stmt_vec_t l = f->get_arguments();
    for (auto i : l)
    {
      if (i!=skip_expression && i->is_column())
      {
        return true;
      }
      if (i->is_binop_aggregate_and_column(skip_expression) == true)
      {
        return true;
      }
    }
  }

  return false;
}

} //namespace s3selectEngine

#endif
