/*
 * /usr/include/boost/bind.hpp:36:1: note: ‘#pragma message: The practice of declaring the Bind placeholders (_1, _2, ...) in the global namespace is deprecated. Please use <boost/bind/bind.hpp> + using namespace boost::placeholders, or define BOOST_BIND_GLOBAL_PLACEHOLDERS to retain the current behavior.’
 */
#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include "s3select.h"
#include "gtest/gtest.h"
#include <string>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

using namespace s3selectEngine;

// parquet conversion
// ============================================================ //
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>

#ifdef _ARROW_EXIST

#include <arrow/io/file.h>
#include <arrow/util/logging.h>

#include <parquet/api/reader.h>
#include <parquet/api/writer.h>

using parquet::ConvertedType;
using parquet::Repetition;
using parquet::Type;
using parquet::schema::GroupNode;
using parquet::schema::PrimitiveNode;

#endif

constexpr int NUM_ROWS = 100000;
constexpr int64_t ROW_GROUP_SIZE = 1024 * 1024;
const char PARQUET_FILENAME[] = "/tmp/csv_converted.parquet";

class tokenize {

  public:
  const char *s;
  std::string input;
  const char *p;
  bool last_token;

  tokenize(std::string& in):s(0),input(in),p(input.c_str()),last_token(false)
  {
  };

  void get_token(std::string& token)
  {
     if(!*p)
     {
      token = "";
      last_token = true;
      return;
     }

     s=p;
     while(*p && *p != ',' && *p != '\n') p++;

     token = std::string(s,p);
     p++;
  }

  bool is_last()
  {
    return last_token == true;
  }
};

#ifdef _ARROW_EXIST

static std::shared_ptr<GroupNode> column_string_2(uint32_t num_of_columns) {

    parquet::schema::NodeVector fields;

    for(uint32_t i=0;i<num_of_columns;i++)
    {
      std::string column_name = "column_" + std::to_string(i) ;
      fields.push_back(PrimitiveNode::Make(column_name, Repetition::OPTIONAL,  Type::BYTE_ARRAY,
          ConvertedType::NONE));
    }

  return std::static_pointer_cast<GroupNode>(
      GroupNode::Make("schema", Repetition::REQUIRED, fields));
}

int csv_to_parquet(std::string & csv_object)
{

  auto csv_num_of_columns = std::count( csv_object.begin(),csv_object.begin() + csv_object.find('\n'),',')+1;
  auto csv_num_of_rows = std::count(csv_object.begin(),csv_object.end(),'\n');

  tokenize csv_tokens(csv_object);

  try {
    // Create a local file output stream instance.

    using FileClass = ::arrow::io::FileOutputStream;
    std::shared_ptr<FileClass> out_file;
    PARQUET_ASSIGN_OR_THROW(out_file, FileClass::Open(PARQUET_FILENAME));

    // Setup the parquet schema
    std::shared_ptr<GroupNode> schema = column_string_2(csv_num_of_columns);

    // Add writer properties
    parquet::WriterProperties::Builder builder;
    // builder.compression(parquet::Compression::SNAPPY);
    std::shared_ptr<parquet::WriterProperties> props = builder.build();

    // Create a ParquetFileWriter instance
    std::shared_ptr<parquet::ParquetFileWriter> file_writer =
      parquet::ParquetFileWriter::Open(out_file, schema, props);

    // Append a BufferedRowGroup to keep the RowGroup open until a certain size
    parquet::RowGroupWriter* rg_writer = file_writer->AppendBufferedRowGroup();

    int num_columns = file_writer->num_columns();
    std::vector<int64_t> buffered_values_estimate(num_columns, 0);

    for (int i = 0; !csv_tokens.is_last() && i<csv_num_of_rows; i++) {
      int64_t estimated_bytes = 0;
      // Get the estimated size of the values that are not written to a page yet
      for (int n = 0; n < num_columns; n++) {
        estimated_bytes += buffered_values_estimate[n];
      }

      // We need to consider the compressed pages
      // as well as the values that are not compressed yet
      if ((rg_writer->total_bytes_written() + rg_writer->total_compressed_bytes() +
            estimated_bytes) > ROW_GROUP_SIZE) {
        rg_writer->Close();
        std::fill(buffered_values_estimate.begin(), buffered_values_estimate.end(), 0);
        rg_writer = file_writer->AppendBufferedRowGroup();
      }


      int col_id;
      for(col_id=0;col_id<num_columns && !csv_tokens.is_last();col_id++)
      {
        // Write the byte-array column
        parquet::ByteArrayWriter* ba_writer =
          static_cast<parquet::ByteArrayWriter*>(rg_writer->column(col_id));
        parquet::ByteArray ba_value;

        std::string token;
        csv_tokens.get_token(token);
        if(token.size() == 0)
        {//null column
          int16_t definition_level = 0;
          ba_writer->WriteBatch(1, &definition_level, nullptr, nullptr);
        }
        else
        {
          int16_t definition_level = 1;
          ba_value.ptr = (uint8_t*)(token.data());
          ba_value.len = token.size();
          ba_writer->WriteBatch(1, &definition_level, nullptr, &ba_value);
        }

        buffered_values_estimate[col_id] = ba_writer->EstimatedBufferedValueBytes();


      } //end-for columns

      if(csv_tokens.is_last() && col_id<num_columns)
      {
        for(;col_id<num_columns;col_id++)
        {
          parquet::ByteArrayWriter* ba_writer =
            static_cast<parquet::ByteArrayWriter*>(rg_writer->column(col_id));

          int16_t definition_level = 0;
          ba_writer->WriteBatch(1, &definition_level, nullptr, nullptr);

          buffered_values_estimate[col_id] = ba_writer->EstimatedBufferedValueBytes();
        }

      }

    }  // end-for rows

    // Close the RowGroupWriter
    rg_writer->Close();
    // Close the ParquetFileWriter
    file_writer->Close();

    // Write the bytes to file
    DCHECK(out_file->Close().ok());

  } catch (const std::exception& e) {
    std::cerr << "Parquet write error: " << e.what() << std::endl;
    return -1;
  }

  return 0;
}

int run_query_on_parquet_file(const char* input_query, const char* input_file, std::string &result)
{
  int status;
  s3select s3select_syntax;
  result.clear();

  status = s3select_syntax.parse_query(input_query);
  if (status != 0)
  {
    std::cout << "failed to parse query " << s3select_syntax.get_error_description() << std::endl;
    return -1;
  }

  FILE *fp=nullptr;

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

  std::function<int(std::string&)> fp_s3select_result_format = [](std::string& result){return 0;};//append
  std::function<int(std::string&)> fp_s3select_header_format = [](std::string& result){return 0;};//append

  parquet_object parquet_processor(input_file,&s3select_syntax,&rgw);

  //std::string result;

  do
  {
    try
    {
      status = parquet_processor.run_s3select_on_object(result,fp_s3select_result_format,fp_s3select_header_format);
    }
    catch (base_s3select_exception &e)
    {
      if (e.severity() == base_s3select_exception::s3select_exp_en_t::FATAL) //abort query execution
      {
        if(fp){
          fclose(fp);
        }
        return -1;
      }
    }

    if (status < 0)
      break;

  } while (0);

  if(fp){
    fclose(fp);
  }
  return 0;
}// ============================================================ //
#else
int run_query_on_parquet_file(const char* input_query, const char* input_file, std::string &result)
{
  return 0;
}
#endif //_ARROW_EXIST

std::string convert_to_json(const char* csv_stream, size_t stream_length)
{
    char* m_stream;
    char* m_end_stream;
    char row_delimiter('\n');
    char column_delimiter(',');
    bool previous{true};

    m_stream = (char*)csv_stream;
    m_end_stream = (char*)csv_stream + stream_length;
    std::stringstream ss;
    ss << std::endl;
    ss << "{\"root\" : [";
    ss << std::endl;
    while (m_stream < m_end_stream) {
      int counter{};
      ss << "{";
      while( *m_stream && (*m_stream != row_delimiter) )  {
          if (*m_stream != column_delimiter && previous)  {
            ss << "\"c" << ++counter << "\"" << ":";
            ss << "\"";
            ss << *m_stream;
            previous = false;
          } else if (*m_stream != column_delimiter) {
              ss << *m_stream;
          } else if (*m_stream == column_delimiter) {
              if (previous)  {
                ss << "\"c" << ++counter << "\"" << ":";
                ss << "null";
              } else {
              ss << "\"";
              }
              ss << ",";
              previous = true;
          }
        m_stream++;
      }
      if(previous)  {
          ss.seekp(-1, std::ios_base::end);
      } else {
          ss << "\"";
      }
      previous = true;
      ss << "}" << ',' << std::endl;
      m_stream++;
    }
    ss.seekp(-2, std::ios_base::end);
    ss << std::endl;
    ss << "]" << "}";
    return ss.str();
}

const char* convert_query(std::string& expression)
{
  std::string from_clause = "s3object";
  boost::replace_all(expression, from_clause, "s3object[*].root");

  std::string from_clause_1 = "stdin";
  boost::replace_all(expression, from_clause_1, "s3object[*].root");

  std::string col_1 = "_1";
  boost::replace_all(expression, col_1, "_1.c1");

  std::string col_2 = "_2";
  boost::replace_all(expression, col_2, "_1.c2");

  std::string col_3 = "_3";
  boost::replace_all(expression, col_3, "_1.c3");

  std::string col_4 = "_4";
  boost::replace_all(expression, col_4, "_1.c4");

  std::string col_5 = "_5";
  boost::replace_all(expression, col_5, "_1.c5");

  std::string col_9 = "_9";
  boost::replace_all(expression, col_9, "_1.c9");

  return expression.c_str();
}


std::string run_expression_in_C_prog(const char* expression)
{
//purpose: per use-case a c-file is generated, compiles , and finally executed.

// side note: its possible to do the following: cat test_hello.c |  gcc  -pipe -x c - -o /dev/stdout > ./1
// gcc can read and write from/to pipe (use pipe2()) i.e. not using file-system , BUT should also run gcc-output from memory

  const int C_FILE_SIZE=(1024*1024);
  std::string c_test_file = std::string("/tmp/test_s3.c");
  std::string c_run_file = std::string("/tmp/s3test");

  FILE* fp_c_file = fopen(c_test_file.c_str(), "w");

  //contain return result
  char result_buff[100];

  char* prog_c = 0;

  if(fp_c_file)
  {
    prog_c = (char*)malloc(C_FILE_SIZE);

                size_t sz=sprintf(prog_c,"#include <stdio.h>\n \
                                #include <float.h>\n \
                                int main() \
                                {\
                                printf(\"%%.*e\\n\",DECIMAL_DIG,(double)(%s));\
                                } ", expression);

    fwrite(prog_c, 1, sz, fp_c_file);
    fclose(fp_c_file);
  }

  std::string gcc_and_run_cmd = std::string("gcc ") + c_test_file + " -o " + c_run_file + " -Wall && " + c_run_file;

  FILE* fp_build = popen(gcc_and_run_cmd.c_str(), "r"); //TODO read stderr from pipe

  if(!fp_build)
  {
    if(prog_c)
        free(prog_c);

    return std::string("#ERROR#");
  }

  char * res = fgets(result_buff, sizeof(result_buff), fp_build);

  if(!res)
  {
    return std::string("#ERROR#");
  }

  unlink(c_run_file.c_str());
  unlink(c_test_file.c_str());
  fclose(fp_build);

  if(prog_c)
    free(prog_c);

  return std::string(result_buff);
}

#define OPER oper[ rand() % oper.size() ]

class gen_expr
{

private:

  int open = 0;
  std::string oper= {"+-+*/*"};

  std::string gexpr()
  {
    return std::to_string(rand() % 1000) + ".0" + OPER + std::to_string(rand() % 1000) + ".0";
  }

  std::string g_openp()
  {
    if ((rand() % 3) == 0)
    {
      open++;
      return std::string("(");
    }
    return std::string("");
  }

  std::string g_closep()
  {
    if ((rand() % 2) == 0 && open > 0)
    {
      open--;
      return std::string(")");
    }
    return std::string("");
  }

public:

  std::string generate()
  {
    std::string exp = "";
    open = 0;

    for (int i = 0; i < 10; i++)
    {
      exp = (exp.size() > 0 ? exp + OPER : std::string("")) + g_openp() + gexpr() + OPER + gexpr() + g_closep();
    }

    if (open)
      for (; open--;)
      {
        exp += ")";
      }

    return exp;
  }
};

const std::string failure_sign("#failure#");

std::string string_to_quot(std::string& s, char quot = '"')
{
  std::string result = "";
  std::stringstream str_strm;
  str_strm << s;
  std::string temp_str;
  int temp_int;
  while(!str_strm.eof()) {
    str_strm >> temp_str;
    if(std::stringstream(temp_str) >> temp_int) {
      std::stringstream s1;
      s1 << temp_int;
      result +=  quot + s1.str() +  quot + "\n";
    }
    temp_str = "";
  }
  return result;
}

void parquet_csv_report_error(std::string parquet_result, std::string csv_result)
{
#ifdef _ARROW_EXIST
  ASSERT_EQ(parquet_result,csv_result);
#else
  ASSERT_EQ(0,0);
#endif
}

void json_csv_report_error(std::string json_result, std::string csv_result)
{
  ASSERT_EQ(json_result, csv_result);
}

std::string run_s3select(std::string expression)
{//purpose: run query on single row and return result(single projections).
  s3select s3select_syntax;

  int status = s3select_syntax.parse_query(expression.c_str());

  if(status)
    return failure_sign;

  std::string s3select_result;
  s3selectEngine::csv_object  s3_csv_object(&s3select_syntax);
  std::string in = "1,1,1,1\n";
  std::string csv_obj = in;
  std::string parquet_result;

  s3_csv_object.run_s3select_on_object(s3select_result, in.c_str(), in.size(), false, false, true);

  s3select_result = s3select_result.substr(0, s3select_result.find_first_of(","));
  s3select_result = s3select_result.substr(0, s3select_result.find_first_of("\n"));//remove last \n

#ifdef _ARROW_EXIST
  csv_to_parquet(csv_obj);
  run_query_on_parquet_file(expression.c_str(),PARQUET_FILENAME,parquet_result);
  parquet_result = parquet_result.substr(0, parquet_result.find_first_of(","));
  parquet_result = parquet_result.substr(0, parquet_result.find_first_of("\n"));//remove last \n

  parquet_csv_report_error(parquet_result,s3select_result);
#endif

  return s3select_result;
}

void run_s3select_test_opserialization(std::string expression,std::string input, char *row_delimiter, char *column_delimiter)
{//purpose: run query on multiple rows and return result(multiple projections).
    s3select s3select_syntax;

    int status = s3select_syntax.parse_query(expression.c_str());

    if(status)
      return;

    std::string s3select_result;
    csv_object::csv_defintions csv;
    csv.redundant_column = false;

    csv.output_row_delimiter = *row_delimiter;
    csv.output_column_delimiter = *column_delimiter;

    s3selectEngine::csv_object s3_csv_object(&s3select_syntax, csv);

    s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), false, false, true);

    std::string s3select_result1 = s3select_result;

    csv.row_delimiter = *row_delimiter;
    csv.column_delimiter = *column_delimiter;
    csv.output_row_delimiter = *row_delimiter;
    csv.output_column_delimiter = *column_delimiter;
    csv.redundant_column = false;
    std::string s3select_result_second_phase;

    s3selectEngine::csv_object s3_csv_object_second(&s3select_syntax, csv);

    s3_csv_object_second.run_s3select_on_object(s3select_result_second_phase, s3select_result.c_str(), s3select_result.size(), false, false, true);

    ASSERT_EQ(s3select_result_second_phase, s3select_result1);
}

std::string run_s3select_opserialization_quot(std::string expression,std::string input, bool quot_always = false, char quot_char = '"')
{//purpose: run query on multiple rows and return result(multiple projections).
    s3select s3select_syntax;

    int status = s3select_syntax.parse_query(expression.c_str());

    if(status)
      return failure_sign;

    std::string s3select_result;
    csv_object::csv_defintions csv;

    csv.redundant_column = false;
    csv.quote_fields_always = quot_always;
    csv.output_quot_char = quot_char;

    s3selectEngine::csv_object s3_csv_object(&s3select_syntax, csv);

    s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), false, false, true);

    return s3select_result;
}

// JSON tests API's
int run_json_query(const char* json_query, std::string& json_input,std::string& result)
{//purpose: run single-chunk json queries

  s3select s3select_syntax;
  int status = s3select_syntax.parse_query(json_query);
  if (status != 0)
  {
    std::cout << "failed to parse query " << s3select_syntax.get_error_description() << std::endl;
    return -1;
  }

  json_object json_query_processor(&s3select_syntax);
  status = json_query_processor.run_s3select_on_stream(result, json_input.data(), json_input.size(), json_input.size());
  std::string prev_result = result;
  status = json_query_processor.run_s3select_on_stream(result, 0, 0, json_input.size());
  
  result = prev_result + result;

  return status;
}

std::string run_s3select(std::string expression,std::string input, const char* json_query = "")
{//purpose: run query on multiple rows and return result(multiple projections).
  s3select s3select_syntax;
  std::string parquet_input = input;

  std::string js = convert_to_json(input.c_str(), input.size());

  int status = s3select_syntax.parse_query(expression.c_str());

  if(status)
    return failure_sign;

  std::string s3select_result;
  std::string json_result;
  s3selectEngine::csv_object  s3_csv_object(&s3select_syntax);
  s3_csv_object.m_csv_defintion.redundant_column = false;

  s3_csv_object.run_s3select_on_object(s3select_result, input.c_str(), input.size(), false, false, true);

#ifdef _ARROW_EXIST
  static int file_no = 1;
  csv_to_parquet(parquet_input);
  std::string parquet_result;
  run_query_on_parquet_file(expression.c_str(),PARQUET_FILENAME,parquet_result);

  if (strcmp(parquet_result.c_str(),s3select_result.c_str()))
  {
    std::cout << "failed on query " << expression << std::endl;
    std::cout << "input for query reside on" << "./failed_test_input" << std::to_string(file_no) << ".[csv|parquet]" << std::endl;

    {
      std::string buffer;

      std::ifstream f(PARQUET_FILENAME);
      f.seekg(0, std::ios::end);
      buffer.resize(f.tellg());
      f.seekg(0);
      f.read(buffer.data(), buffer.size());

      std::string fn = std::string("./failed_test_input_") + std::to_string(file_no) + std::string(".parquet");
      std::ofstream fw(fn.c_str());
      fw.write(buffer.data(), buffer.size());

      fn = std::string("./failed_test_input_") + std::to_string(file_no++) + std::string(".csv");
      std::ofstream fw2(fn.c_str());
      fw2.write(parquet_input.data(), parquet_input.size());

    }
  }

  parquet_csv_report_error(parquet_result,s3select_result);
#endif //_ARROW_EXIST
  
  if(strlen(json_query) == 0) {
    json_query = convert_query(expression);
  }

  run_json_query(json_query, js, json_result);
  json_csv_report_error(json_result, s3select_result);

  return s3select_result;
}



