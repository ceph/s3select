#ifndef __S3SELECT_OPER__
#define __S3SELECT_OPER__

#include <string>
#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>

#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/bind.hpp>
namespace bsc = BOOST_SPIRIT_CLASSIC_NS;

namespace s3selectEngine
{

//=== stl allocator definition
//this allocator is fit for placement new (no calls to heap)

class chunkalloc_out_of_mem
{
};

template <typename T, size_t pool_sz>
class ChunkAllocator : public std::allocator<T>
{
public:
  typedef size_t size_type;
  typedef T* pointer;
  size_t buffer_capacity;
  char* buffer_ptr;

  //only ONE pool,not allocated dynamically; main assumption, caller knows in advance its memory limitations.
  char buffer[pool_sz];

  template <typename _Tp1>
  struct rebind
  {
    typedef ChunkAllocator<_Tp1, pool_sz> other;
  };

  //==================================
  inline T* _Allocate(size_t num_of_element, T*)
  {
    // allocate storage for _Count elements of type T

    pointer res = (pointer)(buffer_ptr + buffer_capacity);

    buffer_capacity+= sizeof(T) * num_of_element;
    
    size_t addr_alignment = (buffer_capacity % sizeof(char*));
    buffer_capacity += addr_alignment != 0 ? sizeof(char*) - addr_alignment : 0;

    if (buffer_capacity> sizeof(buffer))
    {
      throw chunkalloc_out_of_mem();
    }

    return res;
  }

  //==================================
  inline pointer allocate(size_type n,  [[maybe_unused]] const void* hint = 0)
  {
    return (_Allocate(n, (pointer)0));
  }

  //==================================
  inline void deallocate(pointer p, size_type n)
  {
  }

  //==================================
  ChunkAllocator() noexcept : std::allocator<T>()
  {
    // alloc from main-buffer
    buffer_capacity = 0;
    memset( &buffer[0], 0, sizeof(buffer));
    buffer_ptr = &buffer[0];
  }

  //==================================
  ChunkAllocator(const ChunkAllocator& other) noexcept : std::allocator<T>(other)
  {
    // copy const
    buffer_capacity = 0;
    buffer_ptr = &buffer[0];
  }

  //==================================
  ~ChunkAllocator() noexcept
  {
    //do nothing
  }
};

class base_statement;
//typedef std::vector<base_statement *> bs_stmt_vec_t; //without specific allocator

//ChunkAllocator, prevent allocation from heap.
typedef std::vector<base_statement*, ChunkAllocator<base_statement*, 256> > bs_stmt_vec_t;

class base_s3select_exception
{

public:
  enum class s3select_exp_en_t
  {
    NONE,
    ERROR,
    FATAL
  } ;

private:
  s3select_exp_en_t m_severity;

public:
  std::string _msg;
  explicit base_s3select_exception(const char* n) : m_severity(s3select_exp_en_t::NONE)
  {
    _msg.assign(n);
  }
  base_s3select_exception(const char* n, s3select_exp_en_t severity) : m_severity(severity)
  {
    _msg.assign(n);
  }
  base_s3select_exception(std::string n, s3select_exp_en_t severity) : m_severity(severity)
  {
    _msg = n;
  }

  virtual const char* what()
  {
    return _msg.c_str();
  }

  s3select_exp_en_t severity()
  {
    return m_severity;
  }

  virtual ~base_s3select_exception() = default;
};



class s3select_allocator //s3select is the "owner"
{
private:

  std::vector<char*> list_of_buff;
  u_int32_t m_idx;

#define __S3_ALLOCATION_BUFF__ (8*1024)
  void check_capacity(size_t sz)
  {
    if (sz>__S3_ALLOCATION_BUFF__)
    {
      throw base_s3select_exception("requested size too big", base_s3select_exception::s3select_exp_en_t::FATAL);
    }

    if ((m_idx + sz) >= __S3_ALLOCATION_BUFF__)
    {
      list_of_buff.push_back((char*)malloc(__S3_ALLOCATION_BUFF__));
      m_idx = 0;
    }
  }

  void inc(size_t sz)
  {
    m_idx += sz;
    m_idx += sizeof(char*) - (m_idx % sizeof(char*)); //alignment
  }

public:
  s3select_allocator():m_idx(0)
  {
    list_of_buff.push_back((char*)malloc(__S3_ALLOCATION_BUFF__));
  }

  void *alloc(size_t sz)
  {
    check_capacity(sz);

    char* buff = list_of_buff.back();

    u_int32_t idx = m_idx;
   
    inc(sz);
 
    return &buff[ idx ];
  }

  virtual ~s3select_allocator()
  {
    for(auto b : list_of_buff)
    {
      free(b);
    }
  }
};

// placement new for allocation of all s3select objects on single(or few) buffers, deallocation of those objects is by releasing the buffer.
#define S3SELECT_NEW(self, type , ... ) [=]() \
        {   \
            auto res=new (self->getAllocator()->alloc(sizeof(type))) type(__VA_ARGS__); \
            return res; \
        }();

class scratch_area
{

private:
  std::vector<std::string_view> m_columns{128};
  int m_upper_bound;

  std::vector<std::pair<std::string, int >> m_column_name_pos;

public:

  void set_column_pos(const char* n, int pos)//TODO use std::string
  {
    m_column_name_pos.push_back( std::pair<const char*, int>(n, pos));
  }

  void update(std::vector<char*>& tokens, size_t num_of_tokens)
  {
    std::copy_n(tokens.begin(), num_of_tokens, m_columns.begin());
    m_upper_bound = num_of_tokens;
  }

  int get_column_pos(const char* n)
  {
    //done only upon building the AST, not on "runtime"

    for( auto iter : m_column_name_pos)
    {
      if (!strcmp(iter.first.c_str(), n))
      {
        return iter.second;
      }
    }

    return -1;
  }

  std::string_view get_column_value(int column_pos)
  {
    if ((column_pos >= m_upper_bound) || column_pos < 0)
    {
      throw base_s3select_exception("column_position_is_wrong", base_s3select_exception::s3select_exp_en_t::ERROR);
    }

    return m_columns[column_pos];
  }

  int get_num_of_columns()
  {
    return m_upper_bound;
  }
};

class s3select_reserved_word
{
  public:

  enum class reserve_word_en_t
  {
    NA,
    S3S_NULL,//TODO check AWS defintions for reserve words, its a long list , what about functions-names? 
    S3S_NAN,
    S3S_TRUE,
    S3S_FALSE
  } ;

  using reserved_words = std::map<std::string,reserve_word_en_t>;

  const reserved_words m_reserved_words=
  {
    {"null",reserve_word_en_t::S3S_NULL},{"NULL",reserve_word_en_t::S3S_NULL},
    {"nan",reserve_word_en_t::S3S_NAN},{"NaN",reserve_word_en_t::S3S_NAN},
    {"true",reserve_word_en_t::S3S_TRUE},{"TRUE",reserve_word_en_t::S3S_TRUE},
    {"false",reserve_word_en_t::S3S_FALSE},{"FALSE",reserve_word_en_t::S3S_FALSE}
  };

  bool is_reserved_word(std::string & token)
  {
    return m_reserved_words.find(token) != m_reserved_words.end() ;
  }

  reserve_word_en_t get_reserved_word(std::string & token)
  {
    if (is_reserved_word(token)==true)
    {
      return m_reserved_words.find(token)->second;
    }
    else
    {
      return reserve_word_en_t::NA;
    }
  }

};

class base_statement;
class projection_alias
{
//purpose: mapping between alias-name to base_statement*
//those routines are *NOT* intensive, works once per query parse time.

private:
  std::vector< std::pair<std::string, base_statement*> > alias_map;

public:
  std::vector< std::pair<std::string, base_statement*> >* get()
  {
    return &alias_map;
  }

  bool insert_new_entry(std::string alias_name, base_statement* bs)
  {
    //purpose: only unique alias names.

    for(auto alias: alias_map)
    {
      if(alias.first.compare(alias_name) == 0)
      {
        return false;  //alias name already exist
      }

    }
    std::pair<std::string, base_statement*> new_alias(alias_name, bs);
    alias_map.push_back(new_alias);

    return true;
  }

  base_statement* search_alias(std::string alias_name)
  {
    for(auto alias: alias_map)
    {
      if(alias.first.compare(alias_name) == 0)
      {
        return alias.second;  //refernce to execution node
      }
    }
    return 0;
  }
};

struct binop_plus
{
  double operator()(double a, double b)
  {
    return a + b;
  }
};

struct binop_minus
{
  double operator()(double a, double b)
  {
    return a - b;
  }
};

struct binop_mult
{
  double operator()(double a, double b)
  {
    return a * b;
  }
};   

struct binop_div
{
  double operator()(double a, double b)
  {
    if (b == 0) {
      if( isnan(a)) {
        return a;
      } else {
        throw base_s3select_exception("division by zero is not allowed");
      } 
    } else {
      return a / b;
    }
  }
};

struct binop_pow
{
  double operator()(double a, double b)
  {
    return pow(a, b);
  }
};

struct binop_modulo
{
  int64_t operator()(int64_t a, int64_t b)
  {
    if (b == 0)
    {
      throw base_s3select_exception("Mod zero is not allowed");
    } else {
      return a % b;
    }
  }
};

class value
{

public:
  typedef union
  {
    int64_t num;
    char* str;//TODO consider string_view
    double dbl;
    boost::posix_time::ptime* timestamp;
  } value_t;

private:
  value_t __val;
  //std::string m_to_string;
  std::basic_string<char,std::char_traits<char>,ChunkAllocator<char,256>> m_to_string;
  //std::string m_str_value;
  std::basic_string<char,std::char_traits<char>,ChunkAllocator<char,256>> m_str_value;

public:
  enum class value_En_t
  {
    DECIMAL,
    FLOAT,
    STRING,
    TIMESTAMP,
    S3NULL,
    S3NAN,
    BOOL,
    NA
  } ;
  value_En_t type;

  explicit value(int64_t n) : type(value_En_t::DECIMAL)
  {
    __val.num = n;
  }
  explicit value(int n) : type(value_En_t::DECIMAL)
  {
    __val.num = n;
  }
  explicit value(bool b) : type(value_En_t::DECIMAL)
  {
    __val.num = (int64_t)b;
  }
  explicit value(double d) : type(value_En_t::FLOAT)
  {
    __val.dbl = d;
  }
  explicit value(boost::posix_time::ptime* timestamp) : type(value_En_t::TIMESTAMP)
  {
    __val.timestamp = timestamp;
  }

  explicit value(const char* s) : type(value_En_t::STRING)
  {
    m_str_value.assign(s);
    __val.str = m_str_value.data();
  }

  value():type(value_En_t::NA)
  {
    __val.num=0;
  }

  bool is_number() const
  {
    if ((type == value_En_t::DECIMAL || type == value_En_t::FLOAT))
    {
      return true;
    }

    return false;
  }

  bool is_string() const
  {
    return type == value_En_t::STRING;
  }
  bool is_timestamp() const
  {
    return type == value_En_t::TIMESTAMP;
  }

  bool is_bool() const
  {
    return type == value_En_t::BOOL;
  }

  bool is_null() const
  {
    return type == value_En_t::S3NULL;
  }

  bool is_nan() const
  {
    if (type == value_En_t::FLOAT) {
      return std::isnan(this->__val.dbl);
    }
    return type == value_En_t::S3NAN; 
  }

  bool is_true()
  {
    return (i64()!=0 && !is_null());
  }

  void set_nan() 
  {
    __val.dbl = NAN;
    type = value_En_t::FLOAT;
  }

  void set_true() 
  {
    __val.num = 1;
    type = value_En_t::BOOL;
  }

  void set_false() 
  {
    __val.num = 0;
    type = value_En_t::BOOL;
  }

  void setnull()
  {
    type = value_En_t::S3NULL;
  }

  std::string to_string()  //TODO very intensive , must improve this
  {

    if (type != value_En_t::STRING)
    {
      if (type == value_En_t::DECIMAL)
      {
        m_to_string.assign( boost::lexical_cast<std::string>(__val.num) );
      }
      if (type == value_En_t::BOOL)
      {
        if(__val.num == 0)
        {
          m_to_string.assign("false");
        }
        else
        {
          m_to_string.assign("true");
        }
      }
      else if(type == value_En_t::FLOAT)
      {
        m_to_string = boost::lexical_cast<std::string>(__val.dbl);
      }
      else if (type == value_En_t::TIMESTAMP)
      {
        m_to_string =  to_iso_extended_string( *__val.timestamp );
      }
      else if (type == value_En_t::S3NULL)
      {
        m_to_string.assign("null");
      }
    }
    else
    {
      m_to_string.assign( __val.str );
    }

    return std::string( m_to_string.c_str() );
  }


  value& operator=(value& o)
  {
    if(o.type == value_En_t::STRING)
    {
      m_str_value.assign(o.str());
      __val.str = m_str_value.data();
    }
    else
    {
      this->__val = o.__val;
    }

    this->type = o.type;

    return *this;
  }

  value& operator=(const char* s)
  {
    m_str_value.assign(s);
    this->__val.str = m_str_value.data();
    this->type = value_En_t::STRING;

    return *this;
  }

  value& operator=(int64_t i)
  {
    this->__val.num = i;
    this->type = value_En_t::DECIMAL;

    return *this;
  }

  value& operator=(double d)
  {
    this->__val.dbl = d;
    this->type = value_En_t::FLOAT;

    return *this;
  }

  value& operator=(bool b)
  {
    this->__val.num = (int64_t)b;
    this->type = value_En_t::BOOL;

    return *this;
  }

  value& operator=(boost::posix_time::ptime* p)
  {
    this->__val.timestamp = p;
    this->type = value_En_t::TIMESTAMP;

    return *this;
  }

  int64_t i64()
  {
    return __val.num;
  }

  const char* str()
  {
    return __val.str;
  }

  double dbl()
  {
    return __val.dbl;
  }

  boost::posix_time::ptime* timestamp() const
  {
    return __val.timestamp;
  }

  bool operator<(const value& v)//basic compare operator , most itensive runtime operation
  { 
    //TODO NA possible?
    if (is_string() && v.is_string())
    {
      return strcmp(__val.str, v.__val.str) < 0;
    }

    if (is_number() && v.is_number())
    {

      if(type != v.type)  //conversion //TODO find better way
      {
        if (type == value_En_t::DECIMAL)
        {
          return (double)__val.num < v.__val.dbl;
        }
        else
        {
          return __val.dbl < (double)v.__val.num;
        }
      }
      else   //no conversion
      {
        if(type == value_En_t::DECIMAL)
        {
          return __val.num < v.__val.num;
        }
        else
        {
          return __val.dbl < v.__val.dbl;
        }

      }
    }

    if(is_timestamp() && v.is_timestamp())
    {
      return *timestamp() < *(v.timestamp());
    }

    if(is_nan() || v.is_nan())
    {
      return false;
    } 

    throw base_s3select_exception("operands not of the same type(numeric , string), while comparision");
  }

  bool operator>(const value& v) //basic compare operator , most itensive runtime operation
  {
    //TODO NA possible?
    if (is_string() && v.is_string())
    {
      return strcmp(__val.str, v.__val.str) > 0;
    }

    if (is_number() && v.is_number())
    {

      if(type != v.type)  //conversion //TODO find better way
      {
        if (type == value_En_t::DECIMAL)
        {
          return (double)__val.num > v.__val.dbl;
        }
        else
        {
          return __val.dbl > (double)v.__val.num;
        }
      }
      else   //no conversion
      {
        if(type == value_En_t::DECIMAL)
        {
          return __val.num > v.__val.num;
        }
        else
        {
          return __val.dbl > v.__val.dbl;
        }

      }
    }

    if(is_timestamp() && v.is_timestamp())
    {
      return *timestamp() > *(v.timestamp());
    }

    if(is_nan() || v.is_nan())
    {
      return false;
    }

    throw base_s3select_exception("operands not of the same type(numeric , string), while comparision");
  }

  bool operator==(const value& v) //basic compare operator , most itensive runtime operation
  {
    //TODO NA possible?
    if (is_string() && v.is_string())
    {
      return strcmp(__val.str, v.__val.str) == 0;
    }


    if (is_number() && v.is_number())
    {

      if(type != v.type)  //conversion //TODO find better way
      {
        if (type == value_En_t::DECIMAL)
        {
          return (double)__val.num == v.__val.dbl;
        }
        else
        {
          return __val.dbl == (double)v.__val.num;
        }
      }
      else   //no conversion
      {
        if(type == value_En_t::DECIMAL)
        {
          return __val.num == v.__val.num;
        }
        else
        {
          return __val.dbl == v.__val.dbl;
        }

      }
    }

    if(is_timestamp() && v.is_timestamp())
    {
      return *timestamp() == *(v.timestamp());
    }

    if(
    (is_bool() && v.is_bool())
    ||
    (is_number() && v.is_bool())
    ||
    (is_bool() && v.is_number())
    )
    {
      return __val.num == v.__val.num;
    }

    if (is_nan() || v.is_nan())
    {
      return false;
    }  

    throw base_s3select_exception("operands not of the same type(numeric , string), while comparision");
  }
  bool operator<=(const value& v)
  { 
    if (is_nan() || v.is_nan()) {
      return false;
    } else { 
      return !(*this>v);
    } 
  }
  
  bool operator>=(const value& v)
  { 
    if (is_nan() || v.is_nan()) {
      return false;
    } else { 
      return !(*this<v);
    } 
  }
  
  bool operator!=(const value& v)
  { 
    if (is_nan() || v.is_nan()) {
      return true;
    } else { 
      return !(*this == v);
    }
  }
  
  template<typename binop> //conversion rules for arithmetical binary operations
  value& compute(value& l, const value& r) //left should be this, it contain the result
  {
    binop __op;

    if (l.is_string() || r.is_string())
    {
      throw base_s3select_exception("illegal binary operation with string");
    }
    if (l.is_bool() || r.is_bool())
    {
      throw base_s3select_exception("illegal binary operation with bool type");
    }

    if (l.is_number() && r.is_number())
    {
      if (l.type != r.type)
    {
      //conversion

      if (l.type == value_En_t::DECIMAL)
      {
        l.__val.dbl = __op((double)l.__val.num, r.__val.dbl);
        l.type = value_En_t::FLOAT;
      }
      else
      {
        l.__val.dbl = __op(l.__val.dbl, (double)r.__val.num);
        l.type = value_En_t::FLOAT;
      }
    }
    else
    {
      //no conversion

      if (l.type == value_En_t::DECIMAL)
      {
        l.__val.num = __op(l.__val.num, r.__val.num );
        l.type = value_En_t::DECIMAL;
      }
      else
      {
        l.__val.dbl = __op(l.__val.dbl, r.__val.dbl );
        l.type = value_En_t::FLOAT;
      }
    }
  }
    
    if (l.is_null() || r.is_null()) 
    {
      l.setnull();
    } else if(l.is_nan() || r.is_nan()) {
      l.set_nan();
    }

    return l;
  }

  value& operator+(const value& v)
  {
    return compute<binop_plus>(*this, v);
  }

  value operator++(int)
  {
    *this = *this + static_cast<value>(1);
    return *this;
  }
    
  value& operator-(const value& v)
  {
    return compute<binop_minus>(*this, v);
  }

  value& operator*(const value& v)
  {
    return compute<binop_mult>(*this, v);
  }
  
  value& operator/(value& v)
  {
    if (v.is_null() || this->is_null()) {
      v.setnull();
      return v;
    } else {
      return compute<binop_div>(*this, v);
    }
  }
  
  value& operator^(const value& v)
  {
    return compute<binop_pow>(*this, v);
  }

  value & operator%(const value &v)
  {
    if(v.type == value_En_t::DECIMAL) {
      return compute<binop_modulo>(*this,v);
    } else {
      throw base_s3select_exception("wrong use of modulo operation!");
    }
  }
};

class base_statement
{

protected:

  scratch_area* m_scratch;
  projection_alias* m_aliases;
  bool is_last_call; //valid only for aggregation functions
  bool m_is_cache_result;
  value m_alias_result;
  base_statement* m_projection_alias;
  int m_eval_stack_depth;
  bool m_skip_non_aggregate_op;
  value value_na;

public:
  base_statement():m_scratch(nullptr), is_last_call(false), m_is_cache_result(false),
  m_projection_alias(nullptr), m_eval_stack_depth(0), m_skip_non_aggregate_op(false) {}

  virtual value& eval()
  {
    //purpose: on aggregation flow to run only the correct subtree(aggregation subtree)
     
    if (m_skip_non_aggregate_op == false)
      return eval_internal();//not skipping this node.
    else
    {
    //skipping this node.
    //in case execution should skip a node, it will traverse (left and right) 
    //and search for subtree to execute.   
    //example: sum( ... ) - sum( ... ) ; the minus operand is skipped while sum() operand is not.
    if(left())
      left()->eval_internal();
    
    if(right())
      right()->eval_internal();
    
    }

    return value_na;
  }

  virtual value& eval_internal() = 0;
  
  virtual base_statement* left() const
  {
    return 0;
  }
  virtual base_statement* right() const
  {
    return 0;
  }
  virtual std::string print(int ident) =0;//TODO complete it, one option to use level parametr in interface ,
  virtual bool semantic() =0;//done once , post syntax , traverse all nodes and validate semantics.

  virtual void traverse_and_apply(scratch_area* sa, projection_alias* pa)
  {
    m_scratch = sa;
    m_aliases = pa;
    if (left())
    {
      left()->traverse_and_apply(m_scratch, m_aliases);
    }
    if (right())
    {
      right()->traverse_and_apply(m_scratch, m_aliases);
    }
  }

  virtual void set_skip_non_aggregate(bool skip_non_aggregate_op)
  {
    m_skip_non_aggregate_op = skip_non_aggregate_op;

    if (left())
    {
      left()->set_skip_non_aggregate(m_skip_non_aggregate_op);
    }
    if (right())
    {
      right()->set_skip_non_aggregate(m_skip_non_aggregate_op);
    }
  }

  virtual bool is_aggregate() const
  {
    return false;
  }

  virtual bool is_column() const
  {
    return false;
  }

  virtual void resolve_node()
  {//part of semantic analysis(TODO maybe semantic method should handle this)
    if (left())
    {
      left()->resolve_node();
    }
    if (right())
    {
      right()->resolve_node();
    }
  }

  bool is_function() const;
  const base_statement* get_aggregate() const;
  bool is_nested_aggregate(bool&) const;
  bool is_column_reference() const;
  bool mark_aggreagtion_subtree_to_execute();

  virtual void set_last_call()
  {
    is_last_call = true;
    if(left())
    {
      left()->set_last_call();
    }
    if(right())
    {
      right()->set_last_call();
    }
  }

  bool is_set_last_call()
  {
    return is_last_call;
  }

  void invalidate_cache_result()
  {
    m_is_cache_result = false;
  }

  bool is_result_cached()
  {
    return m_is_cache_result == true;
  }

  void set_result_cache(value& eval_result)
  {
    m_alias_result = eval_result;
    m_is_cache_result = true;
  }

  void dec_call_stack_depth()
  {
    m_eval_stack_depth --;
  }

  value& get_result_cache()
  {
    return m_alias_result;
  }

  int& get_eval_call_depth()
  {
    m_eval_stack_depth++;
    return m_eval_stack_depth;
  }

  virtual ~base_statement() {}

  void dtor()
  {
    this->~base_statement();
  }

};

class variable : public base_statement
{

public:

  enum class var_t
  {
    NA,
    VAR,//schema column (i.e. age , price , ...)
    COL_VALUE, //concrete value
    POS, // CSV column number  (i.e. _1 , _2 ... )
    STAR_OPERATION, //'*'
  } ;
  var_t m_var_type;

private:

  std::string _name;
  int column_pos;
  value var_value;
  std::string m_star_op_result;
  char m_star_op_result_charc[4096]; //TODO should be dynamic

  const int undefined_column_pos = -1;
  const int column_alias = -2;

public:
  variable():m_var_type(var_t::NA), _name(""), column_pos(-1) {}

  explicit variable(int64_t i) : m_var_type(var_t::COL_VALUE), column_pos(-1), var_value(i) {}

  explicit variable(double d) : m_var_type(var_t::COL_VALUE), _name("#"), column_pos(-1), var_value(d) {}

  explicit variable(int i) : m_var_type(var_t::COL_VALUE), column_pos(-1), var_value(i) {}

  explicit variable(const std::string& n) : m_var_type(var_t::VAR), _name(n), column_pos(-1) {}

  variable(const std::string& n,  var_t tp) : m_var_type(var_t::NA)
  {
    if(tp == variable::var_t::POS)
    {
      _name = n;
      m_var_type = tp;
      int pos = atoi( n.c_str() + 1 ); //TODO >0 < (schema definition , semantic analysis)
      column_pos = pos -1;// _1 is the first column ( zero position )
    }
    else if (tp == variable::var_t::COL_VALUE)
    {
      _name = "#";
      m_var_type = tp;
      column_pos = -1;
      var_value = n.c_str();
    }
    else if (tp ==variable::var_t::STAR_OPERATION)
    {
      _name = "#";
      m_var_type = tp;
      column_pos = -1;
    }
  }

  explicit variable(s3select_reserved_word::reserve_word_en_t reserve_word)
  {
    if (reserve_word == s3select_reserved_word::reserve_word_en_t::S3S_NULL)
    {
      m_var_type = variable::var_t::COL_VALUE;
      column_pos = -1;
      var_value.type = value::value_En_t::S3NULL;//TODO use set_null
    }
    else if (reserve_word == s3select_reserved_word::reserve_word_en_t::S3S_NAN)
    {
      m_var_type = variable::var_t::COL_VALUE;
      column_pos = -1;
      var_value.set_nan();
    }
    else if (reserve_word == s3select_reserved_word::reserve_word_en_t::S3S_TRUE)
    {
      m_var_type = variable::var_t::COL_VALUE;
      column_pos = -1;
      var_value.set_true();
    }
    else if (reserve_word == s3select_reserved_word::reserve_word_en_t::S3S_FALSE)
    {
      m_var_type = variable::var_t::COL_VALUE;
      column_pos = -1;
      var_value.set_false();
    }
    else 
    {
      _name = "#";
      m_var_type = var_t::NA;
      column_pos = -1;
    }
  }

  void operator=(value& v)
  {
    var_value = v;
  }

  void set_value(const char* s)
  {
    var_value = s;
  }

  void set_value(double d)
  {
    var_value = d;
  }

  void set_value(int64_t i)
  {
    var_value = i;
  }

  void set_value(boost::posix_time::ptime* p)
  {
    var_value = p;
  }

  void set_value(bool b)
  {
  	var_value = b;
    var_value.type = value::value_En_t::BOOL;
  }

  void set_null()
  {
    var_value.setnull();
  }

  virtual ~variable() {}

  virtual bool is_column() const //is reference to column.
  {
    if(m_var_type == var_t::VAR || m_var_type == var_t::POS)
    {
      return true;
    }
    return false;
  }

  value& get_value()
  {
    return var_value; //TODO is it correct
  }
  virtual value::value_En_t get_value_type()
  {
    return var_value.type;
  }


  value& star_operation()   //purpose return content of all columns in a input stream
  {


    int i;
    size_t pos=0;
    int num_of_columns = m_scratch->get_num_of_columns();
    for(i=0; i<num_of_columns-1; i++)
    {
      size_t len = m_scratch->get_column_value(i).size();
      if((pos+len)>sizeof(m_star_op_result_charc))
      {
        throw base_s3select_exception("result line too long", base_s3select_exception::s3select_exp_en_t::FATAL);
      }

      memcpy(&m_star_op_result_charc[pos], m_scratch->get_column_value(i).data(), len);
      pos += len;
      m_star_op_result_charc[ pos ] = ',';//TODO need for another abstraction (per file type)
      pos ++;

    }

    size_t len = m_scratch->get_column_value(i).size();
    if((pos+len)>sizeof(m_star_op_result_charc))
    {
      throw base_s3select_exception("result line too long", base_s3select_exception::s3select_exp_en_t::FATAL);
    }

    memcpy(&m_star_op_result_charc[pos], m_scratch->get_column_value(i).data(), len);
    m_star_op_result_charc[ pos + len ] = 0;
    var_value = (char*)&m_star_op_result_charc[0];
    return var_value;
  }

  virtual value& eval_internal()
  {
    if (m_var_type == var_t::COL_VALUE)
    {
      return var_value;  // a literal,could be deciml / float / string
    }
    else if(m_var_type == var_t::STAR_OPERATION)
    {
      return star_operation();
    }
    else if (column_pos == undefined_column_pos)
    {
      //done once , for the first time
      column_pos = m_scratch->get_column_pos(_name.c_str());

      if(column_pos>=0 && m_aliases->search_alias(_name.c_str()))
      {
        throw base_s3select_exception(std::string("multiple definition of column {") + _name + "} as schema-column and alias", base_s3select_exception::s3select_exp_en_t::FATAL);
      }


      if (column_pos == undefined_column_pos)
      {
        //not belong to schema , should exist in aliases
        m_projection_alias = m_aliases->search_alias(_name.c_str());

        //not enter this scope again
        column_pos = column_alias;
        if(m_projection_alias == 0)
        {
          throw base_s3select_exception(std::string("alias {")+_name+std::string("} or column not exist in schema"), base_s3select_exception::s3select_exp_en_t::FATAL);
        }
      }

    }

    if (m_projection_alias)
    {
      if (m_projection_alias->get_eval_call_depth()>2)
      {
        throw base_s3select_exception("number of calls exceed maximum size, probably a cyclic reference to alias", base_s3select_exception::s3select_exp_en_t::FATAL);
      }

      if (m_projection_alias->is_result_cached() == false)
      {
        var_value = m_projection_alias->eval();
        m_projection_alias->set_result_cache(var_value);
      }
      else
      {
        var_value = m_projection_alias->get_result_cache();
      }

      m_projection_alias->dec_call_stack_depth();
    }
    else
    {
      var_value = (char*)m_scratch->get_column_value(column_pos).data();  //no allocation. returning pointer of allocated space
      //in the case of successive column-delimiter {1,some_data,,3}=> third column is NULL 
      if (*var_value.str()== 0)
          var_value.setnull();
    }

    return var_value;
  }

  virtual std::string print(int ident)
  {
    //std::string out = std::string(ident,' ') + std::string("var:") + std::to_string(var_value.__val.num);
    //return out;
    return std::string("#");//TBD
  }

  virtual bool semantic()
  {
    return false;
  }

};

class arithmetic_operand : public base_statement
{

public:

  enum class cmp_t {NA, EQ, LE, LT, GT, GE, NE} ;

private:
  base_statement* l;
  base_statement* r;

  cmp_t _cmp;
  value var_value;
  bool negation_result;//false: dont negate ; upon NOT operator(unary) its true
  
public:

  virtual bool semantic()
  {
    return true;
  }

  base_statement* left() const override
  {
    return l;
  }
  base_statement* right() const override
  {
    return r;
  }

  virtual std::string print(int ident)
  {
    //std::string out = std::string(ident,' ') + "compare:" += std::to_string(_cmp) + "\n" + l->print(ident-5) +r->print(ident+5);
    //return out;
    return std::string("#");//TBD
  }

  virtual value& eval_internal()
  {
    if ((l->eval()).is_null()) {
        var_value.setnull();
        return var_value;
      } else if((r->eval()).is_null()) {
        var_value.setnull();
        return var_value;
      }
    
    switch (_cmp)
    {
    case cmp_t::EQ:
      return var_value =  bool( (l->eval() == r->eval()) ^ negation_result );
      break;

    case cmp_t::LE:
      return var_value = bool( (l->eval() <= r->eval()) ^ negation_result );
      break;

    case cmp_t::GE:
      return var_value = bool( (l->eval() >= r->eval()) ^ negation_result );
      break;

    case cmp_t::NE:
      return var_value = bool( (l->eval() != r->eval()) ^ negation_result );
      break;

    case cmp_t::GT:
      return var_value = bool( (l->eval() > r->eval()) ^ negation_result );
      break;

    case cmp_t::LT:
      return var_value = bool( (l->eval() < r->eval()) ^ negation_result );
      break;

    default:
      throw base_s3select_exception("internal error");
      break;
    }
  }

  arithmetic_operand(base_statement* _l, cmp_t c, base_statement* _r):l(_l), r(_r), _cmp(c),negation_result(false) {}
  
  explicit arithmetic_operand(base_statement* p)//NOT operator 
  {
    l = dynamic_cast<arithmetic_operand*>(p)->l;
    r = dynamic_cast<arithmetic_operand*>(p)->r;
    _cmp = dynamic_cast<arithmetic_operand*>(p)->_cmp;
    // not( not ( logical expression )) == ( logical expression ); there is no limitation for number of NOT.
    negation_result = ! dynamic_cast<arithmetic_operand*>(p)->negation_result;
  }

  virtual ~arithmetic_operand() {}
};

class logical_operand : public base_statement
{

public:

  enum class oplog_t {AND, OR, NA};

private:
  base_statement* l;
  base_statement* r;

  oplog_t _oplog;
  value var_value;
  bool negation_result;//false: dont negate ; upon NOT operator(unary) its true

public:

  base_statement* left() const override
  {
    return l;
  }
  base_statement* right() const override
  {
    return r;
  }

  virtual bool semantic()
  {
    return true;
  }

  logical_operand(base_statement* _l, oplog_t _o, base_statement* _r):l(_l), r(_r), _oplog(_o),negation_result(false) {}

  explicit logical_operand(base_statement * p)//NOT operator
  {
    l = dynamic_cast<logical_operand*>(p)->l;
    r = dynamic_cast<logical_operand*>(p)->r;
    _oplog = dynamic_cast<logical_operand*>(p)->_oplog;
    // not( not ( logical expression )) == ( logical expression ); there is no limitation for number of NOT.
    negation_result = ! dynamic_cast<logical_operand*>(p)->negation_result; 
  }

  virtual ~logical_operand() {}

  virtual std::string print(int ident)
  {
    //std::string out = std::string(ident, ' ') + "logical_operand:" += std::to_string(_oplog) + "\n" + l->print(ident - 5) + r->print(ident + 5);
    //return out;
    return std::string("#");//TBD
  }
  virtual value& eval_internal()
  {
    if (!l || !r)
    {
      throw base_s3select_exception("missing operand for logical ", base_s3select_exception::s3select_exp_en_t::FATAL);
    }
    value a = l->eval();
    if (_oplog == oplog_t::AND)
    {
      if (!a.is_null() && a.i64() == false) {
        bool res = false ^ negation_result;
        return var_value = res;
      } 
      value b = r->eval();
      if(!b.is_null() && b.i64() == false) {
        bool res = false ^ negation_result;
        return var_value = res;
      } else {
        if (a.is_null() || b.is_null()) {
          var_value.setnull();
          return var_value;
        } else {
          bool res =  true ^ negation_result ;
          return var_value =res;
        }
      }   
    }
    else
    {
      if (a.is_true()) {
        bool res = true ^ negation_result;
        return var_value = res;
      } 
      value b = r->eval();
      if(b.is_true() == true) {
        bool res = true ^ negation_result;
        return var_value = res;
      } else {
        if (a.is_null() || b.is_null()) {
          var_value.setnull();
          return var_value;
        } else {
          bool res =  false ^ negation_result ;
          return var_value =res;
        }
      }
    }
  }
};

class mulldiv_operation : public base_statement
{

public:

  enum class muldiv_t {NA, MULL, DIV, POW, MOD} ;

private:
  base_statement* l;
  base_statement* r;

  muldiv_t _mulldiv;
  value var_value;
  value tmp_value;

public:

  base_statement* left() const override
  {
    return l;
  }
  base_statement* right() const override
  {
    return r;
  }

  virtual bool semantic()
  {
    return true;
  }

  virtual std::string print(int ident)
  {
    //std::string out = std::string(ident, ' ') + "mulldiv_operation:" += std::to_string(_mulldiv) + "\n" + l->print(ident - 5) + r->print(ident + 5);
    //return out;
    return std::string("#");//TBD
  }

  virtual value& eval_internal()
  {
    switch (_mulldiv)
    {
    case muldiv_t::MULL:
      tmp_value = l->eval();
      return var_value = tmp_value * r->eval();
      break;

    case muldiv_t::DIV:
      tmp_value = l->eval();
      return var_value = tmp_value / r->eval();
      break;

    case muldiv_t::POW:
      tmp_value = l->eval();
      return var_value = tmp_value ^ r->eval();
      break;

    case muldiv_t::MOD:
      tmp_value = l->eval();
      return var_value = tmp_value % r->eval();
      break;

    default:
      throw base_s3select_exception("internal error");
      break;
    }
  }

  mulldiv_operation(base_statement* _l, muldiv_t c, base_statement* _r):l(_l), r(_r), _mulldiv(c) {}

  virtual ~mulldiv_operation() {}
};

class addsub_operation : public base_statement
{

public:

  enum class addsub_op_t {ADD, SUB, NA};

private:
  base_statement* l;
  base_statement* r;

  addsub_op_t _op;
  value var_value;
  value tmp_value;

public:

  base_statement* left() const override
  {
    return l;
  }
  base_statement* right() const override
  {
    return r;
  }

  virtual bool semantic()
  {
    return true;
  }

  addsub_operation(base_statement* _l, addsub_op_t _o, base_statement* _r):l(_l), r(_r), _op(_o) {}

  virtual ~addsub_operation() {}

  virtual std::string print(int ident)
  {
    //std::string out = std::string(ident, ' ') + "addsub_operation:" += std::to_string(_op) + "\n" + l->print(ident - 5) + r->print(ident + 5);
    return std::string("#");//TBD
  }

  virtual value& eval_internal()
  {
    if (_op == addsub_op_t::NA) // -num , +num , unary-operation on number
    {
      if (l)
      {
        return var_value = l->eval();
      }
      else if (r)
      {
        return var_value = r->eval();
      }
    }
    else if (_op == addsub_op_t::ADD)
    {tmp_value=l->eval();
      return var_value = (tmp_value + r->eval());
    }
    else
    {tmp_value=l->eval();
      return var_value = (tmp_value - r->eval());
    }

    return var_value;
  }
};

class negate_function_operation : public base_statement
{
  //purpose: some functions (between,like,in) are participating in where-clause as predicates; thus NOT unary-operator may operate on them.

  private:
  
  base_statement* function_to_negate;
  value res;
  
  public:

  explicit negate_function_operation(base_statement *f):function_to_negate(f){}

  virtual std::string print(int ident)
  {
    return std::string("#");//TBD
  }

  virtual bool semantic()
  {
    return true;
  }

  base_statement* left() const override
  {
    return function_to_negate;
  }

  virtual value& eval_internal()
  {
    res = function_to_negate->eval();

    if (res.is_number() || res.is_bool())//TODO is integer type
    {
      if (res.is_true())
      {
        res = (bool)0;
      }
      else
      {
        res = (bool)1;
      }
    }

    return res;
  }

};

class base_function
{

protected:
  bool aggregate;

public:
  //TODO add semantic to base-function , it operate once on function creation
  // validate semantic on creation instead on run-time
  virtual bool operator()(bs_stmt_vec_t* args, variable* result) = 0;
  base_function() : aggregate(false) {}
  bool is_aggregate() const
  {
    return aggregate == true;
  }
  virtual void get_aggregate_result(variable*) {}

  virtual ~base_function() = default;
  
  virtual void dtor()
  {//release function-body implementation 
    this->~base_function();
  }

};

class base_date_extract : public base_function
{
  protected:
    value val_timestamp;
    boost::posix_time::ptime new_ptime;

  public:
    void param_validation(bs_stmt_vec_t*& args)
    {
      auto iter = args->begin();
      int args_size = args->size();

      if (args_size < 1)
      {
        throw base_s3select_exception("to_timestamp should have 2 parameters");
      }

      base_statement* ts = *iter;
      val_timestamp = ts->eval();
      if(val_timestamp.is_timestamp()== false)
      {
        throw base_s3select_exception("second parameter is not timestamp");
      }

      new_ptime = *val_timestamp.timestamp();
    }

};

class base_date_diff : public base_function
{
  protected:
    value val_ts1;
    value val_ts2;

  public:
    void param_validation(bs_stmt_vec_t*& args)
    {
      auto iter = args->begin();
      int args_size = args->size();

      if (args_size < 2)
      {
        throw base_s3select_exception("datediff need 3 parameters");
      }

      base_statement* dt1_param = *iter;
      val_ts1 = dt1_param->eval();

      if (val_ts1.is_timestamp() == false)
      {
        throw base_s3select_exception("second parameter should be timestamp");
      }

      iter++;
      base_statement* dt2_param = *iter;
      val_ts2 = dt2_param->eval();

      if (val_ts2.is_timestamp() == false)
      {
        throw base_s3select_exception("third parameter should be timestamp");
      }
    }

};

class base_date_add : public base_function
{
  protected:
    value val_quantity;
    value val_ts;
    boost::posix_time::ptime new_ptime;

  public:
    void param_validation(bs_stmt_vec_t*& args)
    {
      auto iter = args->begin();
      int args_size = args->size();

      if (args_size < 2)
      {
        throw base_s3select_exception("add_to_timestamp should have 3 parameters");
      }

      base_statement* quan = *iter;
      val_quantity = quan->eval();

      if (val_quantity.is_number() == false)
      {
        throw base_s3select_exception("second parameter should be number");  //TODO what about double?
      }

      iter++;
      base_statement* ts = *iter;
      val_ts = ts->eval();

      if(val_ts.is_timestamp() == false)
      {
        throw base_s3select_exception("third parameter should be time-stamp");
      }

      new_ptime = *val_ts.timestamp();
    }

};

class base_time_to_string
{
  protected:
    std::vector<std::string> months = { "January", "February", "March","April",
                        "May", "June", "July", "August", "September",
                        "October", "November", "December"};
  public:
    virtual std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param) = 0;
    virtual ~base_time_to_string() = default;
};

class derive_yyyy : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t yr = new_ptime.date().year();
      return std::string(param - 4, '0') + std::to_string(yr);
    }
} yyyy_to_string;

class derive_yy : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t yr = new_ptime.date().year();
      return std::string(2 - std::to_string(yr%100).length(), '0') + std::to_string(yr%100);
    }
} yy_to_string;

class derive_y : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t yr = new_ptime.date().year();
      return std::to_string(yr);
    }
} y_to_string;

class derive_mmmmm_month : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t mnth = new_ptime.date().month();
      return (months[mnth - 1]).substr(0, 1);
    }
} mmmmm_month_to_string;

class derive_mmmm_month : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t mnth = new_ptime.date().month();
      return months[mnth - 1];
    }
} mmmm_month_to_string;

class derive_mmm_month : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t mnth = new_ptime.date().month();
      return (months[mnth - 1]).substr(0, 3);
    }
} mmm_month_to_string;

class derive_mm_month : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t mnth = new_ptime.date().month();
      std::string mnth_str = std::to_string(mnth);
      return std::string(2 - mnth_str.length(), '0') + mnth_str;
    }
} mm_month_to_string;

class derive_m_month : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t mnth = new_ptime.date().month();
      return std::to_string(mnth);
    }
} m_month_to_string;

class derive_dd : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string day = std::to_string(new_ptime.date().day());
      return std::string(2 - day.length(), '0') + day;
    }
} dd_to_string;

class derive_d : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string day = std::to_string(new_ptime.date().day());
      return day;
    }
} d_to_string;

class derive_a : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t hr = new_ptime.time_of_day().hours();
      std::string meridiem = (hr < 12 ? "AM" : "PM");
      return meridiem;
    }
} a_to_string;

class derive_hh : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t hr = new_ptime.time_of_day().hours();
      std::string hr_12 = std::to_string(hr%12 == 0 ? 12 : hr%12);
      return std::string(2 - hr_12.length(), '0') + hr_12;
    }
} hh_to_string;

class derive_h : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t hr = new_ptime.time_of_day().hours();
      std::string hr_12 = std::to_string(hr%12 == 0 ? 12 : hr%12);
      return hr_12;
    }
} h_to_string;

class derive_h2 : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t hr = new_ptime.time_of_day().hours();
      std::string hr_24 = std::to_string(hr);
      return std::string(2 - hr_24.length(), '0') + hr_24;
    }
} h2_to_string;

class derive_h1 : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      int64_t hr = new_ptime.time_of_day().hours();
      return std::to_string(hr);
    }
} h1_to_string;

class derive_mm : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string mint = std::to_string(new_ptime.time_of_day().minutes());
      return std::string(2 - mint.length(), '0') + mint;
    }
} mm_to_string;

class derive_m : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string mint = std::to_string(new_ptime.time_of_day().minutes());
      return mint;
    }
} m_to_string;

class derive_ss : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string sec =  std::to_string(new_ptime.time_of_day().seconds());
      return std::string(2 - sec.length(), '0') + sec;
    }
} ss_to_string;

class derive_s : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string sec =  std::to_string(new_ptime.time_of_day().seconds());
      return sec;
    }
} s_to_string;

class derive_frac_sec : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string frac_seconds = std::to_string(new_ptime.time_of_day().fractional_seconds());
      if (param >= frac_seconds.length())
      {
        return frac_seconds + std::string(param - frac_seconds.length(), '0');
      }
      else
      {
        return frac_seconds.substr(0, param);
      }
    }
} frac_sec_to_string;

class derive_n : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      std::string frac_seconds = std::to_string(new_ptime.time_of_day().fractional_seconds());
      return frac_seconds + std::string(9 - frac_seconds.length(), '0');
    }
} n_to_string;

class derive_delimiter : public base_time_to_string
{
  public:
    std::string print_time(boost::posix_time::ptime& new_ptime, uint32_t param)
    {
      char ch = param;
      return std::string(1, ch);
    }
} delimiter_to_string;

class base_timestamp_to_string : public base_function
{
  protected:
    boost::posix_time::ptime new_ptime;
    std::string format;
    std::vector<char> m_metachar {'y', 'M', 'd', 'a', 'h', 'H', 'm', 's', 'S', 'n'};
    std::vector<std::string> m_metaword_vec {"yyy", "yy", "y", "MMMMM", "MMMM",
                                "MMM", "MM", "M", "dd", "d", "a", "hh", "h",
                                "HH", "H", "mm", "m", "ss", "s", "n"};
    std::vector<base_time_to_string*> print_vector;
    std::vector<uint32_t> para;
    bool initialized = false;

    using to_string_lib_t = std::map<std::string,base_time_to_string* >;

    const to_string_lib_t time_to_string_functions =
    {
      {"yyyy+", &yyyy_to_string},
      {"yyy", &y_to_string},
      {"yy", &yy_to_string},
      {"y", &y_to_string},
      {"MMMMM", &mmmmm_month_to_string},
      {"MMMM", &mmmm_month_to_string},
      {"MMM", &mmm_month_to_string},
      {"MM", &mm_month_to_string},
      {"M", &m_month_to_string},
      {"dd", &dd_to_string },
      {"d", &d_to_string },
      {"a", &a_to_string },
      {"hh", &hh_to_string},
      {"h", &h_to_string},
      {"HH", &h2_to_string},
      {"H", &h1_to_string},
      {"mm", &mm_to_string},
      {"m", &m_to_string},
      {"ss", &ss_to_string},
      {"s", &s_to_string},
      {"S+", &frac_sec_to_string},
      {"n", &n_to_string},
      {"delimiter", &delimiter_to_string}
    };

  public:
    void param_validation(bs_stmt_vec_t*& args)
    {
      auto iter = args->begin();
      int args_size = args->size();

      if (args_size < 2)
      {
        throw base_s3select_exception("to_string need 2 parameters");
      }

      base_statement* dt1_param = *iter;
      value val_timestamp = dt1_param->eval();

      if (val_timestamp.is_timestamp() == false)
      {
        throw base_s3select_exception("first parameter should be timestamp");
      }

      iter++;
      base_statement* frmt = *iter;
      value val_format = frmt->eval();

      if (val_format.is_string() == false)
      {
        throw base_s3select_exception("second parameter should be string");
      }

      new_ptime = *val_timestamp.timestamp();
      format = val_format.to_string();
    }

    uint32_t length_same_char_str(std::string str, char ch)
    {
      int i = 0;
      while(str[i] == ch)
        i++;
      return i;
    }

    void prepare_to_string_vector(std::vector<base_time_to_string*>& print_vector, std::vector<uint32_t>& para)
    {
      for (uint32_t i = 0; i < format.length(); i++)
      {
        if (std::find(m_metachar.begin(), m_metachar.end() , format[i]) != m_metachar.end())
        {
          if (format.substr(i, 4).compare("yyyy") == 0)
          {
            uint32_t len = length_same_char_str(format.substr(i), 'y');
            auto it = time_to_string_functions.find("yyyy+");
            print_vector.push_back( it->second);
            para.push_back(len);
            i += len - 1;
            continue;
          }
          else if (format[i] == 'S')
          {
            uint32_t len = length_same_char_str(format.substr(i), 'S');
            auto it = time_to_string_functions.find("S+");
            print_vector.push_back( it->second);
            para.push_back(len);
            i += len - 1;
            continue;
          }

          for (auto word : m_metaword_vec)
          {
            if (format.substr(i, word.length()).compare(word) == 0)
            {
              auto it = time_to_string_functions.find(word.c_str());
              print_vector.push_back( it->second);
              para.push_back('\0');
              i += word.length() - 1;
              break;
            }
          }
        }
        else
        {
          auto it = time_to_string_functions.find("delimiter");
          print_vector.push_back( it->second );
          para.push_back(format[i]);
        }
      }
    }

    std::string execute_to_string(std::vector<base_time_to_string*>& print_vector, std::vector<uint32_t>& para)
    {
      std::string res;
      int temp = 0;
      for(auto p : print_vector)
      {
        res += p->print_time(new_ptime, para.at(temp));
        temp++;
      }
      return res;
    }

};

};//namespace

#endif
