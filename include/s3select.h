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
  std::vector<base_statement*> whenThenQ;
  std::vector<std::string> dataTypeQ;
  std::vector<std::string> trimTypeQ;
  projection_alias alias_map;
  std::string from_clause;
  s3select_projections  projections;

  uint64_t in_set_count;

  size_t when_than_count;

  actionQ():in_set_count(0), when_than_count(0){}

  std::map<const void*,std::vector<const char*> *> x_map;

  ~actionQ()
  {
    for(auto m : x_map)
      delete m.second;
  }
  
  bool is_already_scanned(const void *th,const char *a)
  {
    //purpose: caller get indication in the case a specific builder is scan more than once the same text(pointer)
    auto t = x_map.find(th);

    if(t == x_map.end())
    {
      auto v = new std::vector<const char*>;//TODO delete 
      x_map.insert(std::pair<const void*,std::vector<const char*> *>(th,v));
      v->push_back(a);
    }
    else
    {
      for(auto& c : *(t->second))
      {
        if( strcmp(c,a) == 0)
          return true;
      }
      t->second->push_back(a);
    }
    return false;
  }

};

class s3select;

struct base_ast_builder
{
  void operator()(s3select* self, const char* a, const char* b) const;

  virtual void builder(s3select* self, const char* a, const char* b) const = 0;
  
  virtual ~base_ast_builder() = default;
};

struct push_from_clause : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_from_clause g_push_from_clause;

struct push_number : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_number g_push_number;

struct push_float_number : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_float_number g_push_float_number;

struct push_string : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_string g_push_string;

struct push_variable : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_variable g_push_variable;

/////////////////////////arithmetic unit  /////////////////
struct push_addsub : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_addsub g_push_addsub;

struct push_mulop : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_mulop g_push_mulop;

struct push_addsub_binop : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_addsub_binop g_push_addsub_binop;

struct push_mulldiv_binop : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_mulldiv_binop g_push_mulldiv_binop;

struct push_function_arg : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_function_arg g_push_function_arg;

struct push_function_name : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_function_name g_push_function_name;

struct push_function_expr : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_function_expr g_push_function_expr;

struct push_cast_expr : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_cast_expr g_push_cast_expr;

struct push_data_type : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_data_type g_push_data_type;

////////////////////// logical unit ////////////////////////

struct push_compare_operator : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;

};
static push_compare_operator g_push_compare_operator;

struct push_logical_operator : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;

};
static push_logical_operator g_push_logical_operator;

struct push_arithmetic_predicate : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;

};
static push_arithmetic_predicate g_push_arithmetic_predicate;

struct push_logical_predicate : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_logical_predicate g_push_logical_predicate;

struct push_negation : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_negation g_push_negation;

struct push_column_pos : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static  push_column_pos g_push_column_pos;

struct push_projection : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_projection g_push_projection;

struct push_alias_projection : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_alias_projection g_push_alias_projection;

struct push_between_filter : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_between_filter g_push_between_filter;

struct push_in_predicate : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_in_predicate g_push_in_predicate;

struct push_in_predicate_counter : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_in_predicate_counter g_push_in_predicate_counter;

struct push_in_predicate_counter_start : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_in_predicate_counter_start g_push_in_predicate_counter_start;

struct push_like_predicate : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_like_predicate g_push_like_predicate;

struct push_is_null_predicate : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_is_null_predicate g_push_is_null_predicate;

struct push_case_when_else : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_case_when_else g_push_case_when_else;

struct push_when_than : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_when_than g_push_when_than;

struct push_substr_from : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_substr_from g_push_substr_from;

struct push_substr_from_for : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_substr_from_for g_push_substr_from_for;

struct push_trim_type : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_trim_type g_push_trim_type; 

struct push_trim_whitespace_both : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_trim_whitespace_both g_push_trim_whitespace_both;

struct push_trim_expr_one_side_whitespace : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_trim_expr_one_side_whitespace g_push_trim_expr_one_side_whitespace;

struct push_trim_expr_anychar_anyside : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_trim_expr_anychar_anyside g_push_trim_expr_anychar_anyside;

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
    for (const auto &e : get_projections_list())
    {
      e->resolve_node();
      //upon validate there is no aggregation-function nested calls, it validates legit aggregation call. 
      if (e->is_nested_aggregate(aggr_flow))
      {
        error_description = "nested aggregation function is illegal i.e. sum(...sum ...)";
        throw base_s3select_exception(error_description, base_s3select_exception::s3select_exp_en_t::FATAL);
      }
    }

    if (aggr_flow == true)
    {// atleast one projection column contain aggregation function
      for (const auto &e : get_projections_list())
      {
        auto aggregate_expr = e->get_aggregate();

        if (aggregate_expr)
        {
          //per each column, subtree is mark to skip except for the aggregation function subtree. 
          //for an example: substring( ... , sum() , count() ) :: the substring is mark to skip execution, while sum and count not.
          e->set_skip_non_aggregate(true);
          e->mark_aggreagtion_subtree_to_execute();
        }
        else
        {
          //in case projection column is not aggregate, the projection column must *not* contain reference to columns.
          if(e->is_column_reference())
          {
            error_description = "illegal query; projection contains aggregation function is not allowed with projection contains column reference";
            throw base_s3select_exception(error_description, base_s3select_exception::s3select_exp_en_t::FATAL);
          }
        }
        
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

  std::string get_from_clause() const
  {
    return m_actionQ.from_clause;
  }

  void load_schema(std::vector< std::string>& scm)
  {
    int i = 0;
    for (auto& c : scm)
    {
      m_sca.set_column_pos(c.c_str(), i++);
    }
  }

  base_statement* get_filter()
  {
    if(m_actionQ.condQ.empty())
    {
      return nullptr;
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

  bool is_aggregate_query() const
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
    explicit definition(s3select const& self)
    {
      ///// s3select syntax rules and actions for building AST

      select_expr = bsc::str_p("select")  >> projections >> bsc::str_p("from") >> (s3_object)[BOOST_BIND_ACTION(push_from_clause)] >> !where_clause >> ';';

      projections = projection_expression >> *( ',' >> projection_expression) ;

      projection_expression = (when_case_else_projection) [BOOST_BIND_ACTION(push_projection)] | (arithmetic_expression >> bsc::str_p("as") >> alias_name)[BOOST_BIND_ACTION(push_alias_projection)] | (arithmetic_expression)[BOOST_BIND_ACTION(push_projection)];

      alias_name = bsc::lexeme_d[(+bsc::alpha_p >> *bsc::digit_p)] ;


      when_case_else_projection = (bsc::str_p("case")  >> (+when_stmt) >> bsc::str_p("else") >> arithmetic_expression >> bsc::str_p("end")) [BOOST_BIND_ACTION(push_case_when_else)];

      when_stmt = (bsc::str_p("when") >> condition_expression >> bsc::str_p("than") >> arithmetic_expression)[BOOST_BIND_ACTION(push_when_than)];

      s3_object = bsc::str_p("stdin") | bsc::str_p("s3object")  | object_path ;

      object_path = "/" >> *( fs_type >> "/") >> fs_type;

      fs_type = bsc::lexeme_d[+( bsc::alnum_p | bsc::str_p(".")  | bsc::str_p("_")) ];

      where_clause = bsc::str_p("where") >> condition_expression;

      condition_expression =  ( bsc::str_p("not") >> binary_condition )[BOOST_BIND_ACTION(push_negation)] | binary_condition;

      binary_condition = (arithmetic_predicate >> *(log_op[BOOST_BIND_ACTION(push_logical_operator)] >> arithmetic_predicate[BOOST_BIND_ACTION(push_logical_predicate)]));

      arithmetic_predicate = (special_predicates) | (factor >> *(arith_cmp[BOOST_BIND_ACTION(push_compare_operator)] >> factor[BOOST_BIND_ACTION(push_arithmetic_predicate)]));

      special_predicates = (is_null) | (is_not_null) | (between_predicate) | (in_predicate) | (like_predicate) ;

      is_null = ((arithmetic_expression|factor) >> bsc::str_p("is") >> bsc::str_p("null"))[BOOST_BIND_ACTION(push_is_null_predicate)];

      is_not_null = ((arithmetic_expression|factor) >> bsc::str_p("is") >> bsc::str_p("not") >> bsc::str_p("null"))[BOOST_BIND_ACTION(push_is_null_predicate)];

      between_predicate = ( arithmetic_expression >> bsc::str_p("between") >> arithmetic_expression >> bsc::str_p("and") >> arithmetic_expression )[BOOST_BIND_ACTION(push_between_filter)];

      in_predicate = (arithmetic_expression[BOOST_BIND_ACTION(push_in_predicate_counter_start)] >> bsc::str_p("in") >> '(' >> arithmetic_expression[BOOST_BIND_ACTION(push_in_predicate_counter)] >> *(',' >> arithmetic_expression[BOOST_BIND_ACTION(push_in_predicate_counter)]) >> ')')[BOOST_BIND_ACTION(push_in_predicate)];

      like_predicate = (arithmetic_expression >> bsc::str_p("like") >> arithmetic_expression)[BOOST_BIND_ACTION(push_like_predicate)];

      factor = (arithmetic_expression) | ('(' >> condition_expression >> ')') ;

      arithmetic_expression = (addsub_operand >> *(addsubop_operator[BOOST_BIND_ACTION(push_addsub)] >> addsub_operand[BOOST_BIND_ACTION(push_addsub_binop)] ));

      addsub_operand = (mulldiv_operand >> *(muldiv_operator[BOOST_BIND_ACTION(push_mulop)]  >> mulldiv_operand[BOOST_BIND_ACTION(push_mulldiv_binop)] ));// this non-terminal gives precedense to  mull/div

      mulldiv_operand = arithmetic_argument | ('(' >> (arithmetic_expression) >> ')') ;

      list_of_function_arguments = (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)] >> *(',' >> (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)]);
      
      function = ((variable >> '(' )[BOOST_BIND_ACTION(push_function_name)] >> !list_of_function_arguments >> ')')[BOOST_BIND_ACTION(push_function_expr)];

      arithmetic_argument = (float_number)[BOOST_BIND_ACTION(push_float_number)] |  (number)[BOOST_BIND_ACTION(push_number)] | (column_pos)[BOOST_BIND_ACTION(push_column_pos)] |
                            (string)[BOOST_BIND_ACTION(push_string)] |
                            (cast) | (substr) | (trim) |
                            (function) | (variable)[BOOST_BIND_ACTION(push_variable)] ;//function is pushed by right-term

      cast = (bsc::str_p("cast") >> '(' >> arithmetic_expression >> bsc::str_p("as") >> (data_type)[BOOST_BIND_ACTION(push_data_type)] >> ')') [BOOST_BIND_ACTION(push_cast_expr)];

      data_type = (bsc::str_p("int") | bsc::str_p("float") | bsc::str_p("string") |  bsc::str_p("timestamp") );
     
      substr = (substr_from) | (substr_from_for);
      
      substr_from = (bsc::str_p("substring") >> '(' >> (arithmetic_expression >> bsc::str_p("from") >> arithmetic_expression) >> ')') [BOOST_BIND_ACTION(push_substr_from)];

      substr_from_for = (bsc::str_p("substring") >> '(' >> (arithmetic_expression >> bsc::str_p("from") >> arithmetic_expression >> bsc::str_p("for") >> arithmetic_expression) >> ')') [BOOST_BIND_ACTION(push_substr_from_for)];
      
      trim = (trim_whitespace_both) | (trim_one_side_whitespace) | (trim_anychar_anyside);

      trim_one_side_whitespace = (bsc::str_p("trim") >> '(' >> (trim_type)[BOOST_BIND_ACTION(push_trim_type)] >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_trim_expr_one_side_whitespace)];

      trim_whitespace_both = (bsc::str_p("trim") >> '(' >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_trim_whitespace_both)];

      trim_anychar_anyside = (bsc::str_p("trim") >> '(' >> ((trim_remove_type)[BOOST_BIND_ACTION(push_trim_type)] >> arithmetic_expression >> bsc::str_p("from") >> arithmetic_expression)  >> ')') [BOOST_BIND_ACTION(push_trim_expr_anychar_anyside)];
      
      trim_type = ((bsc::str_p("leading") >> bsc::str_p("from")) | ( bsc::str_p("trailing") >> bsc::str_p("from")) | (bsc::str_p("both") >> bsc::str_p("from")) | bsc::str_p("from") ); 

      trim_remove_type = (bsc::str_p("leading") | bsc::str_p("trailing") | bsc::str_p("both") );

      number = bsc::int_p;

      float_number = bsc::real_p;

      string = (bsc::str_p("\"") >> *( bsc::anychar_p - bsc::str_p("\"") ) >> bsc::str_p("\"")) | (bsc::str_p("\'") >> *( bsc::anychar_p - bsc::str_p("\'") ) >> bsc::str_p("\'")) ;

      column_pos = ('_'>>+(bsc::digit_p) ) | '*' ;

      muldiv_operator = bsc::str_p("*") | bsc::str_p("/") | bsc::str_p("^") | bsc::str_p("%");// got precedense

      addsubop_operator = bsc::str_p("+") | bsc::str_p("-");


      arith_cmp = bsc::str_p(">=") | bsc::str_p("<=") | bsc::str_p("==") | bsc::str_p("<") | bsc::str_p(">") | bsc::str_p("!=");

      log_op = bsc::str_p("and") | bsc::str_p("or");

      variable =  bsc::lexeme_d[(+bsc::alpha_p >> *( bsc::alpha_p | bsc::digit_p | '_') )];
    }


    bsc::rule<ScannerT> cast, data_type, variable, trim_type, trim_remove_type, select_expr, s3_object, where_clause, number, float_number, string, arith_cmp, log_op, condition_expression, binary_condition, arithmetic_predicate, factor, trim, trim_whitespace_both, trim_one_side_whitespace, trim_anychar_anyside, substr, substr_from, substr_from_for;
    bsc::rule<ScannerT> special_predicates,between_predicate, in_predicate, like_predicate, is_null, is_not_null;
    bsc::rule<ScannerT> muldiv_operator, addsubop_operator, function, arithmetic_expression, addsub_operand, list_of_function_arguments, arithmetic_argument, mulldiv_operand;
    bsc::rule<ScannerT> fs_type, object_path;
    bsc::rule<ScannerT> projections, projection_expression, alias_name, column_pos;
    bsc::rule<ScannerT> when_case_else_projection, when_stmt;
    bsc::rule<ScannerT> const& start() const
    {
      return select_expr;
    }
  };
};

void base_ast_builder::operator()(s3select *self, const char *a, const char *b) const
{
  //the purpose of the following procedure is to bypass boost::spirit rescan (calling to bind-action more than once per the same text)
  //which cause wrong AST creation (and later false execution).
  if (self->getAction()->is_already_scanned((void *)(this), const_cast<char *>(a)))
    return;

  builder(self, a, b);
}

void push_from_clause::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  self->getAction()->from_clause = token;
}

void push_number::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  variable* v = S3SELECT_NEW(self, variable, atoi(token.c_str()));

  self->getAction()->exprQ.push_back(v);
}

void push_float_number::builder(s3select* self, const char* a, const char* b) const
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

void push_string::builder(s3select* self, const char* a, const char* b) const
{
  a++;
  b--; // remove double quotes
  std::string token(a, b);

  variable* v = S3SELECT_NEW(self, variable, token, variable::var_t::COL_VALUE);

  self->getAction()->exprQ.push_back(v);
}

void push_variable::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  variable* v = nullptr;

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

void push_addsub::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  if (token == "+")
  {
    self->getAction()->addsubQ.push_back(addsub_operation::addsub_op_t::ADD);
  }
  else
  {
    self->getAction()->addsubQ.push_back(addsub_operation::addsub_op_t::SUB);
  }
}

void push_mulop::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  if (token == "*")
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::MULL);
  }
  else if (token == "/")
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::DIV);
  }
  else if(token == "^")
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::POW);
  }
  else
  {
    self->getAction()->muldivQ.push_back(mulldiv_operation::muldiv_t::MOD);
  }
}

void push_addsub_binop::builder(s3select* self, [[maybe_unused]] const char* a,[[maybe_unused]] const char* b) const
{
  base_statement* l = nullptr, *r = nullptr;

  r = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  l = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  addsub_operation::addsub_op_t o = self->getAction()->addsubQ.back();
  self->getAction()->addsubQ.pop_back();
  addsub_operation* as = S3SELECT_NEW(self, addsub_operation, l, o, r);
  self->getAction()->exprQ.push_back(as);
}

void push_mulldiv_binop::builder(s3select* self, [[maybe_unused]] const char* a, [[maybe_unused]] const char* b) const
{
  base_statement* vl = nullptr, *vr = nullptr;

  vr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  vl = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  mulldiv_operation::muldiv_t o = self->getAction()->muldivQ.back();
  self->getAction()->muldivQ.pop_back();
  mulldiv_operation* f = S3SELECT_NEW(self, mulldiv_operation, vl, o, vr);
  self->getAction()->exprQ.push_back(f);
}

void push_function_arg::builder(s3select* self, const char* a, const char* b) const
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

void push_function_name::builder(s3select* self, const char* a, const char* b) const
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

void push_function_expr::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* func = self->getAction()->funcQ.back();
  self->getAction()->funcQ.pop_back();

  self->getAction()->exprQ.push_back(func);
}

void push_compare_operator::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  arithmetic_operand::cmp_t c = arithmetic_operand::cmp_t::NA;

  if (token == "==")
  {
    c = arithmetic_operand::cmp_t::EQ;
  }
  else if (token == "!=")
  {
    c = arithmetic_operand::cmp_t::NE;
  }
  else if (token == ">=")
  {
    c = arithmetic_operand::cmp_t::GE;
  }
  else if (token == "<=")
  {
    c = arithmetic_operand::cmp_t::LE;
  }
  else if (token == ">")
  {
    c = arithmetic_operand::cmp_t::GT;
  }
  else if (token == "<")
  {
    c = arithmetic_operand::cmp_t::LT;
  }

  self->getAction()->arithmetic_compareQ.push_back(c);
}

void push_logical_operator::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  logical_operand::oplog_t l = logical_operand::oplog_t::NA;

  if (token == "and")
  {
    l = logical_operand::oplog_t::AND;
  }
  else if (token == "or")
  {
    l = logical_operand::oplog_t::OR;
  }

  self->getAction()->logical_compareQ.push_back(l);
}

void push_arithmetic_predicate::builder(s3select* self, const char* a, const char* b) const
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

void push_logical_predicate::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* tl = nullptr, *tr = nullptr;
  logical_operand::oplog_t oplog = self->getAction()->logical_compareQ.back();
  self->getAction()->logical_compareQ.pop_back();

  if (self->getAction()->condQ.empty() == false)
  {
    tr = self->getAction()->condQ.back();
    self->getAction()->condQ.pop_back();
  }
  else if(self->getAction()->exprQ.empty() == false)
  {
    tr = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
  }  
  else 
  {//should reject by syntax parser
    throw base_s3select_exception(std::string("missing right operand for logical expression"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  if (self->getAction()->condQ.empty() == false)
  {
    tl = self->getAction()->condQ.back();
    self->getAction()->condQ.pop_back();
  }
  else if(self->getAction()->exprQ.empty() == false)
  {
    tl = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
  } 
  else 
  {//should reject by syntax parser
    throw base_s3select_exception(std::string("missing left operand for logical expression"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  logical_operand* f = S3SELECT_NEW(self, logical_operand, tl, oplog, tr);

  self->getAction()->condQ.push_back(f);
}

void push_negation::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  base_statement* pred = nullptr;

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
  else if (dynamic_cast<__function*>(pred) || dynamic_cast<negate_function_operation*>(pred))
  {
    negate_function_operation* nf = S3SELECT_NEW(self, negate_function_operation, pred);
    self->getAction()->condQ.push_back(nf);
  }
  else
  {
    arithmetic_operand* f = S3SELECT_NEW(self, arithmetic_operand, pred);
    self->getAction()->condQ.push_back(f);
  }
}

void push_column_pos::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  variable* v;

  if (token == "*" || token == "* ") //TODO space should skip in boost::spirit
  {
    v = S3SELECT_NEW(self, variable, token, variable::var_t::STAR_OPERATION);
  }
  else
  {
    v = S3SELECT_NEW(self, variable, token, variable::var_t::POS);
  }

  self->getAction()->exprQ.push_back(v);
}

void push_projection::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  self->getAction()->projections.get()->push_back(self->getAction()->exprQ.back());
  self->getAction()->exprQ.pop_back();
}

void push_alias_projection::builder(s3select* self, const char* a, const char* b) const
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

void push_between_filter::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  std::string between_function("#between#");

  __function* func = S3SELECT_NEW(self, __function, between_function.c_str(), self->getS3F());

  base_statement* second_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(second_expr);

  base_statement* first_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(first_expr);

  base_statement* main_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(main_expr);

  self->getAction()->condQ.push_back(func);
}

void push_in_predicate_counter::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  self->getAction()->in_set_count ++;

}

void push_in_predicate_counter_start::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  self->getAction()->in_set_count = 1;

}

void push_in_predicate::builder(s3select* self, const char* a, const char* b) const
{
  // expr in (e1,e2,e3 ...)
  std::string token(a, b);

  std::string in_function("#in_predicate#");

  __function* func = S3SELECT_NEW(self, __function, in_function.c_str(), self->getS3F());

  while(self->getAction()->in_set_count)
  {
    base_statement* ei = self->getAction()->exprQ.back();

    self->getAction()->exprQ.pop_back();

    func->push_argument(ei);

    self->getAction()->in_set_count --;
  }

  self->getAction()->condQ.push_back(func);
}

void push_like_predicate::builder(s3select* self, const char* a, const char* b) const
{
  // expr like expr ; both should be string, the second expression could be compiled at the
  // time AST is build, which will improve performance much.

  std::string token(a, b);
  std::string in_function("#like_predicate#");

  __function* func = S3SELECT_NEW(self, __function, in_function.c_str(), self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  if (!dynamic_cast<variable*>(expr))
  {
    throw base_s3select_exception("like expression must be a constant string", base_s3select_exception::s3select_exp_en_t::FATAL);
  }
  else if( dynamic_cast<variable*>(expr)->m_var_type != variable::var_t::COL_VALUE)
  {
    throw base_s3select_exception("like expression must be a constant string", base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  func->push_argument(expr);

  base_statement* main_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(main_expr);

  self->getAction()->condQ.push_back(func);
}

void push_is_null_predicate::builder(s3select* self, const char* a, const char* b) const
{
    //expression is null, is not null 
  std::string token(a, b);
  bool is_null = true;

  for(int i=0;i<token.size();i++)
  {//TODO use other scan rules
    bsc::parse_info<> info = bsc::parse(token.c_str()+i, (bsc::str_p("is") >> bsc::str_p("not") >> bsc::str_p("null")) , bsc::space_p);
    if (info.full)
      is_null = false;
  }

  std::string in_function("#is_null#");

  if (is_null == false)
  {
    in_function = "#is_not_null#";
  }

  __function* func = S3SELECT_NEW(self, __function, in_function.c_str(), self->getS3F());

  if (!self->getAction()->exprQ.empty())
  {
    base_statement* expr = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
    func->push_argument(expr);
  }
  else if (!self->getAction()->condQ.empty())
  {
    base_statement* expr = self->getAction()->condQ.back();
    self->getAction()->condQ.pop_back();
    func->push_argument(expr);
  }

  self->getAction()->condQ.push_back(func);
}

void push_when_than::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "#when-than#", self->getS3F());

 base_statement* than_expr = self->getAction()->exprQ.back();
 self->getAction()->exprQ.pop_back();

 base_statement* when_expr = self->getAction()->condQ.back();
 self->getAction()->condQ.pop_back();

 func->push_argument(than_expr);
 func->push_argument(when_expr);

 self->getAction()->whenThenQ.push_back(func);

 self->getAction()->when_than_count ++;
}

void push_case_when_else::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* else_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  __function* func = S3SELECT_NEW(self, __function, "#case-when-else#", self->getS3F());

  func->push_argument(else_expr);

  while(self->getAction()->when_than_count)
  {
    base_statement* when_then_func = self->getAction()->whenThenQ.back();
    self->getAction()->whenThenQ.pop_back();

    func->push_argument(when_then_func);

    self->getAction()->when_than_count--;
  }

// condQ is cleared explicitly, because of "leftover", due to double scanning upon accepting
// the following rule '(' condition-expression ')' , i.e. (3*3 == 12)
// Because of the double-scan (bug in spirit?defintion?), a sub-tree for the left side is created, twice.
// thus, it causes wrong calculation.

  self->getAction()->condQ.clear();

  self->getAction()->exprQ.push_back(func);
}

void push_cast_expr::builder(s3select* self, const char* a, const char* b) const
{
  //cast(expression as int/float/string/timestamp) --> new function "int/float/string/timestamp" ( args = expression )
  std::string token(a, b);
  
  std::string cast_function;

  cast_function = self->getAction()->dataTypeQ.back();
  self->getAction()->dataTypeQ.pop_back();

  __function* func = S3SELECT_NEW(self, __function, cast_function.c_str(), self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}

void push_data_type::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  auto cast_operator = [&](const char *s){return strncmp(a,s,strlen(s))==0;};

  if(cast_operator("int"))
  {
    self->getAction()->dataTypeQ.push_back("int");
  }else if(cast_operator("float"))
  {
    self->getAction()->dataTypeQ.push_back("float");
  }else if(cast_operator("string"))
  {
    self->getAction()->dataTypeQ.push_back("string");
  }else if(cast_operator("timestamp"))
  {
    self->getAction()->dataTypeQ.push_back("timestamp");
  }
}

void push_trim_whitespace_both::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "#trim#", self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}  

void push_trim_expr_one_side_whitespace::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  std::string trim_function;

  trim_function = self->getAction()->trimTypeQ.back();
  self->getAction()->trimTypeQ.pop_back(); 

  __function* func = S3SELECT_NEW(self, __function, trim_function.c_str(), self->getS3F());

  base_statement* inp_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(inp_expr);

  self->getAction()->exprQ.push_back(func);
} 

void push_trim_expr_anychar_anyside::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  std::string trim_function;

  trim_function = self->getAction()->trimTypeQ.back();
  self->getAction()->trimTypeQ.pop_back(); 

  __function* func = S3SELECT_NEW(self, __function, trim_function.c_str(), self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(expr);

  base_statement* inp_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(inp_expr);

  self->getAction()->exprQ.push_back(func);
} 

void push_trim_type::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  auto trim_option = [&](const char *s){return strncmp(a,s,strlen(s))==0;};

  if(trim_option("leading"))
  {
    self->getAction()->trimTypeQ.push_back("#leading#");
  }else if(trim_option("trailing"))
  {
    self->getAction()->trimTypeQ.push_back("#trailing#");
  }else 
  {
    self->getAction()->trimTypeQ.push_back("#trim#");
  }
} 

void push_substr_from::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "substring", self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  base_statement* start_position = self->getAction()->exprQ.back();

  self->getAction()->exprQ.pop_back();
  func->push_argument(start_position);
  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}  

void push_substr_from_for::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "substring", self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  base_statement* start_position = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  base_statement* end_position = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(end_position);
  func->push_argument(start_position);
  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}  

/////// handling different object types
class base_s3object
{

protected:
  scratch_area* m_sa;
  std::string m_obj_name;

public:
  explicit base_s3object(scratch_area* m) : m_sa(m){}

  void set(scratch_area* m)
  {
    m_sa = m;
  }

  virtual ~base_s3object() = default;
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

  explicit csv_object(s3select* s3_query) :
    base_s3object(s3_query->get_scratch_area()),
    m_skip_last_line(false),
    m_s3_select(nullptr),
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
    m_s3_select(nullptr),
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
    base_s3object(nullptr),
    m_skip_last_line(false),
    m_s3_select(nullptr),
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

    for (auto& p : m_projections)
    {
      p->traverse_and_apply(m_sa, m_s3_select->get_aliases());
    }

    m_aggr_flow = m_s3_select->is_aggregate_query();
  }


  std::string get_error_description()
  {
    return m_error_description;
  }

  virtual ~csv_object() = default;

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
            for (auto& i : m_projections)
            {
              i->set_last_call();
              i->set_skip_non_aggregate(false);//projection column is set to be runnable
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
        for (auto& a : *m_s3_select->get_aliases()->get())
        {
          a.second->invalidate_cache_result();
        }

        if (!m_where_clause || m_where_clause->eval().is_true())
          for (auto i : m_projections)
          {
            i->eval();
          }

      }
      while (true);
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
        for (auto& a : *m_s3_select->get_aliases()->get())
        {
          a.second->invalidate_cache_result();
        }

      }
      while (m_where_clause && !m_where_clause->eval().is_true());

      for (auto& i : m_projections)
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
    int status=0;
    try{
        status = run_s3select_on_stream_internal(result,csv_stream,stream_length,obj_size);
    }
    catch(base_s3select_exception& e)
    {
        m_error_description = e.what();
        m_error_count ++;
        if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL || m_error_count>100)//abort query execution
        {
          return -1;
        }
    }
    catch(chunkalloc_out_of_mem)
    {
      m_error_description = "out of memory";
      return -1;
    }

    return status;
  }

private:
  int run_s3select_on_stream_internal(std::string& result, const char* csv_stream, size_t stream_length, size_t obj_size)
  {
    //purpose: the cv data is "streaming", it may "cut" rows in the middle, in that case the "broken-line" is stores
    //for later, upon next chunk of data is streaming, the stored-line is merge with current broken-line, and processed.
    std::string tmp_buff;
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

      run_s3select_on_object(result, merge_line.c_str(), merge_line.length(), false, false, false);
    }

    if (csv_stream[stream_length - 1] != m_csv_defintion.row_delimiter)
    {
      //in case of "broken" last line
      char* p_obj_chunk = (char*)&(csv_stream[stream_length - 1]);
      while (*p_obj_chunk != m_csv_defintion.row_delimiter && p_obj_chunk>csv_stream)
      {
        p_obj_chunk--;  //scan until end-of previous line in chunk
      }

      u_int32_t skip_last_bytes = (&(csv_stream[stream_length - 1]) - p_obj_chunk);
      m_last_line.assign(p_obj_chunk + 1, p_obj_chunk + 1 + skip_last_bytes); //save it for next chunk

      m_previous_line = true;//it means to skip last line

    }

    return run_s3select_on_object(result, csv_stream, stream_length, m_skip_first_line, m_previous_line, (m_processed_bytes >= obj_size));

  }

public:
  int run_s3select_on_object(std::string& result, const char* csv_stream, size_t stream_length, bool skip_first_line, bool skip_last_line, bool do_aggregate)
  {


    m_stream = (char*)csv_stream;
    m_end_stream = (char*)csv_stream + stream_length;
    m_is_to_aggregate = do_aggregate;
    m_skip_last_line = skip_last_line;

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
        if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL || m_error_count>100 || (m_stream>=m_end_stream))//abort query execution
        {
          return -1;
        }
      }

      if (num < 0)
      {
        break;
      }

    }
    while (true);

    return 0;
  }
};

};//namespace

#endif
