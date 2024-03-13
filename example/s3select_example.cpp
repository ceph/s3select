#include "s3select.h"
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <boost/crc.hpp>
#include <arpa/inet.h>
#include <boost/filesystem.hpp>
#include <boost/tokenizer.hpp>

using namespace s3selectEngine;
using namespace BOOST_SPIRIT_CLASSIC_NS;

std::string output_format{};
std::string header_info{};

class awsCli_handler {


//TODO get parameter 
private:
  std::unique_ptr<s3selectEngine::s3select> s3select_syntax;
  std::string m_s3select_query;
  std::string m_result;
  std::unique_ptr<s3selectEngine::csv_object> m_s3_csv_object;
  std::string m_column_delimiter;//TODO remove
  std::string m_quot;//TODO remove
  std::string m_row_delimiter;//TODO remove
  std::string m_compression_type;//TODO remove
  std::string m_escape_char;//TODO remove
  std::unique_ptr<char[]>  m_buff_header;
  std::string m_header_info;
  std::string m_sql_query;
  uint64_t m_total_object_processing_size;

public:

  awsCli_handler():
      s3select_syntax(std::make_unique<s3selectEngine::s3select>()),
      m_s3_csv_object(std::unique_ptr<s3selectEngine::csv_object>()),
      m_buff_header(std::make_unique<char[]>(1000)),
      m_total_object_processing_size(0),
      crc32(std::unique_ptr<boost::crc_32_type>())
  {
  }

  enum header_name_En
  {
    EVENT_TYPE,
    CONTENT_TYPE,
    MESSAGE_TYPE
  };
  static const char* header_name_str[3];

  enum header_value_En
  {
    RECORDS,
    OCTET_STREAM,
    EVENT,
    CONT
  };
  static const char* header_value_str[4];

private:

    void encode_short(char *buff, uint16_t s, int &i)
    {
      short x = htons(s);
      memcpy(buff, &x, sizeof(s));
      i += sizeof(s);
    }

    void encode_int(char *buff, u_int32_t s, int &i)
    {
      u_int32_t x = htonl(s);
      memcpy(buff, &x, sizeof(s));
      i += sizeof(s);
    }

  int create_header_records(char* buff)
  {
  int i = 0;

  //1
  buff[i++] = char(strlen(header_name_str[EVENT_TYPE]));
  memcpy(&buff[i], header_name_str[EVENT_TYPE], strlen(header_name_str[EVENT_TYPE]));
  i += strlen(header_name_str[EVENT_TYPE]);
  buff[i++] = char(7);
  encode_short(&buff[i], uint16_t(strlen(header_value_str[RECORDS])), i);
  memcpy(&buff[i], header_value_str[RECORDS], strlen(header_value_str[RECORDS]));
  i += strlen(header_value_str[RECORDS]);

  //2
  buff[i++] = char(strlen(header_name_str[CONTENT_TYPE]));
  memcpy(&buff[i], header_name_str[CONTENT_TYPE], strlen(header_name_str[CONTENT_TYPE]));
  i += strlen(header_name_str[CONTENT_TYPE]);
  buff[i++] = char(7);
  encode_short(&buff[i], uint16_t(strlen(header_value_str[OCTET_STREAM])), i);
  memcpy(&buff[i], header_value_str[OCTET_STREAM], strlen(header_value_str[OCTET_STREAM]));
  i += strlen(header_value_str[OCTET_STREAM]);

  //3
  buff[i++] = char(strlen(header_name_str[MESSAGE_TYPE]));
  memcpy(&buff[i], header_name_str[MESSAGE_TYPE], strlen(header_name_str[MESSAGE_TYPE]));
  i += strlen(header_name_str[MESSAGE_TYPE]);
  buff[i++] = char(7);
  encode_short(&buff[i], uint16_t(strlen(header_value_str[EVENT])), i);
  memcpy(&buff[i], header_value_str[EVENT], strlen(header_value_str[EVENT]));
  i += strlen(header_value_str[EVENT]);

  return i;
}

  std::unique_ptr<boost::crc_32_type> crc32;

  int create_message(std::string &out_string, u_int32_t result_len, u_int32_t header_len)
  {
    u_int32_t total_byte_len = 0;
    u_int32_t preload_crc = 0;
    u_int32_t message_crc = 0;
    int i = 0;
    char *buff = out_string.data();

    if (crc32 == 0)
    {
      // the parameters are according to CRC-32 algorithm and its aligned with AWS-cli checksum
      crc32 = std::unique_ptr<boost::crc_32_type>(new boost::crc_optimal<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, true>);
    }

    total_byte_len = result_len + 16;

    encode_int(&buff[i], total_byte_len, i);
    encode_int(&buff[i], header_len, i);

    crc32->reset();
    *crc32 = std::for_each(buff, buff + 8, *crc32);
    preload_crc = (*crc32)();
    encode_int(&buff[i], preload_crc, i);

    i += result_len;

    crc32->reset();
    *crc32 = std::for_each(buff, buff + i, *crc32);
    message_crc = (*crc32)();

    int out_encode;
    encode_int(reinterpret_cast<char*>(&out_encode), message_crc, i);
    out_string.append(reinterpret_cast<char*>(&out_encode),sizeof(out_encode));

    return i;
  }

#define PAYLOAD_LINE "\n<Payload>\n<Records>\n<Payload>\n"
#define END_PAYLOAD_LINE "\n</Payload></Records></Payload>"

public:

  //std::string get_error_description(){}

  std::string& get_result()
  {
    return m_result;
  }

  int run_s3select(const char *query, const char *input, size_t input_length, size_t object_size)
  {
    int status = 0;
    csv_object::csv_defintions csv;

    m_result = "012345678901"; //12 positions for header-crc

    int header_size = 0;

    if (m_s3_csv_object == 0)
    {
      s3select_syntax->parse_query(query);

      if (m_row_delimiter.size())
      {
        csv.row_delimiter = *m_row_delimiter.c_str();
      }

      if (m_column_delimiter.size())
      {
        csv.column_delimiter = *m_column_delimiter.c_str();
      }

      if (m_quot.size())
      {
        csv.quot_char = *m_quot.c_str();
      }

      if (m_escape_char.size())
      {
        csv.escape_char = *m_escape_char.c_str();
      }

      if (m_header_info.compare("IGNORE") == 0)
      {
        csv.ignore_header_info = true;
      }
      else if (header_info.compare("USE") == 0)
      {
        csv.use_header_info = true;
      }

      if(output_format.compare("JSON") == 0)  {
        csv.output_json_format = true;
      }

      m_s3_csv_object = std::unique_ptr<s3selectEngine::csv_object>(new s3selectEngine::csv_object());
      m_s3_csv_object->set_csv_query(s3select_syntax.get(), csv);
    }


    if (s3select_syntax->get_error_description().empty() == false)
    {
      header_size = create_header_records(m_buff_header.get());
      m_result.append(m_buff_header.get(), header_size);
      m_result.append(PAYLOAD_LINE);
      m_result.append(s3select_syntax->get_error_description());
      //ldout(s->cct, 10) << "s3-select query: failed to prase query; {" << s3select_syntax->get_error_description() << "}" << dendl;
      status = -1;
    }
    else
    {
      header_size = create_header_records(m_buff_header.get());
      m_result.append(m_buff_header.get(), header_size);
      m_result.append(PAYLOAD_LINE);
      //status = m_s3_csv_object->run_s3select_on_stream(m_result, input, input_length, s->obj_size);
      status = m_s3_csv_object->run_s3select_on_stream(m_result, input, input_length, object_size);
      if (status < 0)
      {
        m_result.append(m_s3_csv_object->get_error_description());
      }
    }

    if (m_result.size() > strlen(PAYLOAD_LINE))
    {
      m_result.append(END_PAYLOAD_LINE);
      create_message(m_result, m_result.size() - 12, header_size);
      //s->formatter->write_bin_data(m_result.data(), buff_len);
      //if (op_ret < 0)
      //{
      //  return op_ret;
      //}
    }
    //rgw_flush_formatter_and_reset(s, s->formatter);

    return status;
  }
  //int extract_by_tag(std::string tag_name, std::string& result);

  //void convert_escape_seq(std::string& esc);

  //int handle_aws_cli_parameters(std::string& sql_query);

};

const char* awsCli_handler::header_name_str[3] = {":event-type", ":content-type", ":message-type"};
const char* awsCli_handler::header_value_str[4] = {"Records", "application/octet-stream", "event","cont"};
int run_on_localFile(char*  input_query);

bool is_parquet_file(const char * fn)
{//diffrentiate between csv and parquet
   const char * ext = "parquet";

   if(strstr(fn+strlen(fn)-strlen(ext), ext ))
   {
    return true;
   }

    return false;
}

#ifdef _ARROW_EXIST
int run_query_on_parquet_file(const char* input_query, const char* input_file)
{
  int status;
  s3select s3select_syntax;

  status = s3select_syntax.parse_query(input_query);
  if (status != 0)
  {
    std::cout << "failed to parse query " << s3select_syntax.get_error_description() << std::endl;
    return -1;
  }

  FILE *fp;

  fp=fopen(input_file,"r");

  if(!fp){
    std::cout << "can not open " << input_file << std::endl;
    return -1;
  }

  std::function<int(void)> fp_get_size=[&]()
  {
    struct stat l_buf;
    lstat(input_file,&l_buf);
    return l_buf.st_size;
  };

  std::function<size_t(int64_t,int64_t,void*,optional_yield*)> fp_range_req=[&](int64_t start,int64_t length,void *buff,optional_yield*y)
  {
    fseek(fp,start,SEEK_SET);
    size_t read_sz = fread(buff, 1, length, fp);
    return read_sz;
  };

  rgw_s3select_api rgw;
  rgw.set_get_size_api(fp_get_size);
  rgw.set_range_req_api(fp_range_req);
  
  std::function<int(std::string&)> fp_s3select_result_format = [](std::string& result){std::cout << result;result.clear();return 0;};
  std::function<int(std::string&)> fp_s3select_header_format = [](std::string& result){result="";return 0;};
  std::function<void(const char*)> fp_debug = [](const char* msg)
  {
	  std::cout << "DEBUG: {" <<  msg << "}" << std::endl;
  };

  parquet_object parquet_processor(input_file,&s3select_syntax,&rgw);
  //parquet_processor.set_external_debug_system(fp_debug);

  std::string result;

  do
  {
    try
    {
      status = parquet_processor.run_s3select_on_object(result);
    }
    catch (base_s3select_exception &e)
    {
      std::cout << e.what() << std::endl;
      //m_error_description = e.what();
      //m_error_count++;
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL) //abort query execution
      {
        return -1;
      }
    }

    if(status<0)
    {
      std::cout << parquet_processor.get_error_description() << std::endl;
      break;
    }

    std::cout << result << std::endl;

    if(status == 2) // limit reached
    {
      break;
    }

  } while (0);

  return 0;
}
#else
int run_query_on_parquet_file(const char* input_query, const char* input_file)
{
  std::cout << "arrow is not installed" << std::endl;
  return 0;
}
#endif //_ARROW_EXIST

#define BUFFER_SIZE (4*1024*1024)
int process_json_query(const char* input_query,const char* fname)
{//purpose: process json query 

  s3select s3select_syntax;
  s3selectEngine::json_object m_s3_json_object;
  json_object::csv_definitions json;
  int status = s3select_syntax.parse_query(input_query);
  if (status != 0)
  {
    std::cout << "failed to parse query " << s3select_syntax.get_error_description() << std::endl;
    return -1;
  }

  if(output_format.compare("JSON") == 0)  {
    json.output_json_format = true;
  }

  std::ifstream input_file_stream;
  try {
  	input_file_stream = std::ifstream(fname, std::ios::in | std::ios::binary);
  }
  catch( ... )
  {
	std::cout << "failed to open file " << fname << std::endl;	
	exit(-1);
  }

  auto object_sz = boost::filesystem::file_size(fname);
  m_s3_json_object.set_json_query(&s3select_syntax, json);
  std::string buff(BUFFER_SIZE,0);
  std::string result;


  size_t read_sz = input_file_stream.read(buff.data(),BUFFER_SIZE).gcount();
  int chunk_count=0;
  size_t bytes_read=0;
  while(read_sz)
  {
    bytes_read += read_sz;
    std::cout << "read next chunk " << chunk_count++ << ":" << read_sz << ":" << bytes_read << "\r";

    result.clear();

    try{
    	status = m_s3_json_object.run_s3select_on_stream(result, buff.data(), read_sz, object_sz, json.output_json_format);
  } catch (base_s3select_exception &e)
  {
      std::cout << e.what() << std::endl;
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL) //abort query execution
      {
        return -1;
      }
  }

    if(result.size())
    {
	std::cout << result << std::endl;
    }
 
    if(status<0)
    {
      std::cout << "failure upon processing " << std::endl;
      return -1;
    } 
    if(m_s3_json_object.is_sql_limit_reached())
    {
      std::cout << "json processing reached limit " << std::endl;
      break;
    }
    read_sz = input_file_stream.read(buff.data(),BUFFER_SIZE).gcount();  
  }
  try{
    	result.clear();
  	m_s3_json_object.run_s3select_on_stream(result, 0, 0, object_sz, json.output_json_format);
  } catch (base_s3select_exception &e)
  {
      std::cout << e.what() << std::endl;
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL) //abort query execution
      {
        return -1;
      }
  }

  std::cout << result << std::endl;
  return 0;
}

int run_on_localFile(char* input_query)
{
  //purpose: demostrate the s3select functionalities
  s3select s3select_syntax;

  if (!input_query)
  {
    std::cout << "type -q 'select ... from ...  '" << std::endl;
    return -1;
  }

  int status = s3select_syntax.parse_query(input_query);
  if (status != 0)
  {
    std::cout << "failed to parse query " << s3select_syntax.get_error_description() << std::endl;
    return -1;
  }

  std::string object_name = s3select_syntax.get_from_clause(); 

  if (is_parquet_file(object_name.c_str()))
  {
    try {
      return run_query_on_parquet_file(input_query, object_name.c_str());
    }
    catch (base_s3select_exception &e)
    {
      std::cout << e.what() << std::endl;
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL) //abort query execution
      {
        return -1;
      }
    }
  }

  FILE* fp = nullptr;

  if (object_name.compare("stdin")==0)
  {
    fp = stdin;
  }
  else
  {
    fp  = fopen(object_name.c_str(), "r");
  }

  if(!fp)
  {
    std::cout << " input stream is not valid, abort;" << std::endl;
    return -1;
  }

  struct stat statbuf;
  lstat(object_name.c_str(), &statbuf);

  std::string s3select_result;
  s3selectEngine::csv_object::csv_defintions csv;
  csv.use_header_info = false;
  csv.quote_fields_always=false;

  if(output_format.compare("JSON") == 0)  {
    csv.output_json_format = true;
  }

  if(header_info.compare("USE") == 0)  {
    csv.use_header_info = true;
  }

#define CSV_QUOT "CSV_ALWAYS_QUOT"
#define CSV_COL_DELIM "CSV_COLUMN_DELIMETER"
#define CSV_ROW_DELIM "CSV_ROW_DELIMITER"
#define CSV_HEADER_INFO "CSV_HEADER_INFO"

  if(getenv(CSV_QUOT))
  {
	csv.quote_fields_always=true;
  }
  if(getenv(CSV_COL_DELIM))
  {
	csv.column_delimiter=*getenv(CSV_COL_DELIM);
  }
  if(getenv(CSV_ROW_DELIM))
  {
	csv.row_delimiter=*getenv(CSV_ROW_DELIM);
  }
  if(getenv(CSV_HEADER_INFO))
  {
	csv.use_header_info = true;
  }
  	
  s3selectEngine::csv_object  s3_csv_object;
  s3_csv_object.set_csv_query(&s3select_syntax, csv);

  std::function<void(const char*)> fp_debug = [](const char* msg)
  {
          std::cout << "DEBUG" <<  msg << std::endl;
  };

  //s3_csv_object.set_external_debug_system(fp_debug);

#define BUFF_SIZE (1024*1024*4) //simulate 4mb parts in s3 object
  char* buff = (char*)malloc( BUFF_SIZE );
  while(1)
  {
    buff[0]=0;
    size_t input_sz = fread(buff, 1, BUFF_SIZE, fp);
    char* in=buff;

    if (!input_sz)
    {
	if(fp == stdin)
	{
    		status = s3_csv_object.run_s3select_on_stream(s3select_result, nullptr, 0, 0);
    		if(s3select_result.size()>0)
    		{
      			std::cout << s3select_result;
    		}
	}
	break;
    }

    if(fp != stdin)
    {
    	status = s3_csv_object.run_s3select_on_stream(s3select_result, in, input_sz, statbuf.st_size);
    }
    else
    {
    	status = s3_csv_object.run_s3select_on_stream(s3select_result, in, input_sz, INT_MAX);
    }

    if(status<0)
    {
      std::cout << "failure on execution " << std::endl << s3_csv_object.get_error_description() <<  std::endl;
      break;
    }

    if(s3select_result.size()>0)
    {
      std::cout << s3select_result;
    }

    if(!input_sz || feof(fp) || status == 2)
    {
      break;
    }

    s3select_result.clear();
  }//end-while

    free(buff);
    fclose(fp);

    return 0;
}

std::string get_ranged_string(std::string& inp)
{
    size_t startPos = inp.find("<Payload>");
    size_t endPos = inp.find("</Payload>");

    return inp.substr(startPos,endPos-startPos);
}

int run_on_single_query(const char* fname, const char* query)
{

  std::unique_ptr<awsCli_handler> awscli = std::make_unique<awsCli_handler>() ;
  std::ifstream input_file_stream;
  try {
  	input_file_stream = std::ifstream(fname, std::ios::in | std::ios::binary);
  }
  catch( ... )
  {
	std::cout << "failed to open file " << fname << std::endl;	
	exit(-1);
  }


  if (is_parquet_file(fname))
  {
    std::string result;
    int status = run_query_on_parquet_file(query, fname);
    return status;
  }

  s3select query_ast;
  auto status = query_ast.parse_query(query); 
  if(status<0)	
  {
    std::cout << "failed to parse query : " << query_ast.get_error_description() << std::endl;
    return -1;
  }
   
  if(query_ast.is_json_query())
  {
    return process_json_query(query,fname);
  } 


  auto file_sz = boost::filesystem::file_size(fname);

  std::string buff(BUFFER_SIZE,0);
  while (1)
  {
    size_t read_sz = input_file_stream.read(buff.data(),BUFFER_SIZE).gcount();

    status = awscli->run_s3select(query, buff.data(), read_sz, file_sz);
    if(status<0)
    {
      std::cout << "failure on execution " << std::endl;
      std::cout << get_ranged_string( awscli->get_result() ) << std::endl;
      break;
    }
    else 
    {
    	std::cout << get_ranged_string( awscli->get_result() ) << std::endl;
    }

    if(!read_sz || input_file_stream.eof())
    {
      break;
    }
  }

  return status;
}

int main(int argc,char **argv)
{
	char *query=0;
	char *fname=0;
	char *query_file=0;//file contains many queries

	for (int i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "-key"))
		{//object recieved as CLI parameter
			fname = argv[i + 1];
			continue;
		}

    if (!strcmp(argv[i], "-output"))
		{//object recieved as CLI parameter
			output_format = argv[i + 1];
			continue;
		}

		if (!strcmp(argv[i], "-q"))
		{
			query = argv[i + 1];
			continue;
		}

    if (!strcmp(argv[i], "-HeaderInfo"))
		{
			header_info = argv[i + 1];
			continue;
		}

		if (!strcmp(argv[i], "-cmds"))
		{//query file contain many queries
			query_file = argv[i + 1];
			continue;
		}

		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help"))
		{
			std::cout << "CSV_ALWAYS_QUOT= CSV_COLUMN_DELIMETER= CSV_ROW_DELIMITER= CSV_HEADER_INFO= s3select_example -q \"... query ...\" -key object-path -cmds queries-file" << std::endl; 
			exit(0);
		}
	}

	if(fname  == 0)
	{//object is in query explicitly.
		return run_on_localFile(query);
	}

	if(query_file)
	{
		//purpose: run many queries (reside in file) on single file.
		std::fstream f(query_file, std::ios::in | std::ios::binary);
		const auto sz = boost::filesystem::file_size(query_file);
		std::string result(sz, '\0');
		f.read(result.data(), sz);
		boost::char_separator<char> sep("\n");
		boost::tokenizer<boost::char_separator<char>> tokens(result, sep);

		for (const auto& t : tokens) {
			std::cout << t << std::endl;
			int status = run_on_single_query(fname,t.c_str());
			std::cout << "status: " << status << std::endl;
		}
		
		return(0);
	}

	int status = run_on_single_query(fname,query);
	return status;
}

