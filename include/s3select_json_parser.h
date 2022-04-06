#ifndef S3SELECT_JSON_PARSER_H
#define S3SELECT_JSON_PARSER_H

#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/error/en.h"
#include <cassert>
#include <sstream>
#include <vector>
#include <iostream>
#include <functional>

class Valuesax {
//TODO replace with s3select::value
  public:

    enum Type {
      Decimal,
      Double,
      String,
      Bool,
      Null
    };

  private:

    Type _type;
    double _double;
    std::string _string;
    bool _bool;
    int64_t _num;;
    std::nullptr_t _null;

  public:

    Valuesax(): _type(Decimal), _double(0.0), _bool(false),_num(0) {}

    Valuesax& Parse(std::string const& s) {
      _type = Valuesax::String;
      _string = s;
      return *this;
    }

    Valuesax& Parse(const double& s) {
      _type = Valuesax::Double;
      _double = s;
      return *this;
    }

    Valuesax& Parse(bool& s) {
      _type = Valuesax::Bool;
      _bool = s;
      return *this;
    }

    Valuesax& Parse(const int& s) {
      _type = Valuesax::Decimal;
      _num = s;
      return *this;
    }

    Valuesax& Parse(const unsigned& s) {
      _type = Valuesax::Decimal;
      _num = s;
      return *this;
    }

    Valuesax& Parse(const int64_t& s) {
      _type = Valuesax::Decimal;
      _num = s;
      return *this;
    }

    Valuesax& Parse(const uint64_t& s) {
      _type = Valuesax::Decimal;
      _num = s;
      return *this;
    }

    Valuesax& Parse(const std::nullptr_t& s) {
      _type = Valuesax::Null;
      _null = s;
      return *this;
    }


    Type type() const { return _type; }

    int asInt() const {
      assert(_type == Decimal && "not an int");
      return _num;
    }

    double asDouble() const {
      assert(_type == Double && "not a double");
      return _double;
    }

    std::string const& asString() const {
      assert(_type == String && "not a string");
      return _string;
    }

    bool asBool() const {
      assert(_type == Bool && "not a bool");
      return _bool;
    }

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

    enum class row_state
    {
      NA,
      ARRAY,
      START_ROW,
      OBJECT_START_ROW,
      ARRAY_START_ROW,
      ARRAY_END_ROW,
      END_ROW
    };

    row_state state = row_state::NA;

    std::function <int(std::pair < std::string, Valuesax>)> fp;

    std::vector <std::vector<std::string>> query_matrix{};
    int row_count{};
    std::vector <std::string> from_clause{};
    bool prefix_match{};
    Valuesax value;
    ChunksStreamer stream_buffer;
    bool init_buffer_stream;
    rapidjson::Reader reader;
    std::vector<en_json_elm_state_t> json_element_state;
    std::string m_result;//debug purpose
    std::vector<std::string> key_path;

    JsonParserHandler() : init_buffer_stream(false)
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
      } 
      else if (prefix_match) {
          if (state == row_state::ARRAY || state == row_state::START_ROW) {
            state = row_state::START_ROW;
            ++row_count;
          }
      }
    }

    void push_new_key_value(Valuesax& v)
    {  
      if (prefix_match) {
        for (size_t i = 1; i < query_matrix.size(); i++) { // 0th index -> from-clause. Ignore that.
          if(std::equal(key_path.begin() + from_clause.size(), key_path.end(), query_matrix[i].begin())) {
            fp(make_pair(get_key_path(),v));
          }
        }
      }
      dec_key_path();
    }

    bool Null() {
      // at this point should verify against from-clause/where-clause/project. if match then push to scratch-area
      push_new_key_value(value.Parse(nullptr));
      return true; }

    bool Bool(bool b) {
      push_new_key_value(value.Parse(b));
      return true; }

    bool Int(int i) { 
      push_new_key_value(value.Parse(i));
      return true; }

    bool Uint(unsigned u) {
      push_new_key_value(value.Parse(u));
      return true; }

    bool Int64(int64_t i) { 
      push_new_key_value(value.Parse(i));
      return true; }

    bool Uint64(uint64_t u) { 
      push_new_key_value(value.Parse(u));
      return true; }

    bool Double(double d) { 
      push_new_key_value(value.Parse(d));
      return true; }

    bool String(const char* str, rapidjson::SizeType length, bool copy) {
      //TODO use copy
      push_new_key_value(value.Parse(str));
      return true;
    }

    bool Key(const char* str, rapidjson::SizeType length, bool copy) {
      key_path.push_back(std::string(str));
      
      if (key_path == from_clause) {
        prefix_match = true;
      }
      return true;
    }

    bool StartObject() {      
      if (key_path.size()) {
        if (prefix_match && (key_path[key_path.size() -1] == from_clause[from_clause.size() - 1])) {
          if (state != row_state::ARRAY && state != row_state::ARRAY_END_ROW) {
          state = row_state::OBJECT_START_ROW;
          ++row_count;
        } else {
          state = row_state::ARRAY_START_ROW;
          ++row_count;
          }
        }
      }

      json_element_state.push_back(OBJECT_STATE);
      return true; 
    }
  
    bool EndObject(rapidjson::SizeType memberCount) {
      json_element_state.pop_back();
      dec_key_path();
      if (state == row_state::OBJECT_START_ROW) {
        state = row_state::END_ROW;
      }
      if (state == row_state::ARRAY_START_ROW) {
        state = row_state::ARRAY_END_ROW;
      }
      return true; 
    }
 
    bool StartArray() {
      json_element_state.push_back(ARRAY_STATE);
      if (prefix_match && (key_path[key_path.size() - 1] == from_clause[from_clause.size() - 1])) {
          state = row_state::ARRAY;
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

    int process_json_buffer(char* json_buffer,size_t json_buffer_sz, std::function <int(std::pair < std::string, Valuesax>)>& f, bool end_of_stream=false)
    {//user keeps calling with buffers, the method is not aware of the object size.

      fp = f;

      try {

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

		      //TODO this condition could be replaced. it also define the amount of data that would be copy per each chunk
		      if (!end_of_stream && stream_buffer.next_src_==0 && stream_buffer.getBytesLeft() < 100)
		      {//the non processed bytes will be processed on next fetched chunk
		        //TODO save remaining-bytes to internal buffer (or caller will use 2 sets of buffer)
			      stream_buffer.saveRemainingBytes();
			      return 0;
		      }

		      // error message
		      if(reader.HasParseError())  {
			      rapidjson::ParseErrorCode c = reader.GetParseErrorCode();
			      size_t o = reader.GetErrorOffset();
			      std::cout << "PARSE ERROR " << c << " " << o << std::endl;
			      return -1;	  
		      }
	      }//while reader.IterativeParseComplete
      }
      catch (std::exception& e) {
        std::cerr << "exception caught: " << e.what() << '\n';
      }
	    return 0;
    }
};

#endif

