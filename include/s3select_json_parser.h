#ifndef S3SELECT_JSON_PARSER_H
#define S3SELECT_JSON_PARSER_H

//TODO add __FILE__ __LINE__ message
#define RAPIDJSON_ASSERT(x) s3select_json_parse_error(x)
bool s3select_json_parse_error(bool b);
bool s3select_json_parse_error(const char* error);

#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/error/en.h"
#include "rapidjson/document.h"
#include <cassert>
#include <sstream>
#include <vector>
#include <iostream>
#include <functional>
#include <boost/spirit/include/classic_core.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "s3select_oper.h"//class value
#include <boost/algorithm/string/predicate.hpp>

//TODO missing s3selectEngine namespace

bool s3select_json_parse_error(bool b)
{
  if(!b)
  {
    std::cout << "failure while processing " << std::endl;
  }
  return false;
}

bool s3select_json_parse_error(const char* error)
{
  if(!error)
  {
    std::cout << "failure while processing " << std::endl;
  }
  return false;
}

static auto iequal_predicate = [](std::string& it1, std::string& it2)
			  {
			    return boost::iequals(it1,it2);
			  };


class ChunksStreamer : public rapidjson::MemoryStream {

  //purpose: adding a method `resetBuffer` that enables to parse chunk after chunk
  //per each new chunk it reset internal data members
  public:

    std::string internal_buffer;
    const Ch* next_src_;
    size_t next_size_;

    ChunksStreamer():rapidjson::MemoryStream(0,0){next_src_=0;next_size_=0;}

    ChunksStreamer(const Ch *src, size_t size) : rapidjson::MemoryStream(src,size){next_src_=0;next_size_=0;}

    //override Peek methode
    Ch Peek() //const 
    {
      if(RAPIDJSON_UNLIKELY(src_ == end_))
      {
	if(next_src_)//next chunk exist
	{//upon reaching to end of current buffer, to switch with next one
	  src_ = next_src_;
	  begin_ = src_;
	  size_ =next_size_;
	  end_ = src_ + size_;

	  next_src_ = 0;
	  next_size_ = 0;
	  return *src_;
	}
	else return 0;
      }
      return *src_;
    }

    //override Take method
    Ch Take() 
    {
      if(RAPIDJSON_UNLIKELY(src_ == end_))
      {
	if(next_src_)//next chunk exist
	{//upon reaching to end of current buffer, to switch with next one
	  src_ = next_src_;
	  begin_ = src_;
	  size_ = next_size_;
	  end_ = src_ + size_;

	  next_src_ = 0;
	  next_size_ = 0;
	  return *src_;
	}
	else return 0;
      }
      return *src_++;
    }

    void resetBuffer(char* buff, size_t size)
    {
      if(!src_)
      {//first time calling
	begin_ = buff;
	src_ = buff;
	size_ = size;
	end_= src_ + size_;
	return;
      }

      if(!next_src_)
      {//save the next-chunk that will be used upon parser reaches end of current buffer
	next_src_ = buff;
	next_size_ = size;
      }
      else
      {// should not happen
	std::cout << "can not replace pointers!!!" << std::endl;//TODO exception
	return;
      }
    }

    void saveRemainingBytes()
    {//this routine called per each new chunk
      //save the remaining bytes, before its overriden by the next-chunk.
      size_t copy_left_sz = getBytesLeft(); //should be very small
      internal_buffer.assign(src_,copy_left_sz);
      
      src_ = internal_buffer.data();
      begin_ = src_;
      size_ = copy_left_sz;
      end_= src_ + copy_left_sz;
    }

    size_t getBytesLeft() { return end_ - src_; }

};

enum class row_state
{
  NA,
  OBJECT_START_ROW,
  ARRAY_START_ROW
};

class json_variable_access {
//purpose: a state-machine for json-variables. 
//upon the syntax-parser accepts a variable (projection / where-clause) it create this object.
//this object get events (key,start-array ... etc) as the JSON reader scans the input, 
//these events are advancing the states until it reaches to the last one, result with pushing value into scratch-area.

private:

// to set the following. 
std::vector<std::string>* from_clause;
std::vector<std::string>* key_path;
int* m_current_depth;
std::function <int(s3selectEngine::value&,int)>* m_exact_match_cb;
//  a state number : (_1).a.b.c[ 17 ].d.e (a.b)=1 (c[)=2  (17)=3 (.d.e)=4
int current_state;//contain the current state of the state machine for searching-expression (each JSON variable in SQL statement has a searching expression)
int nested_array_level;//in the case of array within array it contain the nesting level

struct variable_state_md {
    std::vector<std::string> required_path;//set by the syntax-parser. in the case of array its empty
    int required_array_entry_no;//set by the syntax-parser, in the case of object-key its -1.
    int actual_array_entry_no;//upon scanning the JSON input, this value increased by 1 each new element 
    int required_depth_size;// depth of state, is aggregated (include the previous). it's the summary of key-elements and array-operator's.
    int required_key_depth_size;// same as the above, not including the array-operators.
    int last_array_start;//it actually mark the nested-array-level (array within array)
};

std::vector<struct variable_state_md> variable_states;//vector is populated upon syntax phase.

public:

json_variable_access():from_clause(nullptr),key_path(nullptr),m_current_depth(nullptr),m_exact_match_cb(nullptr),current_state(-1),nested_array_level(0)
{}

void init(
	  std::vector<std::string>* reader_from_clause,
	  std::vector<std::string>* reader_key_path,
	  int* reader_current_depth,
	  std::function <int(s3selectEngine::value&,int)>* excat_match_cb)
{//this routine should be called before scanning the JSON input
  from_clause = reader_from_clause;
  key_path = reader_key_path;
  m_exact_match_cb = excat_match_cb;
  m_current_depth = reader_current_depth;
  current_state = 0;

  //loop on variable_states compute required_depth_size
}

void clear()
{
	variable_states.clear();
}

void debug_info()
{
  auto f = [](std::vector<std::string> x){std::string res;for(auto i : x){res.append(i);res.append(".");};return res;};

  std::cout << "m_current_depth=" << *m_current_depth << " required_depth_size= " << reader_position_state().required_depth_size << " ";
  std::cout << "variable_states[ current_state ].last_array_start=" << reader_position_state().last_array_start;
  std::cout << " current_state=" << current_state << " key_path=" << f(*key_path) << std::endl; 
}
#define DBG {std::cout << "event=" << __FUNCTION__ << std::endl; debug_info();}
#undef DBG
#define DBG

void compile_state_machine()
{
  size_t aggregated_required_depth_size = 0;
  size_t aggregated_required_key_depth_size = 0;
  for(auto& v : variable_states)
  {
    if(v.required_path.size())
    {
      v.required_depth_size = aggregated_required_depth_size + v.required_path.size();//depth size in general, including array 
      v.required_key_depth_size = aggregated_required_key_depth_size;//depth include ONLY key parts 
      aggregated_required_key_depth_size += v.required_path.size();
    }
    else
    {
      v.required_depth_size = aggregated_required_depth_size + 1;
    }
    aggregated_required_depth_size = v.required_depth_size;
  }
}

void push_variable_state(std::vector<std::string>& required_path,int required_array_entry_no)
{
  struct variable_state_md new_state={required_path,required_array_entry_no,-1,0,0,-1};
  variable_states.push_back(new_state);
  //TODO required_path.size() > 0 or required_path,required_array_entry_no>=0 : not both
  compile_state_machine();
}

struct variable_state_md& reader_position_state()
{
  return variable_states[ current_state ];
}

bool is_array_state()
{
  return (reader_position_state().required_array_entry_no>=0);
}

bool is_reader_located_on_required_depth()
{
  return (*m_current_depth == reader_position_state().required_depth_size);
}

bool is_on_final_state()
{
  return ((size_t)current_state == (variable_states.size()));  
	  //&& *m_current_depth == variable_states[ current_state -1 ].required_depth_size); 
	  
	  // NOTE: by ignoring the current-depth, the matcher gives precedence to key-path match, while not ignoring accessing using array
	  // meaning, upon requeting a.b[12] , the [12] is not ignored, the a<-->b distance should be calculated as key distance, i.e. not counting array/object with *no keys*.
	  // user may request 'select _1.phonearray.num'; the reader will traverse `num` exist in `phonearray`
}

bool is_reader_reached_required_array_entry()
{
 return (reader_position_state().actual_array_entry_no == reader_position_state().required_array_entry_no); 
}

bool is_reader_passed_required_array_entry()
{
  return (reader_position_state().actual_array_entry_no > reader_position_state().required_array_entry_no);
}

bool is_reader_located_on_array_according_to_current_state()
{
  return (nested_array_level == reader_position_state().last_array_start);
}

bool is_reader_position_depth_lower_than_required()
{
  return (*m_current_depth < reader_position_state().required_depth_size);
}

bool is_reader_located_on_array_entry_according_to_current_state()
{
  return (reader_position_state().actual_array_entry_no == reader_position_state().required_array_entry_no);
}

void increase_current_state()
{
  DBG

  if((size_t)current_state >= (variable_states.size())) return;
  current_state ++;
}

void decrease_current_state()
{ 
  DBG

  if(current_state == 0) return;
  current_state --;
}

void key()
{
  DBG

  if(reader_position_state().required_path.size())//state has a key
  {// key should match
    std::vector<std::string>* filter = &reader_position_state().required_path;
    auto required_key_depth_size = reader_position_state().required_key_depth_size;
    if(std::equal((*key_path).begin()+(*from_clause).size() + required_key_depth_size, //key-path-start-point + from-clause-depth-size + key-depth
		  (*key_path).end(), 
		  (*filter).begin(),
		  (*filter).end(), iequal_predicate))
    {
      increase_current_state();//key match, advancing to next
    }
  }
}

void increase_array_index()
{
  if(is_reader_located_on_required_depth() && is_array_state())//TODO && is_array_state().  is it necessary? what about nesting level
  {
      DBG
      reader_position_state().actual_array_entry_no++;
  }
}

void dec_key()
{
  DBG

  if(is_reader_position_depth_lower_than_required())
  {//actual key-path is shorter than required
    decrease_current_state();
    return;
  }

  if(is_reader_located_on_required_depth() && is_array_state())//TODO && is_array_state().  is it necessary?; json_element_state.back() != ARRAY_STATE)
  {//key-path-depth matches, and it an array
    if(is_reader_reached_required_array_entry())
    {//we reached the required array entry
      increase_current_state();
    } 
    else if(is_reader_passed_required_array_entry())
    {//had passed the array entry
      decrease_current_state();
    }
  }
}

void new_value(s3selectEngine::value& v,size_t json_index)
{
  DBG

  if(is_on_final_state())
  {
    (*m_exact_match_cb)(v, json_index);
    increase_array_index();
    decrease_current_state();//TODO why decrease? the state-machine reached its final destination, and it should be only one result
  } 
  increase_array_index();//next-value in array
}

void end_object()
{
  increase_array_index();
}

void end_array()
{
  //init the correct array index
  DBG

  if(is_reader_located_on_array_according_to_current_state())
  {//it reached end of required array
    reader_position_state().actual_array_entry_no = 0;
    decrease_current_state();
  }
  nested_array_level --;

  // option 1. move out of one array, and enter a new one; option-2. enter an object
  increase_array_index();//increase only upon correct array //TODO move it into dec_key()?
  dec_key();
}

void start_array()
{
  DBG

  nested_array_level++;
  if(is_reader_located_on_required_depth())
  {//reader entered an array required by JSON variable 
    reader_position_state().actual_array_entry_no = 0;
    reader_position_state().last_array_start  = nested_array_level;
    
    if(is_reader_located_on_array_entry_according_to_current_state())
    {//we reached the required array entry -> next state
      increase_current_state();
    }
  }
}

}; //json_variable_access

class json_variables_operations {

	public:
    
	std::vector<std::pair<json_variable_access*,size_t>> json_statement_variables{};

	void init(std::vector<std::pair<json_variable_access*,size_t>>& jsv, //TODO init upon construction?
		  std::vector <std::string>* from_clause,
		  std::vector<std::string>* key_path,
		  int* current_depth,
		  std::function <int(s3selectEngine::value&,int)>* exact_match_cb)
	{
	  json_statement_variables = jsv;

	  for(auto& var : json_statement_variables)
	  {
	    var.first->init(from_clause,
		      key_path,
		      current_depth,
		      exact_match_cb);
	  }
	}

	void start_array()
	{
	    for(auto& j : json_statement_variables)
	    {
	      j.first->start_array();
	    }
	}
	void end_array()
	{
	    for(auto& j : json_statement_variables)
	    {
	      j.first->end_array();
	    }
	}
	void dec_key()
	{
	    for(auto& j : json_statement_variables)
	    {
	      j.first->dec_key();
	    }
	}
	void end_object()
	{
	    for(auto& j : json_statement_variables)
	    {
	      j.first->end_object();
	    }
	}
	void key()
	{
	    for(auto& j : json_statement_variables)
	    {
	      j.first->key();
	    }
	}
	void new_value(s3selectEngine::value& v)
	{
	    for(auto& j : json_statement_variables)
	    {
	      j.first->new_value(v,j.second);
	    }
	}
};//json_variables_operations

class JsonParserHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JsonParserHandler> {

  public:

    typedef enum {OBJECT_STATE,ARRAY_STATE} en_json_elm_state_t;
    typedef std::pair<std::vector<std::string>, s3selectEngine::value> json_key_value_t;

    row_state state = row_state::NA;
    std::function <int(s3selectEngine::value&,int)> m_exact_match_cb;
    std::function <int(s3selectEngine::scratch_area::json_key_value_t&)> m_star_operation_cb;

    json_variables_operations variable_match_operations;
    int row_count{};
    std::vector <std::string> from_clause{};
    bool prefix_match{};
    s3selectEngine::value var_value;
    ChunksStreamer stream_buffer;
    bool init_buffer_stream;
    rapidjson::Reader reader;
    std::vector<en_json_elm_state_t> json_element_state;
    std::vector<std::string> key_path;
    std::function<int(void)> m_s3select_processing;
    int m_start_row_depth;   
    int m_current_depth;
    bool m_star_operation;

    JsonParserHandler() : prefix_match(false),init_buffer_stream(false),m_start_row_depth(-1),m_current_depth(0),m_star_operation(false)
    {
    } 

    std::string get_key_path()
    {//for debug
	  std::string res;
	  for(const auto & i: key_path)
	  {
	    res.append(i);
	    res.append(std::string("/"));
	  }
	  return res;
    }

    void dec_key_path()
    {
      if (json_element_state.size())  {
        if(json_element_state.back() != ARRAY_STATE)  {
	        if(key_path.size() != 0) {
	          key_path.pop_back();
          }
        }
      }
     
      variable_match_operations.dec_key();

      //TODO m_current_depth-- should done here 
      if(m_start_row_depth > m_current_depth)
      {
	  prefix_match = false;
      } else
      if (prefix_match) {
          if (state == row_state::ARRAY_START_ROW && m_start_row_depth == m_current_depth) {
	    m_s3select_processing(); //per each element in array
            ++row_count;
          }
      }
    }

    void push_new_key_value(s3selectEngine::value& v)
    { 
      if (m_star_operation && prefix_match)
      {
	json_key_value_t key_value(key_path,v);
	m_star_operation_cb(key_value);
      }
	if (prefix_match)
      		variable_match_operations.new_value(v);

      dec_key_path();
    }

    bool Null() {
      var_value.setnull();
      push_new_key_value(var_value);
      return true; }

    bool Bool(bool b) {
      var_value = b;
      push_new_key_value(var_value);
      return true; }

    bool Int(int i) { 
      var_value = i;
      push_new_key_value(var_value);
      return true; }

    bool Uint(unsigned u) {
      var_value = u;
      push_new_key_value(var_value);
      return true; }

    bool Int64(int64_t i) { 
      var_value = i;
      push_new_key_value(var_value);
      return true; }

    bool Uint64(uint64_t u) { 
      var_value = u;
      push_new_key_value(var_value);
      return true; }

    bool Double(double d) { 
      var_value = d;
      push_new_key_value(var_value);
      return true; }

    bool String(const char* str, rapidjson::SizeType length, bool copy) {
      //TODO use copy
      var_value = str;
      push_new_key_value(var_value);
      return true;
    }

    bool Key(const char* str, rapidjson::SizeType length, bool copy) {
      key_path.push_back(std::string(str));
      
      if(from_clause.size() == 0 || std::equal(key_path.begin(), key_path.end(), from_clause.begin(), from_clause.end(), iequal_predicate)) {
        prefix_match = true;
      }

      variable_match_operations.key();

      return true;
    }

    bool is_already_row_started()
    {
      if(state == row_state::OBJECT_START_ROW || state == row_state::ARRAY_START_ROW)
	return true;
      else
	return false;
    }

    bool StartObject() {      
	json_element_state.push_back(OBJECT_STATE);
	m_current_depth++;
        if (prefix_match && !is_already_row_started()) {
          state = row_state::OBJECT_START_ROW;
	  m_start_row_depth = m_current_depth;
          ++row_count;
        }

      return true; 
    }
  
    bool EndObject(rapidjson::SizeType memberCount) {
      json_element_state.pop_back();
      m_current_depth --;

      variable_match_operations.end_object();
      
      dec_key_path();
      if (state == row_state::OBJECT_START_ROW && (m_start_row_depth > m_current_depth)) {
	m_s3select_processing();
	state = row_state::NA;
      }
      return true; 
    }
 
    bool StartArray() {
      json_element_state.push_back(ARRAY_STATE);
      m_current_depth++;
      if (prefix_match && !is_already_row_started()) {
          state = row_state::ARRAY_START_ROW;
	  m_start_row_depth = m_current_depth;
        }
 
      variable_match_operations.start_array();

      return true;
    }

    bool EndArray(rapidjson::SizeType elementCount) { 
      json_element_state.pop_back();
      m_current_depth--;
      dec_key_path();

      if (state == row_state::ARRAY_START_ROW && (m_start_row_depth > m_current_depth)) {
	state = row_state::NA;
      }
     
      variable_match_operations.end_array();

      return true;
    }

    void set_prefix_match(std::vector<std::string>& requested_prefix_match)
    {//purpose: set the filter according to SQL statement(from clause)
      from_clause = requested_prefix_match;
      if(from_clause.size() ==0)
      {
	prefix_match = true;
	m_start_row_depth = m_current_depth;
      }
    }

    void set_statement_json_variables(std::vector<std::pair<json_variable_access*,size_t>>& statement_variables)
    {//purpose: set the json variables extracted from the SQL statement(projection columns, predicates columns)
		    variable_match_operations.init(
			  statement_variables,
			  &from_clause,
			  &key_path,
			  &m_current_depth,
			  &m_exact_match_cb);
    }

    void set_exact_match_callback(std::function<int(s3selectEngine::value&, int)> f)
    {//purpose: upon key is matching one of the exact filters, the callback is called.
      m_exact_match_cb = f;
    }

    void set_s3select_processing_callback(std::function<int(void)>& f)
    {//purpose: execute s3select statement on matching row (according to filters)
      m_s3select_processing = f;
    }

    void set_push_per_star_operation_callback( std::function <int(s3selectEngine::scratch_area::json_key_value_t&)> cb)
    {
      m_star_operation_cb = cb;
    }

    void set_star_operation()
    {
      m_star_operation = true;
    }

    int process_json_buffer(char* json_buffer,size_t json_buffer_sz, bool end_of_stream=false)
    {//user keeps calling with buffers, the method is not aware of the object size.


      try{
	    if(!init_buffer_stream)
	    {
		    //set the memoryStreamer
		    reader.IterativeParseInit();
		    init_buffer_stream = true;

	    }

	    //the non-processed bytes plus the next chunk are copy into main processing buffer 
	    if(!end_of_stream)
		    stream_buffer.resetBuffer(json_buffer, json_buffer_sz);

	    while (!reader.IterativeParseComplete()) {
		    reader.IterativeParseNext<rapidjson::kParseDefaultFlags>(stream_buffer, *this);

		    //once all key-values move into s3select(for further filtering and processing), it should be cleared

		    //TODO in the case the chunk is too small or some value in input is too big, the parsing will fail.
		    if (!end_of_stream && stream_buffer.next_src_==0 && stream_buffer.getBytesLeft() < 2048)
		    {//the non processed bytes will be processed on next fetched chunk
		     //TODO save remaining-bytes to internal buffer (or caller will use 2 sets of buffer)
			    stream_buffer.saveRemainingBytes();
			    return 0;
		    }

		    // error message
		    if(reader.HasParseError())  {
			    rapidjson::ParseErrorCode c = reader.GetParseErrorCode();
			    size_t ofs = reader.GetErrorOffset();
			    std::stringstream error_str;
			    error_str << "parsing error. code:" << c << " position: " << ofs << std::endl;
			    std::cout << error_str.str();
			    return -1;	  
		    }
	    }//while reader.IterativeParseComplete
	}
        catch(std::exception &e){//TODO specific exception
                std::cout << "failed to process JSON" << e.what() << std::endl;
                return -1;
        }
	return 0;
    }
};


#endif

