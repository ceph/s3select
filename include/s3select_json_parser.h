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
//#include "s3select.h"
#include <boost/spirit/include/classic_core.hpp>
#include "s3select_oper.h"//class value
#include <boost/algorithm/string/predicate.hpp>

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
      //savine the remaining bytes, before its overriden by the next-chunk.
      size_t copy_left_sz = getBytesLeft(); //should be very small
      internal_buffer.assign(src_,copy_left_sz);
      
      src_ = internal_buffer.data();
      begin_ = src_;
      size_ = copy_left_sz;
      end_= src_ + copy_left_sz;
    }

    size_t getBytesLeft() { return end_ - src_; }

};

class JsonParserHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JsonParserHandler> {

  public:

    typedef enum {OBJECT_STATE,ARRAY_STATE} en_json_elm_state_t;

    typedef std::pair<std::vector<std::string>, s3selectEngine::value> json_key_value_t;

    enum class row_state
    {
      NA,
      OBJECT_START_ROW,
      ARRAY_START_ROW,
      END_ROW
    };

    row_state state = row_state::NA;
    std::function <int(s3selectEngine::value&,int)> m_exact_match_cb;

    std::vector <std::vector<std::string>> query_matrix{};
    int row_count{};
    std::vector <std::string> from_clause{};
    bool prefix_match{};
    s3selectEngine::value var_value;
    ChunksStreamer stream_buffer;
    bool init_buffer_stream;
    rapidjson::Reader reader;
    std::vector<en_json_elm_state_t> json_element_state;
    std::string m_result;//debug purpose
    std::vector<std::string> key_path;
    std::function<int(void)> m_s3select_processing;


    JsonParserHandler() : prefix_match(false),init_buffer_stream(false)
    {} 

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
      
      if (key_path.size() < from_clause.size()) {
        prefix_match = false;
        state = row_state::END_ROW;
      } 
      else if (prefix_match) {
          if (state == row_state::ARRAY_START_ROW) {
	    m_s3select_processing();
            ++row_count;
          }
      }
    }

    void push_new_key_value(s3selectEngine::value& v)
    { int json_idx =0; 

      //std::cout << get_key_path() << std::endl;

      auto predicate = [](std::string& it1, std::string& it2)
			  {
			    boost::to_upper(it1);
			    boost::to_upper(it2);
			    return it1 == it2;
			  };

      if (prefix_match) {
        for (auto filter : query_matrix) {

          if(std::equal(key_path.begin() + from_clause.size(), key_path.end(), filter.begin(),iequal_predicate)) {
            m_exact_match_cb(v, json_idx);
          }
	  json_idx ++;//TODO can use filter - begin()
        }
      }
      dec_key_path();
    }

    bool Null() {
      // at this point should verify against from-clause/where-clause/project. if match then push to scratch-area
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
      
      if (key_path == from_clause || from_clause.size()==0) {
        prefix_match = true;
      }
      return true;
    }

    bool StartObject() {      
      if (key_path.size()) {
        if (prefix_match && from_clause.size() && (key_path[key_path.size() -1] == from_clause[from_clause.size() - 1])) {
          state = row_state::OBJECT_START_ROW;
          ++row_count;
        }
      }
      if (!key_path.size()) {
          state = row_state::END_ROW;
        }

      json_element_state.push_back(OBJECT_STATE);
      return true; 
    }
  
    bool EndObject(rapidjson::SizeType memberCount) {
      json_element_state.pop_back();
      dec_key_path();
      if (state == row_state::OBJECT_START_ROW) {
	m_s3select_processing();
      }
      return true; 
    }
 
    bool StartArray() {
      json_element_state.push_back(ARRAY_STATE);
      if (prefix_match && from_clause.size() && (key_path[key_path.size() - 1] == from_clause[from_clause.size() - 1])) {
          state = row_state::ARRAY_START_ROW;
        }
      return true;
    }

    bool EndArray(rapidjson::SizeType elementCount) { 
      json_element_state.pop_back();
      dec_key_path();
        if (!key_path.size()) {
          state = row_state::END_ROW;
        }
      return true;
    }

    void set_prefix_match(std::vector<std::string>& prefix_match)
    {//purpose: set the filter according to SQL statement(from clause)
      from_clause = prefix_match;
    }

    void set_exact_match_filters(std::vector<std::vector<std::string>>& exact_match_filters)
    {//purpose: set the filters according to SQL statement(projection columns, predicates columns)
      query_matrix = exact_match_filters;
    }

    void set_exact_match_callback(std::function<int(s3selectEngine::value&, int)> f)
    {//purpose: upon key is matching one of the exact filters, the callback is called.
      m_exact_match_cb = f;
    }

    void set_s3select_processing_callback(std::function<int(void)>& f)
    {//purpose: execute s3select statement on matching row (according to filters)
      m_s3select_processing = f;
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

