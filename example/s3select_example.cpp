#include "s3select.h"
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace s3selectEngine;
using namespace BOOST_SPIRIT_CLASSIC_NS;


bool is_parquet_file(const char * fn)
{//diffrentiate between csv and parquet
   const char * ext = "parquet";

   if(strstr(fn+strlen(fn)-strlen(ext), ext ))
   {
    return true;
   }

    return false;
}

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

  parquet_object parquet_processor(input_file,&s3select_syntax);

  std::string result;

  do
  {
    try
    {
      status = parquet_processor.run_s3select_on_object(result); //TODO error description
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

    std::cout << result << std::endl;

    if (status < 0)
      break;

  } while (1);

  return 0;
}

int main(int argc, char** argv)
{

  //purpose: demostrate the s3select functionalities
  s3select s3select_syntax;

  char*  input_query = 0;

  for (int i = 0; i < argc; i++)
  {

    if (!strcmp(argv[i], "-q"))
    {
      input_query = argv[i + 1];
    }
  }


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

  FILE* fp;

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
  bool to_aggregate=false;

  lstat(object_name.c_str(), &statbuf);

  std::string s3select_result;
  s3selectEngine::csv_object::csv_defintions csv;
  csv.use_header_info = false;
  //csv.column_delimiter='|';
  //csv.row_delimiter='\t';


  s3selectEngine::csv_object  s3_csv_object(&s3select_syntax, csv);
  //s3selectEngine::csv_object  s3_csv_object(&s3select_syntax);

#define BUFF_SIZE 1024*1024*4
  char* buff = (char*)malloc( BUFF_SIZE );
  while(1)
  {
    //char buff[4096];

    //char * in = fgets(buff,sizeof(buff),fp);
    size_t input_sz = fread(buff, 1, BUFF_SIZE, fp);
    char* in=buff;
    //input_sz = strlen(buff);
    //size_t input_sz = in == 0 ? 0 : strlen(in);


    if(fp == stdin)
    {
      if (feof(fp)) 
      {
	to_aggregate = true;
      }
      status = s3_csv_object.run_s3select_on_object(s3select_result,in,input_sz,false,false,to_aggregate);
    }
    else
    {
      status = s3_csv_object.run_s3select_on_stream(s3select_result, in, input_sz, statbuf.st_size);
    }
    if(status<0)
    {
      std::cout << "failure on execution " << std::endl << s3_csv_object.get_error_description() <<  std::endl;
      break;
    }

    if(s3select_result.size()>1)
    {
      std::cout << s3select_result;
    }

    s3select_result = "";
    if(!input_sz || feof(fp))
    {
      break;
    }

  }

  free(buff);
  fclose(fp);

  return 0;
}

