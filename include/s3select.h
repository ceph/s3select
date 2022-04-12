#ifndef __S3SELECT__
#define __S3SELECT__
#define BOOST_SPIRIT_THREADSAFE

#pragma once
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
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
  std::vector<base_statement*> whenThenQ;
  std::vector<base_statement*> inPredicateQ;
  base_statement* inMainArg;
  std::vector<std::string> dataTypeQ;
  std::vector<std::string> trimTypeQ;
  std::vector<std::string> datePartQ;
  std::vector<base_statement*> caseValueQ;
  projection_alias alias_map;
  std::string from_clause;
  std::string column_prefix;
  std::string table_alias;
  s3select_projections  projections;

  bool projection_or_predicate_state; //true->projection false->predicate(where-clause statement)
  std::vector<base_statement*> predicate_columns;
  std::vector<base_statement*> projections_columns; 

  size_t when_then_count;

  actionQ(): inMainArg(0),from_clause("##"),column_prefix("##"),table_alias("##"),projection_or_predicate_state(true),when_then_count(0){}//TODO remove when_then_count

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

struct push_not_between_filter : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_not_between_filter g_push_not_between_filter;

struct push_in_predicate : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_in_predicate g_push_in_predicate;

struct push_in_predicate_arguments : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_in_predicate_arguments g_push_in_predicate_arguments;

struct push_in_predicate_first_arg : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_in_predicate_first_arg g_push_in_predicate_first_arg;

struct push_like_predicate_escape : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_like_predicate_escape g_push_like_predicate_escape;

struct push_like_predicate_no_escape : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_like_predicate_no_escape g_push_like_predicate_no_escape;

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

struct push_when_condition_then : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_when_condition_then g_push_when_condition_then;

struct push_when_value_then : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_when_value_then g_push_when_value_then;

struct push_case_value : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_case_value g_push_case_value;

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

struct push_datediff : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_datediff g_push_datediff;

struct push_dateadd : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_dateadd g_push_dateadd;

struct push_extract : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_extract g_push_extract;

struct push_date_part : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_date_part g_push_date_part;

struct push_time_to_string_constant : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_time_to_string_constant g_push_time_to_string_constant;

struct push_time_to_string_dynamic : public base_ast_builder
{
  void builder(s3select* self, const char* a, const char* b) const;
};
static push_time_to_string_dynamic g_push_time_to_string_dynamic;

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
        error_description = std::string("failure -->") + query_parse_position + std::string("<---");
        return -1;
      }

      semantic();
    }
    catch (base_s3select_exception& e)
    {
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
    if(m_actionQ.exprQ.empty())
    {
      return nullptr;
    }

    return m_actionQ.exprQ.back();
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

//the input is converted to lower case
#define S3SELECT_KW( reserve_word ) bsc::as_lower_d[ reserve_word ]

  template <typename ScannerT>
  struct definition
  {
    explicit definition(s3select const& self)
    {
      ///// s3select syntax rules and actions for building AST

      select_expr =  (select_expr_base >> ';') | select_expr_base;

      select_expr_base =  S3SELECT_KW("select") >> projections >> S3SELECT_KW("from") >> (from_expression)[BOOST_BIND_ACTION(push_from_clause)] >> !where_clause ;

      projections = projection_expression >> *( ',' >> projection_expression) ;

      projection_expression = (when_case_else_projection|when_case_value_when) [BOOST_BIND_ACTION(push_projection)] | 
                              (arithmetic_expression >> S3SELECT_KW("as") >> alias_name)[BOOST_BIND_ACTION(push_alias_projection)] | 
                              (arithmetic_expression)[BOOST_BIND_ACTION(push_projection)] | 
			      (arithmetic_predicate >> S3SELECT_KW("as") >> alias_name)[BOOST_BIND_ACTION(push_alias_projection)] |
                              (arithmetic_predicate)[BOOST_BIND_ACTION(push_projection)] ;

      alias_name = bsc::lexeme_d[(+bsc::alpha_p >> *bsc::digit_p)] ;

      when_case_else_projection = (S3SELECT_KW("case")  >> (+when_stmt) >> S3SELECT_KW("else") >> arithmetic_expression >> S3SELECT_KW("end")) [BOOST_BIND_ACTION(push_case_when_else)];

      when_stmt = (S3SELECT_KW("when") >> condition_expression >> S3SELECT_KW("then") >> arithmetic_expression)[BOOST_BIND_ACTION(push_when_condition_then)];

      when_case_value_when = (S3SELECT_KW("case") >> arithmetic_expression[BOOST_BIND_ACTION(push_case_value)]  >> 
                              (+when_value_then) >> S3SELECT_KW("else") >> arithmetic_expression >> S3SELECT_KW("end")) [BOOST_BIND_ACTION(push_case_when_else)];

      when_value_then = (S3SELECT_KW("when") >> arithmetic_expression >> S3SELECT_KW("then") >> arithmetic_expression)[BOOST_BIND_ACTION(push_when_value_then)];

      from_expression = (s3_object >> (variable - S3SELECT_KW("where"))) | s3_object;

      //the stdin and object_path are for debug purposes(not part of the specs)
      s3_object = S3SELECT_KW("stdin") | S3SELECT_KW("s3object") | object_path ;

      object_path = "/" >> *( fs_type >> "/") >> fs_type;

      fs_type = bsc::lexeme_d[+( bsc::alnum_p | bsc::str_p(".")  | bsc::str_p("_")) ];

      where_clause = S3SELECT_KW("where") >> condition_expression;

      condition_expression = arithmetic_predicate;

      arithmetic_predicate = (S3SELECT_KW("not") >> logical_predicate)[BOOST_BIND_ACTION(push_negation)] | logical_predicate;

      logical_predicate =  (logical_and) >> *(or_op[BOOST_BIND_ACTION(push_logical_operator)] >> (logical_and)[BOOST_BIND_ACTION(push_logical_predicate)]);

      logical_and =  (cmp_operand) >> *(and_op[BOOST_BIND_ACTION(push_logical_operator)] >> (cmp_operand)[BOOST_BIND_ACTION(push_logical_predicate)]);

      cmp_operand = special_predicates | (factor) >> *(arith_cmp[BOOST_BIND_ACTION(push_compare_operator)] >> (factor)[BOOST_BIND_ACTION(push_arithmetic_predicate)]);

      special_predicates = (is_null) | (is_not_null) | (between_predicate) | (not_between) | (in_predicate) | (like_predicate);

      is_null = ((factor) >> S3SELECT_KW("is") >> S3SELECT_KW("null"))[BOOST_BIND_ACTION(push_is_null_predicate)];

      is_not_null = ((factor) >> S3SELECT_KW("is") >> S3SELECT_KW("not") >> S3SELECT_KW("null"))[BOOST_BIND_ACTION(push_is_null_predicate)];

      between_predicate = (arithmetic_expression >> S3SELECT_KW("between") >> arithmetic_expression >> S3SELECT_KW("and") >> arithmetic_expression)[BOOST_BIND_ACTION(push_between_filter)];

      not_between = (arithmetic_expression >> S3SELECT_KW("not") >> S3SELECT_KW("between") >> arithmetic_expression >> S3SELECT_KW("and") >> arithmetic_expression)[BOOST_BIND_ACTION(push_not_between_filter)];

      in_predicate = (arithmetic_expression >> S3SELECT_KW("in") >> '(' >> arithmetic_expression[BOOST_BIND_ACTION(push_in_predicate_first_arg)] >> *(',' >> arithmetic_expression[BOOST_BIND_ACTION(push_in_predicate_arguments)]) >> ')')[BOOST_BIND_ACTION(push_in_predicate)];
      
      like_predicate = (like_predicate_escape) |(like_predicate_no_escape);

      like_predicate_no_escape = (arithmetic_expression >> S3SELECT_KW("like") >> arithmetic_expression)[BOOST_BIND_ACTION(push_like_predicate_no_escape)];

      like_predicate_escape = (arithmetic_expression >> S3SELECT_KW("like") >> arithmetic_expression >> S3SELECT_KW("escape") >> arithmetic_expression)[BOOST_BIND_ACTION(push_like_predicate_escape)];

      factor = arithmetic_expression  | ( '(' >> arithmetic_predicate >> ')' ) ; 

      arithmetic_expression = (addsub_operand >> *(addsubop_operator[BOOST_BIND_ACTION(push_addsub)] >> addsub_operand[BOOST_BIND_ACTION(push_addsub_binop)] ));

      addsub_operand = (mulldiv_operand >> *(muldiv_operator[BOOST_BIND_ACTION(push_mulop)]  >> mulldiv_operand[BOOST_BIND_ACTION(push_mulldiv_binop)] ));// this non-terminal gives precedense to  mull/div

      mulldiv_operand = arithmetic_argument | ('(' >> (arithmetic_expression) >> ')') ;

      list_of_function_arguments = (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)] >> *(',' >> (arithmetic_expression)[BOOST_BIND_ACTION(push_function_arg)]);
      
      function = ((variable >> '(' )[BOOST_BIND_ACTION(push_function_name)] >> !list_of_function_arguments >> ')')[BOOST_BIND_ACTION(push_function_expr)];

      arithmetic_argument = (float_number)[BOOST_BIND_ACTION(push_float_number)] |  (number)[BOOST_BIND_ACTION(push_number)] | (column_pos)[BOOST_BIND_ACTION(push_column_pos)] |
                            (string)[BOOST_BIND_ACTION(push_string)] | (datediff) | (dateadd) | (extract) | (time_to_string_constant) | (time_to_string_dynamic) |
                            (cast) | (substr) | (trim) |
                            (function) | (variable)[BOOST_BIND_ACTION(push_variable)]; //function is pushed by right-term

      cast = (S3SELECT_KW("cast") >> '(' >> arithmetic_expression >> S3SELECT_KW("as") >> (data_type)[BOOST_BIND_ACTION(push_data_type)] >> ')') [BOOST_BIND_ACTION(push_cast_expr)];

      data_type = (S3SELECT_KW("int") | S3SELECT_KW("float") | S3SELECT_KW("string") |  S3SELECT_KW("timestamp") | S3SELECT_KW("bool") );
     
      substr = (substr_from) | (substr_from_for);
      
      substr_from = (S3SELECT_KW("substring") >> '(' >> (arithmetic_expression >> S3SELECT_KW("from") >> arithmetic_expression) >> ')') [BOOST_BIND_ACTION(push_substr_from)];

      substr_from_for = (S3SELECT_KW("substring") >> '(' >> (arithmetic_expression >> S3SELECT_KW("from") >> arithmetic_expression >> S3SELECT_KW("for") >> arithmetic_expression) >> ')') [BOOST_BIND_ACTION(push_substr_from_for)];
      
      trim = (trim_whitespace_both) | (trim_one_side_whitespace) | (trim_anychar_anyside);

      trim_one_side_whitespace = (S3SELECT_KW("trim") >> '(' >> (trim_type)[BOOST_BIND_ACTION(push_trim_type)] >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_trim_expr_one_side_whitespace)];

      trim_whitespace_both = (S3SELECT_KW("trim") >> '(' >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_trim_whitespace_both)];

      trim_anychar_anyside = (S3SELECT_KW("trim") >> '(' >> ((trim_remove_type)[BOOST_BIND_ACTION(push_trim_type)] >> arithmetic_expression >> S3SELECT_KW("from") >> arithmetic_expression)  >> ')') [BOOST_BIND_ACTION(push_trim_expr_anychar_anyside)];
      
      trim_type = ((S3SELECT_KW("leading") >> S3SELECT_KW("from")) | ( S3SELECT_KW("trailing") >> S3SELECT_KW("from")) | (S3SELECT_KW("both") >> S3SELECT_KW("from")) | S3SELECT_KW("from") ); 

      trim_remove_type = (S3SELECT_KW("leading") | S3SELECT_KW("trailing") | S3SELECT_KW("both") );

      datediff = (S3SELECT_KW("date_diff") >> '(' >> date_part >> ',' >> arithmetic_expression >> ',' >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_datediff)];

      dateadd = (S3SELECT_KW("date_add") >> '(' >> date_part >> ',' >> arithmetic_expression >> ',' >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_dateadd)];

      extract = (S3SELECT_KW("extract") >> '(' >> (date_part_extract)[BOOST_BIND_ACTION(push_date_part)] >> S3SELECT_KW("from") >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_extract)];

      date_part = (S3SELECT_KW("year") | S3SELECT_KW("month") | S3SELECT_KW("day") | S3SELECT_KW("hour")  | S3SELECT_KW("minute") | S3SELECT_KW("second")) [BOOST_BIND_ACTION(push_date_part)];

      date_part_extract = ((date_part) |  S3SELECT_KW("week") | S3SELECT_KW("timezone_hour") | S3SELECT_KW("timezone_minute"));

      time_to_string_constant = (S3SELECT_KW("to_string") >> '(' >> arithmetic_expression >> ',' >> (string)[BOOST_BIND_ACTION(push_string)] >> ')') [BOOST_BIND_ACTION(push_time_to_string_constant)];

      time_to_string_dynamic = (S3SELECT_KW("to_string") >> '(' >> arithmetic_expression >> ',' >> arithmetic_expression >> ')') [BOOST_BIND_ACTION(push_time_to_string_dynamic)];

      number = bsc::int_p;

      float_number = bsc::real_p;

      string = (bsc::str_p("\"") >> *( bsc::anychar_p - bsc::str_p("\"") ) >> bsc::str_p("\"")) | (bsc::str_p("\'") >> *( bsc::anychar_p - bsc::str_p("\'") ) >> bsc::str_p("\'")) ;

      column_pos = (variable_name >> "." >> column_pos_name) | column_pos_name; //TODO what about space

      column_pos_name = ('_'>>+(bsc::digit_p) ) | '*' ;

      muldiv_operator = bsc::str_p("*") | bsc::str_p("/") | bsc::str_p("^") | bsc::str_p("%");// got precedense

      addsubop_operator = bsc::str_p("+") | bsc::str_p("-");

      arith_cmp = bsc::str_p(">=") | bsc::str_p("<=") | bsc::str_p("=") | bsc::str_p("<") | bsc::str_p(">") | bsc::str_p("!=");

      and_op =  S3SELECT_KW("and");

      or_op =  S3SELECT_KW("or");

      variable_name =  bsc::lexeme_d[(+bsc::alpha_p >> *( bsc::alpha_p | bsc::digit_p | '_') ) -  S3SELECT_KW("not")];

      variable = (variable_name >> "." >> variable_name) | variable_name;
    }


    bsc::rule<ScannerT> cast, data_type, variable,  variable_name, select_expr, select_expr_base, s3_object, where_clause, number, float_number, string, from_expression;
    bsc::rule<ScannerT> cmp_operand, arith_cmp, condition_expression, arithmetic_predicate, logical_predicate, factor; 
    bsc::rule<ScannerT> trim, trim_whitespace_both, trim_one_side_whitespace, trim_anychar_anyside, trim_type, trim_remove_type, substr, substr_from, substr_from_for;
    bsc::rule<ScannerT> datediff, dateadd, extract, date_part, date_part_extract, time_to_string_constant, time_to_string_dynamic;
    bsc::rule<ScannerT> special_predicates, between_predicate, not_between, in_predicate, like_predicate, like_predicate_escape, like_predicate_no_escape, is_null, is_not_null;
    bsc::rule<ScannerT> muldiv_operator, addsubop_operator, function, arithmetic_expression, addsub_operand, list_of_function_arguments, arithmetic_argument, mulldiv_operand;
    bsc::rule<ScannerT> fs_type, object_path;
    bsc::rule<ScannerT> projections, projection_expression, alias_name, column_pos,column_pos_name;
    bsc::rule<ScannerT> when_case_else_projection, when_case_value_when, when_stmt, when_value_then;
    bsc::rule<ScannerT> logical_and,and_op,or_op;
    bsc::rule<ScannerT> const& start() const
    {
      return  select_expr ;
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
  std::string token(a, b),table_name,alias_name;

  //should search for generic space
  if(token.find(' ') != std::string::npos)
  {
    size_t pos = token.find(' '); 
    table_name = token.substr(0,pos);
    
    pos = token.rfind(' ');
    alias_name = token.substr(pos+1,token.size());

    self->getAction()->table_alias = alias_name;

    if(self->getAction()->column_prefix != "##" && self->getAction()->table_alias != self->getAction()->column_prefix)
    {
      throw base_s3select_exception(std::string("query can not contain more then a single table-alias"), base_s3select_exception::s3select_exp_en_t::FATAL);
    }

    token = table_name;
  }

  self->getAction()->from_clause = token; //TODO add table alias 

  self->getAction()->exprQ.clear();

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
    else if (g_s3select_reserve_word.get_reserved_word(token) == s3select_reserved_word::reserve_word_en_t::S3S_FALSE)
    {
      v = S3SELECT_NEW(self, variable, s3select_reserved_word::reserve_word_en_t::S3S_FALSE);
    }
    else if (g_s3select_reserve_word.get_reserved_word(token) == s3select_reserved_word::reserve_word_en_t::S3S_TRUE)
    {
      v = S3SELECT_NEW(self, variable, s3select_reserved_word::reserve_word_en_t::S3S_TRUE);
    }
    else
    {
      v = S3SELECT_NEW(self, variable, s3select_reserved_word::reserve_word_en_t::NA);
    }
    
  }
  else
  {
    size_t pos = token.find('.');
    std::string alias_name;
    if(pos != std::string::npos)
    {
      alias_name = token.substr(0,pos);
      pos ++;
      token = token.substr(pos,token.size());

      if(self->getAction()->column_prefix != "##" && alias_name != self->getAction()->column_prefix)
      {
        throw base_s3select_exception(std::string("query can not contain more then a single table-alias"), base_s3select_exception::s3select_exp_en_t::FATAL);
      }

      self->getAction()->column_prefix = alias_name;
    }
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

  if (token == "=")
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

  if (!self->getAction()->exprQ.empty())
  {
    vr = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
  }
  else
  {
    throw base_s3select_exception(std::string("missing right operand for arithmetic-comparision expression"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }
  
  if (!self->getAction()->exprQ.empty())
  {
    vl = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
  }
  else
  {
    throw base_s3select_exception(std::string("missing left operand for arithmetic-comparision expression"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }
  
  arithmetic_operand* t = S3SELECT_NEW(self, arithmetic_operand, vl, c, vr);

  self->getAction()->exprQ.push_back(t);
}

void push_logical_predicate::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* tl = nullptr, *tr = nullptr;
  logical_operand::oplog_t oplog = self->getAction()->logical_compareQ.back();
  self->getAction()->logical_compareQ.pop_back();

  if (self->getAction()->exprQ.empty() == false)
  {
    tr = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
  }  
  else 
  {//should reject by syntax parser
    throw base_s3select_exception(std::string("missing right operand for logical expression"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  if (self->getAction()->exprQ.empty() == false)
  {
    tl = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
  } 
  else 
  {//should reject by syntax parser
    throw base_s3select_exception(std::string("missing left operand for logical expression"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  logical_operand* f = S3SELECT_NEW(self, logical_operand, tl, oplog, tr);

  self->getAction()->exprQ.push_back(f);
}

void push_negation::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  base_statement* pred = nullptr;

  if (self->getAction()->exprQ.empty() == false)
  {
    pred = self->getAction()->exprQ.back();
    self->getAction()->exprQ.pop_back();
  }
  else
  {
    throw base_s3select_exception(std::string("failed to create AST for NOT operator"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }
  
  //upon NOT operator, the logical and arithmetical operators are "tagged" to negate result.
  if (dynamic_cast<logical_operand*>(pred))
  {
    logical_operand* f = S3SELECT_NEW(self, logical_operand, pred);
    self->getAction()->exprQ.push_back(f);
  }
  else if (dynamic_cast<__function*>(pred) || dynamic_cast<negate_function_operation*>(pred) || dynamic_cast<variable*>(pred))
  {
    negate_function_operation* nf = S3SELECT_NEW(self, negate_function_operation, pred);
    self->getAction()->exprQ.push_back(nf);
  }
  else if(dynamic_cast<arithmetic_operand*>(pred))
  {
    arithmetic_operand* f = S3SELECT_NEW(self, arithmetic_operand, pred);
    self->getAction()->exprQ.push_back(f);
  }
  else
  {
    throw base_s3select_exception(std::string("failed to create AST for NOT operator"), base_s3select_exception::s3select_exp_en_t::FATAL);
  }
}

void push_column_pos::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  std::string alias_name;
  variable* v;

  if (token == "*" || token == "* ") //TODO space should skip in boost::spirit
  {
    v = S3SELECT_NEW(self, variable, token, variable::var_t::STAR_OPERATION);

    //NOTE: variable may leak upon star-operation(multi_value object is not destruct entirly, it contain stl-vactor which is allocated on heap).
    //TODO: find a generic way for such use-cases, one possible solution is to push all-nodes(upon AST is complete) into cleanup-container.
    self->getS3F()->push_for_cleanup(v);
  }
  else
  {
    size_t pos = token.find('.');
    if(pos != std::string::npos)
    {
      alias_name = token.substr(0,pos);

      pos ++;
      token = token.substr(pos,token.size());

      if(self->getAction()->column_prefix != "##" && self->getAction()->column_prefix != alias_name)
      {
        throw base_s3select_exception(std::string("query can not contain more then a single table-alias"), base_s3select_exception::s3select_exp_en_t::FATAL);
      }
    
      self->getAction()->column_prefix = alias_name;
    }
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

  self->getAction()->exprQ.push_back(func);
}

void push_not_between_filter::builder(s3select* self, const char* a, const char* b) const
{

  static constexpr const std::string_view not_between_function("#not_between#");

  __function* func = S3SELECT_NEW(self, __function, not_between_function.data(), self->getS3F());

  base_statement* second_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(second_expr);

  base_statement* first_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(first_expr);

  base_statement* main_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(main_expr);

  self->getAction()->exprQ.push_back(func);
}

void push_in_predicate_first_arg::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  if(self->getAction()->exprQ.empty())
  {
    throw base_s3select_exception("failed to create AST for in predicate", base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  self->getAction()->inPredicateQ.push_back( self->getAction()->exprQ.back() );
  self->getAction()->exprQ.pop_back();

  if(self->getAction()->exprQ.empty())
  {
    throw base_s3select_exception("failed to create AST for in predicate", base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  self->getAction()->inMainArg = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();


}

void push_in_predicate_arguments::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  if(self->getAction()->exprQ.empty())
  {
    throw base_s3select_exception("failed to create AST for in predicate", base_s3select_exception::s3select_exp_en_t::FATAL);
  }

  self->getAction()->inPredicateQ.push_back( self->getAction()->exprQ.back() );

  self->getAction()->exprQ.pop_back();

}

void push_in_predicate::builder(s3select* self, const char* a, const char* b) const
{
  // expr in (e1,e2,e3 ...)
  std::string token(a, b);

  std::string in_function("#in_predicate#");

  __function* func = S3SELECT_NEW(self, __function, in_function.c_str(), self->getS3F());

  while(!self->getAction()->inPredicateQ.empty())
  {
    base_statement* ei = self->getAction()->inPredicateQ.back();

    self->getAction()->inPredicateQ.pop_back();

    func->push_argument(ei);

  }

  func->push_argument( self->getAction()->inMainArg );

  self->getAction()->exprQ.push_back(func);

  self->getAction()->inPredicateQ.clear();

  self->getAction()->inMainArg = 0;
}

void push_like_predicate_no_escape::builder(s3select* self, const char* a, const char* b) const
{

  std::string token(a, b);
  std::string in_function("#like_predicate#");

  __function* func = S3SELECT_NEW(self, __function, in_function.c_str(), self->getS3F());
  
  variable* v = S3SELECT_NEW(self, variable, "\\",variable::var_t::COL_VALUE);
  func->push_argument(v);
  
  // experimenting valgrind-issue happens only on teuthology
  //self->getS3F()->push_for_cleanup(v);
  
  base_statement* like_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(like_expr);  

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}

void push_like_predicate_escape::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);
  std::string in_function("#like_predicate#");

  __function* func = S3SELECT_NEW(self, __function, in_function.c_str(), self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(expr);

  base_statement* main_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(main_expr);

  base_statement* escape_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();
  func->push_argument(escape_expr);

  self->getAction()->exprQ.push_back(func);
}

void push_is_null_predicate::builder(s3select* self, const char* a, const char* b) const
{
    //expression is null, is not null 
  std::string token(a, b);
  bool is_null = true;

  for(size_t i=0;i<token.size();i++)
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

  self->getAction()->exprQ.push_back(func);
}

void push_when_condition_then::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "#when-then#", self->getS3F());

 base_statement* then_expr = self->getAction()->exprQ.back();
 self->getAction()->exprQ.pop_back();

 base_statement* when_expr = self->getAction()->exprQ.back();
 self->getAction()->exprQ.pop_back();

 func->push_argument(then_expr);
 func->push_argument(when_expr);

 self->getAction()->whenThenQ.push_back(func);

 self->getAction()->when_then_count ++;
}

void push_case_when_else::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* else_expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  __function* func = S3SELECT_NEW(self, __function, "#case-when-else#", self->getS3F());

  func->push_argument(else_expr);

  while(self->getAction()->when_then_count)
  {
    base_statement* when_then_func = self->getAction()->whenThenQ.back();
    self->getAction()->whenThenQ.pop_back();

    func->push_argument(when_then_func);

    self->getAction()->when_then_count--;
  }

// condQ is cleared explicitly, because of "leftover", due to double scanning upon accepting
// the following rule '(' condition-expression ')' , i.e. (3*3 == 12)
// Because of the double-scan (bug in spirit?defintion?), a sub-tree for the left side is created, twice.
// thus, it causes wrong calculation.

  self->getAction()->exprQ.clear();

  self->getAction()->exprQ.push_back(func);
}

void push_case_value::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  base_statement* case_value = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  self->getAction()->caseValueQ.push_back(case_value);
}

void push_when_value_then::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "#when-value-then#", self->getS3F());

 base_statement* then_expr = self->getAction()->exprQ.back();
 self->getAction()->exprQ.pop_back();

 base_statement* when_expr = self->getAction()->exprQ.back();
 self->getAction()->exprQ.pop_back();

 base_statement* case_expr = self->getAction()->caseValueQ.back();

 func->push_argument(then_expr);
 func->push_argument(when_expr);
 func->push_argument(case_expr);

 self->getAction()->whenThenQ.push_back(func);

 self->getAction()->when_then_count ++;
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
    self->getAction()->dataTypeQ.push_back("to_timestamp");
  }else if(cast_operator("bool"))
  {
    self->getAction()->dataTypeQ.push_back("to_bool");
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

void push_datediff::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  std::string date_op;

  date_op = self->getAction()->datePartQ.back();
  self->getAction()->datePartQ.pop_back();

  std::string date_function =  "#datediff_" + date_op + "#";

  __function* func = S3SELECT_NEW(self, __function, date_function.c_str(), self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  base_statement* start_position = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(start_position);
  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}

void push_dateadd::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  std::string date_op;

  date_op = self->getAction()->datePartQ.back();
  self->getAction()->datePartQ.pop_back();

  std::string date_function =  "#dateadd_" + date_op + "#";

  __function* func = S3SELECT_NEW(self, __function, date_function.c_str(), self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  base_statement* start_position = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(start_position);
  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}

void push_extract::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  std::string date_op;

  date_op = self->getAction()->datePartQ.back();
  self->getAction()->datePartQ.pop_back();

  std::string date_function =  "#extract_" + date_op + "#";

  __function* func = S3SELECT_NEW(self, __function, date_function.c_str(), self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);
}

void push_date_part::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  self->getAction()->datePartQ.push_back(token);
}

void push_time_to_string_constant::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "#to_string_constant#", self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  base_statement* frmt = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(frmt);
  func->push_argument(expr);

  self->getAction()->exprQ.push_back(func);

}

void push_time_to_string_dynamic::builder(s3select* self, const char* a, const char* b) const
{
  std::string token(a, b);

  __function* func = S3SELECT_NEW(self, __function, "#to_string_dynamic#", self->getS3F());

  base_statement* expr = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  base_statement* frmt = self->getAction()->exprQ.back();
  self->getAction()->exprQ.pop_back();

  func->push_argument(frmt);
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
    char output_row_delimiter;
    char output_column_delimiter;
    char escape_char;
    char output_escape_char;
    char output_quot_char;
    char quot_char;
    bool use_header_info;
    bool ignore_header_info;//skip first line
    bool quote_fields_always;
    bool quote_fields_asneeded;
    bool redundant_column;

    csv_defintions():row_delimiter('\n'), column_delimiter(','), output_row_delimiter('\n'), output_column_delimiter(','), escape_char('\\'), output_escape_char('\\'), output_quot_char('"'), quot_char('"'), use_header_info(false), ignore_header_info(false), quote_fields_always(false), quote_fields_asneeded(false), redundant_column(false) {}

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

  void set_csv_query(s3select* s3_query,struct csv_defintions csv)
  {
    if(m_s3_select != nullptr) 
    {
      return;
    }

    set(s3_query);
    m_csv_defintion = csv;
    csv_parser.set(m_csv_defintion.row_delimiter, m_csv_defintion.column_delimiter, m_csv_defintion.quot_char, m_csv_defintion.escape_char);
  }

  std::string get_error_description()
  {
    return m_error_description;
  }

  virtual ~csv_object() = default;

public:

  void result_values_to_string(multi_values& projections_resuls, std::string& result)
  {
    size_t i = 0;
    std::string output_delimiter(1,m_csv_defintion.output_column_delimiter);

    for(auto res : projections_resuls.values)
    {
            if (m_csv_defintion.quote_fields_always) {
              std::ostringstream quoted_result;
              quoted_result << std::quoted(res->to_string(),m_csv_defintion.output_quot_char, m_csv_defintion.escape_char);
              result.append(quoted_result.str());
            }//TODO to add asneeded
	    else
	    {
            	result.append( res->to_string() );
	    }

            if(!m_csv_defintion.redundant_column) {
              if(++i < projections_resuls.values.size()) {
                result.append(output_delimiter);
              }
            }
            else {
              result.append(output_delimiter);
            }    
    }
  }

  int getMatchRow( std::string& result) //TODO virtual ? getResult
  {
    int number_of_tokens = 0;
    std::string output_delimiter(1,m_csv_defintion.output_row_delimiter);
    multi_values projections_resuls;
    


    if (m_aggr_flow == true)
    {
      do
      {

        number_of_tokens = getNextRow();
        if (number_of_tokens < 0) //end of stream
        {
          projections_resuls.clear();
          if (m_is_to_aggregate)
            for (auto& i : m_projections)
            {
              i->set_last_call();
              i->set_skip_non_aggregate(false);//projection column is set to be runnable

              projections_resuls.push_value( &(i->eval()) );
            }

          result_values_to_string(projections_resuls,result);
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

      projections_resuls.clear();
      for (auto& i : m_projections)
      {
        projections_resuls.push_value( &(i->eval()) );
      }
      result_values_to_string(projections_resuls,result);
      result.append(output_delimiter);
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

#ifdef _ARROW_EXIST
class parquet_object : public base_s3object
{

private:
  base_statement *m_where_clause;
  std::vector<base_statement *> m_projections;
  bool m_aggr_flow = false; //TODO once per query
  bool m_is_to_aggregate;
  std::string m_error_description;
  s3select *m_s3_select;
  size_t m_error_count;
  parquet_file_parser* object_reader;
  parquet_file_parser::column_pos_t m_where_clause_columns;
  parquet_file_parser::column_pos_t m_projections_columns;
  std::vector<parquet_file_parser::parquet_value_t> m_predicate_values;
  std::vector<parquet_file_parser::parquet_value_t> m_projections_values;

public:

  void result_values_to_string(multi_values& projections_resuls, std::string& result)
  {
    size_t i = 0;

    for(auto res : projections_resuls.values)
    {
      std::ostringstream quoted_result;
      //quoted_result << std::quoted(res->to_string(),'"','\\');
      quoted_result << res->to_string();
      if(++i < projections_resuls.values.size()) {
      quoted_result << ',';//TODO to use output serialization?
      }
      result.append(quoted_result.str());
    }
  }

  parquet_object(std::string parquet_file_name, s3select *s3_query,s3selectEngine::rgw_s3select_api* rgw) : base_s3object(s3_query->get_scratch_area()),m_error_count(0),object_reader(nullptr)
  {
    try{
    
      object_reader = new parquet_file_parser(parquet_file_name,rgw); //TODO uniq ptr
    } catch(std::exception &e)
    { 
      throw base_s3select_exception(std::string("failure while processing parquet meta-data ") + std::string(e.what()) ,base_s3select_exception::s3select_exp_en_t::FATAL);
    }

    set(s3_query);
    
    s3_query->get_scratch_area()->set_parquet_type();

    load_meta_data_into_scratch_area();

    for(auto x : m_s3_select->get_projections_list())
    {
        x->extract_columns(m_projections_columns,object_reader->get_num_of_columns());
    }

    if(m_s3_select->get_filter())
        m_s3_select->get_filter()->extract_columns(m_where_clause_columns,object_reader->get_num_of_columns());
  }

  parquet_object() : base_s3object(nullptr),m_s3_select(nullptr),object_reader(nullptr)
  {}

  ~parquet_object()
  {
    if(object_reader != nullptr)
    {
      delete object_reader;
    }

  }

  std::string get_error_description()
  {
    return m_error_description;
  }

  bool is_set()
  {
    return m_s3_select != nullptr; 
  }

  void set_parquet_object(std::string parquet_file_name, s3select *s3_query,s3selectEngine::rgw_s3select_api* rgw) //TODO duplicate code
  {
    try{
    
      object_reader = new parquet_file_parser(parquet_file_name,rgw); //TODO uniq ptr
    } catch(std::exception &e)
    { 
      throw base_s3select_exception(std::string("failure while processing parquet meta-data ") + std::string(e.what()) ,base_s3select_exception::s3select_exp_en_t::FATAL);
    }

    set(s3_query);

    m_sa = s3_query->get_scratch_area();
    
    s3_query->get_scratch_area()->set_parquet_type();

    load_meta_data_into_scratch_area();

    for(auto x : m_s3_select->get_projections_list())
    {
        x->extract_columns(m_projections_columns,object_reader->get_num_of_columns());
    }

    if(m_s3_select->get_filter())
        m_s3_select->get_filter()->extract_columns(m_where_clause_columns,object_reader->get_num_of_columns());
  }
  

  int run_s3select_on_object(std::string &result,
        std::function<int(std::string&)> fp_s3select_result_format,
        std::function<int(std::string&)> fp_s3select_header_format)
  {
    int status = 0;

    do
    {
      try
      {
        status = getMatchRow(result);
      }
      catch (base_s3select_exception &e)
      {
        m_error_description = e.what();
        m_error_count++;
        if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL || m_error_count > 100) //abort query execution
        {
          return -1;
        }
      }
      catch (std::exception &e)
      {
        m_error_description = e.what();
        m_error_count++;
        if (m_error_count > 100) //abort query execution
        {
          return -1;
        }
      }

#define S3SELECT_RESPONSE_SIZE_LIMIT (4 * 1024 * 1024)
      if (result.size() > S3SELECT_RESPONSE_SIZE_LIMIT)
      {//AWS-cli limits response size the following callbacks send response upon some threshold
        fp_s3select_result_format(result);

        if (!is_end_of_stream())
        {
          fp_s3select_header_format(result);
        }
      }
      else
      {
        if (is_end_of_stream())
        {
          fp_s3select_result_format(result);
        }
      }

      if (status < 0 || is_end_of_stream())
      {
        break;
      }

    } while (1);

    return status;
  }

  void load_meta_data_into_scratch_area()
  {
    int i=0;
    for(auto x : object_reader->get_schema())
    {
      m_s3_select->get_scratch_area()->set_column_pos(x.first.c_str(),i++); 
    }
  }

  void set(s3select* s3_query) //TODO reuse code on base
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

  bool is_end_of_stream()
  {
    return object_reader->end_of_stream();
  }

  int getMatchRow(std::string &result) //TODO virtual ? getResult
  {

    // get all column-references from where-clause
    // call parquet-reader(predicate-column-positions ,&row-values)
    // update scrach area with row-values
    // run where (if exist) in-case its true --> parquet-reader(projections-column-positions ,&row-values)

    bool next_rownum_status = true;
    multi_values projections_resuls;

    if (m_aggr_flow == true)
    {
      do
      {
        if (is_end_of_stream())
        {
          if (true) //(m_is_to_aggregate)
          {
            for (auto i : m_projections)
            {
              i->set_last_call();
              i->set_skip_non_aggregate(false);//projection column is set to be runnable
              projections_resuls.push_value( &(i->eval()) );
            }
	    result_values_to_string(projections_resuls,result);
          }

          return 0;
        }

        if ((*m_projections.begin())->is_set_last_call())
        {
          //should validate while query execution , no update upon nodes are marked with set_last_call
          throw base_s3select_exception("on aggregation query , can not stream row data post do-aggregate call", base_s3select_exception::s3select_exp_en_t::FATAL);
        }

        //TODO if (m_where_clause)
        object_reader->get_column_values_by_positions(m_where_clause_columns, m_predicate_values); //TODO status should indicate error/end-of-stream/success

        m_sa->update(m_predicate_values, m_where_clause_columns);

        for (auto a : *m_s3_select->get_aliases()->get())
        {
          a.second->invalidate_cache_result();
        }

        if (!m_where_clause || m_where_clause->eval().is_true())
        {
          object_reader->get_column_values_by_positions(m_projections_columns, m_projections_values);
          m_sa->update(m_projections_values, m_projections_columns);
          for (auto i : m_projections)
          {
            i->eval();
          }
        }

        object_reader->increase_rownum();

      } while (1);
    }
    else
    {
      if (m_where_clause)
      {
        do
        {

          for (auto a : *m_s3_select->get_aliases()->get())
          {
            a.second->invalidate_cache_result();
          }

          object_reader->get_column_values_by_positions(m_where_clause_columns, m_predicate_values); //TODO status should indicate error/end-of-stream/success

          m_sa->update(m_predicate_values, m_where_clause_columns);

          if (m_where_clause->eval().is_true())
            break;
          else
            next_rownum_status = object_reader->increase_rownum();

        } while (next_rownum_status);

        if (next_rownum_status == false)
          return 1;
      }
      else
      {
        for (auto a : *m_s3_select->get_aliases()->get())
        {
          a.second->invalidate_cache_result();
        }
      }

      object_reader->get_column_values_by_positions(m_projections_columns, m_projections_values);
      m_sa->update(m_projections_values, m_projections_columns);

      for (auto i : m_projections)
      {
	projections_resuls.push_value( &(i->eval()) );
      }
      result_values_to_string(projections_resuls,result);
      result.append("\n");//TODO not generic 

      object_reader->increase_rownum();

      if (is_end_of_stream())
      {
        return 0;
      }
    }

    return 1; //1>0
  }
};
#endif //_ARROW_EXIST

}; // namespace s3selectEngine

#endif
