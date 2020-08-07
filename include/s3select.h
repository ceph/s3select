#ifndef __S3SELECT__
#define __S3SELECT__
#define BOOST_SPIRIT_THREADSAFE

#include <boost/spirit/include/classic_core.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>
#include <list>
#include "s3select_oper.h"
#include "s3select_functions.h"
#include "s3select_csv_parser.h"
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <functional>


#define _DEBUG_TERM {string  token(a,b);std::cout << __FUNCTION__ << token << std::endl;}


namespace s3selectEngine
{

/// AST builder

class s3select_projections
{

private:
  std::vector<base_statement*> m_projections;

public:
  bool is_aggregate()
  {
    //TODO iterate on projections , and search for aggregate
    //for(auto p : m_projections){}

    return false;
  }

  bool semantic()
  {
    //TODO check aggragtion function are not nested
    return false;
  }

  std::vector<base_statement*>* get()
  {
    return &m_projections;
  }

};

static s3select_reserved_word g_s3select_reserve_word;//read-only

struct actionQ
{
// upon parser is accepting a token (lets say some number),
// it push it into dedicated queue, later those tokens are poped out to build some "higher" contruct (lets say 1 + 2)
// those containers are used only for parsing phase and not for runtime.

  std::vector<mulldiv_operation::muldiv_t> muldivQ;
  std::vector<addsub_operation::addsub_op_t> addsubQ;
  std::vector<arithmetic_operand::cmp_t> arithmetic_compareQ;
  std::vector<logical_operand::oplog_t> logical_compareQ;
  std::vector<base_statement*> exprQ;
  std::vector<base_statement*> funcQ;
  std::vector<base_statement*> condQ;
  projection_alias alias_map;
  std::string from_clause;
  std::vector<std::string> schema_columns;
  s3select_projections  projections;

};

class s3select;

struct push_from_clause
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_from_clause g_push_from_clause;

struct push_number
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_number g_push_number;

struct push_float_number
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_float_number g_push_float_number;

struct push_string
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_string g_push_string;

struct push_variable
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_variable g_push_variable;

/////////////////////////arithmetic unit  /////////////////
struct push_addsub
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_addsub g_push_addsub;

struct push_mulop
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_mulop g_push_mulop;

struct push_addsub_binop
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_addsub_binop g_push_addsub_binop;

struct push_mulldiv_binop
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_mulldiv_binop g_push_mulldiv_binop;

struct push_function_arg
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_function_arg g_push_function_arg;

struct push_function_name
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_function_name g_push_function_name;

struct push_function_expr
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_function_expr g_push_function_expr;

////////////////////// logical unit ////////////////////////

struct push_compare_operator
{
  void operator()(s3select* self, const char* a, const char* b) const;

};
static push_compare_operator g_push_compare_operator;

struct push_logical_operator
{
  void operator()(s3select* self, const char* a, const char* b) const;

};
static push_logical_operator g_push_logical_operator;

struct push_arithmetic_predicate
{
  void operator()(s3select* self, const char* a, const char* b) const;

};
static push_arithmetic_predicate g_push_arithmetic_predicate;

struct push_logical_predicate
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_logical_predicate g_push_logical_predicate;

struct push_negation
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_negation g_push_negation;

struct push_column_pos
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static  push_column_pos g_push_column_pos;

struct push_projection
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_projection g_push_projection;

struct push_alias_projection
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_alias_projection g_push_alias_projection;

struct push_debug_1
{
  void operator()(s3select* self, const char* a, const char* b) const;
};
static push_debug_1 g_push_debug_1;

struct s3select : public bsc::grammar<s3select>
{
private:

  actionQ m_actionQ;

  scratch_area m_sca;

  s3select_functions m_s3select_functions;

  std::string error_description;

  s3select_allocator m_s3select_allocator;

  bool aggr_flow;

#define BOOST_BIND_ACTION( push_name ) boost::bind( &push_name::operator(), g_ ## push_name, const_cast<s3select*>(&self), _1, _2)

public:

  actionQ* getAction()
  {
    return &m_actionQ;
  }

  s3select_allocator* getAllocator()
  {
    return &m_s3select_allocator;
  }

  s3select_functions* getS3F()
  {
    return &m_s3select_functions;
  }

  int semantic()
  {
    for (auto e : get_projections_list())
    {
      base_statement* aggr = 0;

      if ((aggr = e->get_aggregate()) != 0)
      {
        if (aggr->is_nested_aggregate(aggr))
        {
          error_description = "nested aggregation function is illegal i.e. sum(...sum ...)";
          throw base_s3select_exception(error_description, base_s3select_exception::s3select_exp_en_t::FATAL);
        }

        aggr_flow = true;
      }
    }

    if (aggr_flow == true)
      for (auto e : get_projections_list())
      {
        base_statement* skip_expr = e->get_aggregate();

        if (e->is_binop_aggregate_and_column(skip_expr))
        {
          error_description = "illegal expression. /select sum(c1) + c1 ..../ is not allow type of query";
          throw base_s3select_exception(error_description, base_s3select_exception::s3select_exp_en_t::FATAL);
        }
      }

    return 0;
  }

  int parse_query(const char* input_query)
  {
    if(get_projections_list().empty() == false)
    {
      return 0;  //already parsed
    }


    error_description.clear();
    aggr_flow = false;

    try
    {
      bsc::parse_info<> info = bsc::parse(input_query, *this, bsc::space_p);
      auto query_parse_position = info.stop;

      if (!info.full)
      {
        std::cout << "failure -->" << query_parse_position << "<---" << std::endl;
        error_description = std::string("failure -->") + query_parse_position + std::string("<---");
        return -1;
      }

      semantic();
    }
    catch (base_s3select_exception& e)
    {
      std::cout << e.what() << std::endl;
      error_description.assign(e.what());
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL) //abort query execution
      {
        return -1;
      }
    }

    return 0;
  }

  std::string get_error_description()
  {
    return error_description;
  }

  s3select()
  {
    m_s3select_functions.setAllocator(&m_s3select_allocator);
  }

  bool is_semantic()//TBD traverse and validate semantics per all nodes
  {
    base_statement* cond = m_actionQ.exprQ.back();

    return  cond->semantic();
  }

  std::string get_from_clause()
  {
    return m_actionQ.from_clause;
  }

  void load_schema(std::vector< std::string>& scm)
  {
    int i = 0;
    for (auto c : scm)
    {
      m_sca.set_column_pos(c.c_str(), i++);
    }
  }

  base_statement* get_filter()
  {
    if(m_actionQ.condQ.size()==0)
    {
      return NULL;
    }

    return m_actionQ.condQ.back();
  }

  std::vector<base_statement*>  get_projections_list()
  {
    return *m_actionQ.projections.get(); //TODO return COPY(?) or to return evalaution results (list of class value{}) / return reference(?)
  }

  scratch_area* get_scratch_area()
  {
    return &m_sca;
  }

  projection_alias* get_aliases()
  {
    return &m_actionQ.alias_map;
  }

  bool is_aggregate_query()
  {
    return aggr_flow == true;
  }

  ~s3select()
  {
    m_s3select_functions.clean();
  }


  template <typename ScannerT>
  struct definition
  {
    definition(s3select const& self)
    {
      ///// s3select syntax rules and actions for building AST

      select_expr = bsc::str_p("select")  >> projections >> bsc::str_p("from") >> (s3_object)[BOOST_BIND_ACTION(push_from_clause)] >> !where_clause >> ';';

      projections = projection_expression >> *( ',' >> projection_expression) ;

      projection_expression = (arithmetic_expression >> bsc::str_p("as") >> alias_name)[BOOST_BIND_ACTION(push_alias_projection)] | (arithmetic_expression)[BOOST_BIND_ACTION(push_projection)]  ;

      alias_name = bsc::lexeme_d[(+bsc::alpha_p >> *bsc::digit_p)] ;


      s3_object = bsc::str_p("stdin") | object_path ;

      object_path = "/" >> *( fs_type >> "/") >> fs_type;

      fs_type = bsc::lexeme_d[+( bsc::alnum_p | bsc::str_p(".")  | bsc::str_p("_")) ];

      where_clause = bsc::str_p("where") >> condition_expression;

      condition_expression =  ( bsc::str_p("not") >> binary_condition )[BOOST_BIND_ACTION(push_negation)] | binary_condition;

      binary_condition = (arithmetic_predicate >> *(log_op[BOOST_BIND_ACTION(push_logical_operator)] >> arithmetic_predicate[BOOST_BIND_ACTION(push_logical_predicate)]));

      arithmetic_predicate = (factor >> *(arith_cmp[BOOST_BIND_ACTION(push_compare_operator)] >> factor[BOOST_BIND_ACTION(push_arithmetic_predicate)]));

      factor = (arithmetic_expression) | ('(' >> condition_expression >> ')') ;

      arithmetic_expression = (addsub_operand >> *(addsubop_operator[BOOST_BIND_ACTION(push_addsub)] >> addsub_operand[BOOST_BIND_ACTION(push_addsub_binop)] ));

      addsub_operand = (mulldiv_operand >> *(muldiv_operator[BOOST_BIND_ACTION(push_mulop)]  >> mulldiv_operand[BOOST_BIND_ACTION(push_mulldiv_binop)] ));// this non-terminal gives precedense to  mull/div

      mulldiv_operand = arithmetic_argument | ('(' >> (arithmetic_expression) >> ')') ;

      list_of_function_arguments = (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)] >> *(',' >> (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)]);
      function = ((variable >> '(' )[BOOST_BIND_ACTION(push_function_name)] >> !list_of_function_arguments >> ')')[BOOST_BIND_ACTION(push_function_expr)];

      arithmetic_argument = (float_number)[BOOST_BIND_ACTION(push_float_number)] |  (number)[BOOST_BIND_ACTION(push_number)] | (column_pos)[BOOST_BIND_ACTION(push_column_pos)] |
                            (string)[BOOST_BIND_ACTION(push_string)] |
                            (function)[BOOST_BIND_ACTION(push_debug_1)]  | (variable)[BOOST_BIND_ACTION(push_variable)] ;//function is pushed by right-term


      number = bsc::int_p;

      float_number = bsc::real_p;

      string = bsc::str_p("\"") >> *( bsc::anychar_p - bsc::str_p("\"") ) >> bsc::str_p("\"") ;

      column_pos = ('_'>>+(bsc::digit_p) ) | '*' ;

      muldiv_operator = bsc::str_p("*") | bsc::str_p("/") | bsc::str_p("^") | bsc::str_p("%");// got precedense

      addsubop_operator = bsc::str_p("+") | bsc::str_p("-");


      arith_cmp = bsc::str_p(">=") | bsc::str_p("<=") | bsc::str_p("==") | bsc::str_p("<") | bsc::str_p(">") | bsc::str_p("!=");

      log_op = bsc::str_p("and") | bsc::str_p("or");

      variable =  bsc::lexeme_d[(+bsc::alpha_p >> *bsc::digit_p)];
    }


    bsc::rule<ScannerT> variable, select_expr, s3_object, where_clause, number, float_number, string, arith_cmp, log_op, condition_expression, binary_condition, arithmetic_predicate, factor;
    bsc::rule<ScannerT> muldiv_operator, addsubop_operator, function, arithmetic_expression, addsub_operand, list_of_function_arguments, arithmetic_argument, mulldiv_operand;
    bsc::rule<ScannerT> fs_type, object_path;
    bsc::rule<ScannerT> projections, projection_expression, alias_name, column_pos;
    bsc::rule<ScannerT> const& start() const
    {
      return select_expr;
    }
  };
};

void push_from_clause::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  self->getAction()->from_clause = token;
}

void push_number::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  variable* v = S3SELECT_NEW(self, variable, atoi(token.c_str()));

  self->getAction()->exprQ.push_back(v);
}

void push_float_number::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  //the parser for float(real_p) is accepting also integers, thus "blocking" integer acceptence and all are float.
  bsc::parse_info<> info = bsc::parse(token.c_str(), bsc::int_p, bsc::space_p);

  if (!info.full)
  {
    char* perr;
    double d = strtod(token.c_str(), &perr);
    variable* v = S3SELECT_NEW(self, variable, d);

    self->getAction()->exprQ.push_back(v);
  }
  else
  {
    variable* v = S3SELECT_NEW(self, variable, atoi(token.c_str()));

    self->getAction()->exprQ.push_back(v);
  }
}

void push_string::operator()(s3select* self, const char* a, const char* b) const
{
  a++;
  b--; // remove double quotes
  std::string token(a, b);

  variable* v = S3SELECT_NEW(self, variable, token, variable::var_t::COL_VALUE);

  self->getAction()->exprQ.push_back(v);
}

void push_variable::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  variable* v = 0;

  if (g_s3select_reserve_word.is_reserved_word(token))
  {
    if (g_s3select_reserve_word.get_reserved_word(token) == s3select_reserved_word::reserve_word_en_t::S3S_NULL)
    {
      v = S3SELECT_NEW(self, variable, s3select_reserved_word::reserve_word_en_t::S3S_NULL);
    }
    else if (g_s3select_reserve_word.get_reserved_word(token) == s3select_reserved_word::reserve_word_en_t::S3S_NAN)
    {
      v = S3SELECT_NEW(self, variable, s3select_reserved_word::reserve_word_en_t::S3S_NAN);
    }
    else
    {
      v = S3SELECT_NEW(self, variable, s3select_reserved_word::reserve_word_en_t::NA);
    }
    
  }
  else
  {
    v = S3SELECT_NEW(self, variable, token);
  }
  
  self->getAction()->exprQ.push_back(v);
}

void push_addsub::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  if (token.compare("+") == 0)
  {
    self->getAction()->addsubQ.push_back(addsub_operation::addsub_op_t::ADD);
  }
  else
  {
    self->getAction()->addsubQ.push_back(addsub_operation::addsub_op_t::SUB);
  }
}

void push_mulop::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  if (token.compare("*") == 0)
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::MULL);
  }
  else if (token.compare("/") == 0)
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::DIV);
  }
  else if(token.compare("^") == 0)
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::POW);
  }
  else
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::MOD);
  }
}

void push_addsub_binop::operator()(s3select* self, const char* a, const char* b) const
{
  base_statement* l = 0, *r = 0;

  r = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  l = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  addsub_operation::addsub_op_t o = self->getAction()->addsubQ.back();
  self->getAction()->addsubQ.pop_back();
  addsub_operation* as = S3SELECT_NEW(self, addsub_operation, l, o, r);
  self->getAction()->exprQ.push_back(as);
}

void push_mulldiv_binop::operator()(s3select* self, const char* a, const char* b) const
{
  base_statement* vl = 0, *vr = 0;

  vr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  vl = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  mulldiv_operation::muldiv_t o = self->getAction()->muldivQ.back();
  self->getAction()->muldivQ.pop_back();
  mulldiv_operation* f = S3SELECT_NEW(self, mulldiv_operation, vl, o, vr);
  self->getAction()->exprQ.push_back(f);
}

void push_function_arg::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* be = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  base_statement* f = self->getAction()->funcQ.back();

  if (dynamic_cast<__function*>(f))
  {
    dynamic_cast<__function*>(f)->push_argument(be);
  }
}

void push_function_name::operator()(s3select* self, const char* a, const char* b) const
{
  b--;
  while (*b == '(' || *b == ' ')
  {
    b--; //point to function-name
  }

  std::string fn;
  fn.assign(a, b - a + 1);

  __function* func = S3SELECT_NEW(self, __function, fn.c_str(), self->getS3F());
  self->getAction()->funcQ.push_back(func);
}

void push_function_expr::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* func = self->getAction()->funcQ.back();
  self->getAction()->funcQ.pop_back();

  self->getAction()->exprQ.push_back(func);
}

void push_compare_operator::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  arithmetic_operand::cmp_t c = arithmetic_operand::cmp_t::NA;

  if (token.compare("==") == 0)
  {
    c = arithmetic_operand::cmp_t::EQ;
  }
  else if (token.compare("!=") == 0)
  {
    c = arithmetic_operand::cmp_t::NE;
  }
  else if (token.compare(">=") == 0)
  {
    c = arithmetic_operand::cmp_t::GE;
  }
  else if (token.compare("<=") == 0)
  {
    c = arithmetic_operand::cmp_t::LE;
  }
  else if (token.compare(">") == 0)
  {
    c = arithmetic_operand::cmp_t::GT;
  }
  else if (token.compare("<") == 0)
  {
    c = arithmetic_operand::cmp_t::LT;
  }
  else
  {
    c = arithmetic_operand::cmp_t::NA;
  }

  self->getAction()->arithmetic_compareQ.push_back(c);
}

void push_logical_operator::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  logical_operand::oplog_t l = logical_operand::oplog_t::NA;

  if (token.compare("and") == 0)
  {
    l = logical_operand::oplog_t::AND;
  }
  else if (token.compare("or") == 0)
  {
    l = logical_operand::oplog_t::OR;
  }
  else
  {
    l = logical_operand::oplog_t::NA;
  }

  self->getAction()->logical_compareQ.push_back(l);
}

void push_arithmetic_predicate::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* vr, *vl;
  arithmetic_operand::cmp_t c = self->getAction()->arithmetic_compareQ.back();
  self->getAction()->arithmetic_compareQ.pop_back();
  vr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  vl = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  arithmetic_operand* t = S3SELECT_NEW(self, arithmetic_operand, vl, c, vr);

  self->getAction()->condQ.push_back(t);
}

void push_logical_predicate::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* tl = 0, *tr = 0;
  logical_operand::oplog_t oplog = self->getAction()->logical_compareQ.back();
  self->getAction()->logical_compareQ.pop_back();

  if (self->getAction()->condQ.empty() == false)
  {
    tr = self->getAction()->condQ.back();
    self->getAction()->condQ.pop_back();
  }
  if (self->getAction()->condQ.empty() == false)
  {
    tl = self->getAction()->condQ.back();
    self->getAction()->condQ.pop_back();
  }

  logical_operand* f = S3SELECT_NEW(self, logical_operand, tl, oplog, tr);

  self->getAction()->condQ.push_back(f);
}

void push_negation::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  base_statement* pred;
  if (self->getAction()->condQ.empty() == false)
  {
    pred = self->getAction()->condQ.back();
    self->getAction()->condQ.pop_back();
  }
  //upon NOT operator, the logical and arithmetical operators are "tagged" to negate result.
  if (dynamic_cast<logical_operand*>(pred))
  {
    logical_operand* f = S3SELECT_NEW(self, logical_operand, pred);
    self->getAction()->condQ.push_back(f);
  }
  else
  {
    arithmetic_operand* f = S3SELECT_NEW(self, arithmetic_operand, pred);
    self->getAction()->condQ.push_back(f);
  }
}

void push_column_pos::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  variable* v;

  if (token.compare("*") == 0 || token.compare("* ") == 0) //TODO space should skip in boost::spirit
  {
    v = S3SELECT_NEW(self, variable, token, variable::var_t::STAR_OPERATION);
  }
  else
  {
    v = S3SELECT_NEW(self, variable, token, variable::var_t::POS);
  }

  self->getAction()->exprQ.push_back(v);
}

void push_projection::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  self->getAction()->projections.get()->push_back(self->getAction()->exprQ.back());
  self->getAction()->exprQ.pop_back();
}

void push_alias_projection::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  //extract alias name
  const char* p = b;
  while (*(--p) != ' ')
    ;
  std::string alias_name(p + 1, b);
  base_statement* bs = self->getAction()->exprQ.back();

  //mapping alias name to base-statement
  bool res = self->getAction()->alias_map.insert_new_entry(alias_name, bs);
  if (res == false)
  {
    throw base_s3select_exception(std::string("alias <") + alias_name + std::string("> is already been used in query"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  self->getAction()->projections.get()->push_back(bs);
  self->getAction()->exprQ.pop_back();
}

void push_debug_1::operator()(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
}

/////// handling different object types
class base_s3object
{

protected:
  scratch_area* m_sa;
  std::string m_obj_name;

public:
  base_s3object(scratch_area* m) : m_sa(m), m_obj_name("") {}

  void set(scratch_area* m)
  {
    m_sa = m;
    m_obj_name = "";
  }

  virtual ~base_s3object() {}
};


class csv_object : public base_s3object
{

public:
  struct csv_defintions
  {
    char row_delimiter;
    char column_delimiter;
    char escape_char;
    char quot_char;
    bool use_header_info;
    bool ignore_header_info;//skip first line

    csv_defintions():row_delimiter('\n'), column_delimiter(','), escape_char('\\'), quot_char('"'), use_header_info(false), ignore_header_info(false) {}

  } m_csv_defintion;

  csv_object(s3select* s3_query) :
    base_s3object(s3_query->get_scratch_area()),
    m_skip_last_line(false),
    m_s3_select(0),
    m_error_count(0),
    m_extract_csv_header_info(false),
    m_previous_line(false),
    m_skip_first_line(false),
    m_processed_bytes(0)
  {
    set(s3_query);
    csv_parser.set(m_csv_defintion.row_delimiter, m_csv_defintion.column_delimiter, m_csv_defintion.quot_char, m_csv_defintion.escape_char);
  }

  csv_object(s3select* s3_query, struct csv_defintions csv) :
    base_s3object(s3_query->get_scratch_area()),
    m_skip_last_line(false),
    m_s3_select(0),
    m_error_count(0),
    m_extract_csv_header_info(false),
    m_previous_line(false),
    m_skip_first_line(false),
    m_processed_bytes(0)
  {
    set(s3_query);
    m_csv_defintion = csv;
    csv_parser.set(m_csv_defintion.row_delimiter, m_csv_defintion.column_delimiter, m_csv_defintion.quot_char, m_csv_defintion.escape_char);
  }

  csv_object():
    base_s3object(0),
    m_skip_last_line(false),
    m_s3_select(0),
    m_error_count(0),
    m_extract_csv_header_info(false),
    m_previous_line(false),
    m_skip_first_line(false),
    m_processed_bytes(0)
  {
    csv_parser.set(m_csv_defintion.row_delimiter, m_csv_defintion.column_delimiter, m_csv_defintion.quot_char, m_csv_defintion.escape_char);
  }

private:
  base_statement* m_where_clause;
  std::vector<base_statement*> m_projections;
  bool m_aggr_flow = false; //TODO once per query
  bool m_is_to_aggregate;
  bool m_skip_last_line;
  size_t m_stream_length;
  std::string m_error_description;
  char* m_stream;
  char* m_end_stream;
  std::vector<char*> m_row_tokens{128};
  s3select* m_s3_select;
  csvParser csv_parser;
  size_t m_error_count;
  bool m_extract_csv_header_info;
  std::vector<std::string> m_csv_schema{128};

  //handling arbitrary chunks (rows cut in the middle)
  bool m_previous_line;
  bool m_skip_first_line;
  std::string merge_line;
  std::string m_last_line;
  size_t m_processed_bytes;

  int getNextRow()
  {
    size_t num_of_tokens=0;

    if(m_stream>=m_end_stream)
    {
      return -1;
    }

    if(csv_parser.parse(m_stream, m_end_stream, &m_row_tokens, &num_of_tokens)<0)
    {
      throw base_s3select_exception("failed to parse csv stream", base_s3select_exception::s3select_exp_en_t::FATAL);
    }

    m_stream = (char*)csv_parser.currentLoc();

    if (m_skip_last_line && m_stream >= m_end_stream)
    {
      return -1;
    }

    return num_of_tokens;

  }

public:

  void set(s3select* s3_query)
  {
    m_s3_select = s3_query;
    base_s3object::set(m_s3_select->get_scratch_area());

    m_projections = m_s3_select->get_projections_list();
    m_where_clause = m_s3_select->get_filter();

    if (m_where_clause)
    {
      m_where_clause->traverse_and_apply(m_sa, m_s3_select->get_aliases());
    }

    for (auto p : m_projections)
    {
      p->traverse_and_apply(m_sa, m_s3_select->get_aliases());
    }

    m_aggr_flow = m_s3_select->is_aggregate_query();
  }


  std::string get_error_description()
  {
    return m_error_description;
  }

  virtual ~csv_object() {}

public:


  int getMatchRow( std::string& result) //TODO virtual ? getResult
  {
    int number_of_tokens = 0;


    if (m_aggr_flow == true)
    {
      do
      {

        number_of_tokens = getNextRow();
        if (number_of_tokens < 0) //end of stream
        {
          if (m_is_to_aggregate)
            for (auto i : m_projections)
            {
              i->set_last_call();
              result.append( i->eval().to_string() );
              result.append(",");
            }

          return number_of_tokens;
        }

        if ((*m_projections.begin())->is_set_last_call())
        {
          //should validate while query execution , no update upon nodes are marked with set_last_call
          throw base_s3select_exception("on aggregation query , can not stream row data post do-aggregate call", base_s3select_exception::s3select_exp_en_t::FATAL);
        }

        m_sa->update(m_row_tokens, number_of_tokens);
        for (auto a : *m_s3_select->get_aliases()->get())
        {
          a.second->invalidate_cache_result();
        }

        if (!m_where_clause || m_where_clause->eval().i64() == true)
          for (auto i : m_projections)
          {
            i->eval();
          }

      }
      while (1);
    }
    else
    {

      do
      {

        number_of_tokens = getNextRow();
        if (number_of_tokens < 0)
        {
          return number_of_tokens;
        }

        m_sa->update(m_row_tokens, number_of_tokens);
        for (auto a : *m_s3_select->get_aliases()->get())
        {
          a.second->invalidate_cache_result();
        }

      }
      while (m_where_clause && m_where_clause->eval().i64() == false);

      for (auto i : m_projections)
      {
        result.append( i->eval().to_string() );
        result.append(",");
      }
      result.append("\n");
    }

    return number_of_tokens; //TODO wrong
  }

  int extract_csv_header_info()
  {

    if (m_csv_defintion.ignore_header_info == true)
    {
      while(*m_stream && (*m_stream != m_csv_defintion.row_delimiter ))
      {
        m_stream++;
      }
      m_stream++;
    }
    else if(m_csv_defintion.use_header_info == true)
    {
      size_t num_of_tokens = getNextRow();//TODO validate number of tokens

      for(size_t i=0; i<num_of_tokens; i++)
      {
        m_csv_schema[i].assign(m_row_tokens[i]);
      }

      m_s3_select->load_schema(m_csv_schema);
    }

    m_extract_csv_header_info = true;

    return 0;
  }

  int run_s3select_on_stream(std::string& result, const char* csv_stream, size_t stream_length, size_t obj_size)
  {
    //purpose: the cv data is "streaming", it may "cut" rows in the middle, in that case the "broken-line" is stores
    //for later, upon next chunk of data is streaming, the stored-line is merge with current broken-line, and processed.
    int status;
    std::string tmp_buff;
    u_int32_t skip_last_bytes = 0;
    m_processed_bytes += stream_length;

    m_skip_first_line = false;

    if (m_previous_line)
    {
      //if previous broken line exist , merge it to current chunk
      char* p_obj_chunk = (char*)csv_stream;
      while (*p_obj_chunk != m_csv_defintion.row_delimiter && p_obj_chunk<(csv_stream+stream_length))
      {
        p_obj_chunk++;
      }

      tmp_buff.assign((char*)csv_stream, (char*)csv_stream + (p_obj_chunk - csv_stream));
      merge_line = m_last_line + tmp_buff + m_csv_defintion.row_delimiter;
      m_previous_line = false;
      m_skip_first_line = true;

      status = run_s3select_on_object(result, merge_line.c_str(), merge_line.length(), false, false, false);
    }

    if (csv_stream[stream_length - 1] != m_csv_defintion.row_delimiter)
    {
      //in case of "broken" last line
      char* p_obj_chunk = (char*)&(csv_stream[stream_length - 1]);
      while (*p_obj_chunk != m_csv_defintion.row_delimiter && p_obj_chunk>csv_stream)
      {
        p_obj_chunk--;  //scan until end-of previous line in chunk
      }

      skip_last_bytes = (&(csv_stream[stream_length - 1]) - p_obj_chunk);
      m_last_line.assign(p_obj_chunk + 1, p_obj_chunk + 1 + skip_last_bytes); //save it for next chunk

      m_previous_line = true;//it means to skip last line

    }

    status = run_s3select_on_object(result, csv_stream, stream_length, m_skip_first_line, m_previous_line, (m_processed_bytes >= obj_size));

    return status;
  }

  int run_s3select_on_object(std::string& result, const char* csv_stream, size_t stream_length, bool skip_first_line, bool skip_last_line, bool do_aggregate)
  {


    m_stream = (char*)csv_stream;
    m_end_stream = (char*)csv_stream + stream_length;
    m_is_to_aggregate = do_aggregate;
    m_skip_last_line = skip_last_line;

    m_stream_length = stream_length;

    if(m_extract_csv_header_info == false)
    {
      extract_csv_header_info();
    }

    if(skip_first_line)
    {
      while(*m_stream && (*m_stream != m_csv_defintion.row_delimiter ))
      {
        m_stream++;
      }
      m_stream++;//TODO nicer
    }

    do
    {

      int num = 0;
      try
      {
        num = getMatchRow(result);
      }
      catch (base_s3select_exception& e)
      {
        std::cout << e.what() << std::endl;
        m_error_description = e.what();
        m_error_count ++;
        if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL || m_error_count>100)//abort query execution
        {
          return -1;
        }
      }

      if (num < 0)
      {
        break;
      }

    }
    while (1);

    return 0;
  }
};

};//namespace

#endif
