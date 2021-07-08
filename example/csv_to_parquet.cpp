
// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements. See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership. The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the License for the
// specific language governing permissions and limitations
// under the License.

#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <iomanip>
#include <algorithm>
#include "boost/date_time/gregorian/gregorian.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include <arrow/io/file.h>
#include <arrow/util/logging.h>

#include <parquet/api/reader.h>
#include <parquet/api/writer.h>

using parquet::ConvertedType;
using parquet::Repetition;
using parquet::Type;
using parquet::schema::GroupNode;
using parquet::schema::PrimitiveNode;

/*
 * This example describes writing and reading Parquet Files in C++ and serves as a
 * reference to the API.
 * The file contains all the physical data types supported by Parquet.
 * This example uses the RowGroupWriter API that supports writing RowGroups based on a
 *certain size
 **/

/* Parquet is a structured columnar file format
 * Parquet File = "Parquet data" + "Parquet Metadata"
 * "Parquet data" is simply a vector of RowGroups. Each RowGroup is a batch of rows in a
 * columnar layout
 * "Parquet Metadata" contains the "file schema" and attributes of the RowGroups and their
 * Columns
 * "file schema" is a tree where each node is either a primitive type (leaf nodes) or a
 * complex (nested) type (internal nodes)
 * For specific details, please refer the format here:
 * https://github.com/apache/parquet-format/blob/master/LogicalTypes.md
 **/

#include <string>
#include <boost/tokenizer.hpp>
using namespace boost;
using namespace std;

//constexpr int NUM_ROWS = 10000000;
constexpr int NUM_ROWS = 10000;

//constexpr int64_t ROW_GROUP_SIZE = 16 * 1024 * 1024;  // 16 MB
constexpr int64_t ROW_GROUP_SIZE = 1024 * 1024;  

const char PARQUET_FILENAME[] = "csv_converted.parquet";

static std::shared_ptr<GroupNode> column_string_2(uint32_t num_of_columns) {

    parquet::schema::NodeVector fields;

    for(uint32_t i=0;i<num_of_columns;i++)
    {
      std::string column_name = "column_" + to_string(i) ;
      fields.push_back(PrimitiveNode::Make(column_name, Repetition::OPTIONAL,  Type::BYTE_ARRAY,
	  ConvertedType::NONE));
    }

  return std::static_pointer_cast<GroupNode>(
      GroupNode::Make("schema", Repetition::REQUIRED, fields));
}


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

void generate_rand_columns_csv_datetime(std::string& out, size_t size) {
  std::stringstream ss;
  auto year = [](){return rand()%100 + 1900;};
  auto month = [](){return 1 + rand()%12;};
  auto day = [](){return 1 + rand()%28;};
  auto hours = [](){return rand()%24;};
  auto minutes = [](){return rand()%60;};
  auto seconds = [](){return rand()%60;};

  for (auto i = 0U; i < size; ++i) {
    ss << year() << "-" << std::setw(2) << std::setfill('0')<< month() << "-" << std::setw(2) << std::setfill('0')<< day() << "T" <<std::setw(2) << std::setfill('0')<< hours() << ":" << std::setw(2) << std::setfill('0')<< minutes() << ":" << std::setw(2) << std::setfill('0')<<seconds() << "Z" << "," << std::endl;
  }
  out = ss.str();
}

void generate_columns_csv(std::string& out, size_t size) {
  std::stringstream ss;

  for (auto i = 0U; i < size; ++i) {
    ss << i << "," << i+1 << "," << i << "," << i << "," << i << "," << i << "," << i << "," << i << "," << i << "," << i << std::endl;
  }
  out = ss.str();
}

void generate_rand_columns_csv_with_null(std::string& out, size_t size) {
  std::stringstream ss;
  auto r = [](){ int x=rand()%1000;if (x<100) return std::string(""); else return std::to_string(x);};

  for (auto i = 0U; i < size; ++i) {
    ss << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << std::endl;
  }
  out = ss.str();
}

void generate_fix_columns_csv(std::string& out, size_t size) {
  std::stringstream ss;
  for (auto i = 0U; i < size; ++i) {
    ss << 1 << "," << 2 << "," << 3 << "," << 4 << "," << 5 << std::endl;
  }
  out = ss.str();
}

void generate_rand_csv_datetime_to_string(std::string& out, std::string& result, size_t size, bool const_frmt = true) {

  std::stringstream ss_out, ss_res;
  std::string format = "yyyysMMMMMdddSSSSSSSSSSSMMMM HHa:m -:-";
  std::string months[12] = {"January", "February", "March","April", "May", "June", "July", "August", "September", "October", "November", "December"};
  auto year = [](){return rand()%100 + 1900;};
  auto month = [](){return 1 + rand()%12;};
  auto day = [](){return 1 + rand()%28;};
  auto hours = [](){return rand()%24;};
  auto minutes = [](){return rand()%60;};
  auto seconds = [](){return rand()%60;};
  auto fracation_sec = [](){return rand()%1000000;};

  for (auto i = 0U; i < size; ++i)
  {
    auto yr = year();
    auto mnth = month();
    auto dy = day();
    auto hr = hours();
    auto mint = minutes();
    auto sec = seconds();
    auto frac_sec = fracation_sec();

    if (const_frmt)
    {
      ss_out << yr << "-" << std::setw(2) << std::setfill('0') << mnth << "-" << std::setw(2) << std::setfill('0') << dy << "T" <<std::setw(2) << std::setfill('0') << hr << ":" << std::setw(2) << std::setfill('0') << mint << ":" << std::setw(2) << std::setfill('0') <<sec << "." << frac_sec << "Z" << "," << std::endl;

      ss_res << yr << sec << months[mnth-1].substr(0, 1) << std::setw(2) << std::setfill('0') << dy << dy << frac_sec << std::string(11 - std::to_string(frac_sec).length(), '0') << months[mnth-1] << " " << std::setw(2) << std::setfill('0') << hr << (hr < 12 ? "AM" : "PM") << ":" << mint << " -:-" << "," << std::endl;
    }
    else
    {
      switch(rand()%5)
      {
        case 0:
            format = "yyyysMMMMMdddSSSSSSSSSSSMMMM HHa:m -:-";
            ss_res << yr << sec << months[mnth-1].substr(0, 1) << std::setw(2) << std::setfill('0') << dy << dy << frac_sec << std::string(11 - std::to_string(frac_sec).length(), '0') << months[mnth-1] << " " << std::setw(2) << std::setfill('0') << hr << (hr < 12 ? "AM" : "PM") << ":" << mint << " -:-" << "," << std::endl;
            break;
        case 1:
            format = "aMMhh";
            ss_res << (hr < 12 ? "AM" : "PM") << std::setw(2) << std::setfill('0') << mnth << std::setw(2) << std::setfill('0') << (hr%12 == 0 ? 12 : hr%12) << "," << std::endl;
            break;
        case 2:
            format = "y M d ABCDEF";
            ss_res << yr << " " << mnth << " " << dy << " ABCDEF" << "," << std::endl;
            break;
        case 3:
            format = "W h:MMMM";
            ss_res << "W " << (hr%12 == 0 ? 12 : hr%12) << ":" << months[mnth-1] << "," << std::endl;
            break;
        case 4:
            format = "H:m:s";
            ss_res << hr << ":" << mint << ":" << sec << "," << std::endl;
            break;
      }

      ss_out << yr << "-" << std::setw(2) << std::setfill('0') << mnth << "-" << std::setw(2) << std::setfill('0') << dy << "T" <<std::setw(2) << std::setfill('0') << hr << ":" << std::setw(2) << std::setfill('0') << mint << ":" << std::setw(2) << std::setfill('0') <<sec << "." << frac_sec << "Z" << "," << format << "," << std::endl;
    }
  }
  out = ss_out.str();
  result = ss_res.str();
}
void generate_rand_columns_csv(std::string& out, size_t size) {
  std::stringstream ss;
  auto r = [](){return rand()%1000;};

  for (auto i = 0U; i < size; ++i) {
    ss << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << "," << r() << std::endl;
  }
  out = ss.str();
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


static int csv_file_to_parquet(int argc,char **argv)
{
  //open file (CSV) and load into std::string, convert to parquet(save to FS)

  if (argc<2) exit(-1);

  FILE* fp;
  struct stat l_buf;
  int st = lstat(argv[1], &l_buf);
  if(st<0) exit(-1);

  printf("input csv file size = %lld\n",l_buf.st_size);

  char * buffer = new char[ l_buf.st_size ];
  fp = fopen(argv[1],"r");

  if(!fp) exit(-1);

  fread(buffer,l_buf.st_size,1,fp);

  std::string csv_obj;
  csv_obj.append(buffer,l_buf.st_size);

  csv_to_parquet(csv_obj);

  return 0; 
}

int csv_object_to_parquet(int argc,char **argv)
{
  srand(time(0));

  std::string csv_obj;
  std::string expected_result;
  generate_rand_columns_csv(csv_obj, 128);
  //generate_rand_csv_datetime_to_string(csv_obj, expected_result, 10000);
  //generate_rand_columns_csv_with_null(csv_obj, 10000);
  //generate_columns_csv(csv_obj,128);
  //generate_rand_columns_csv_datetime(csv_obj,10000);
  generate_fix_columns_csv(csv_obj,128);
  FILE *fp = fopen("10k.csv","w");

  if(fp)
  {
    fwrite(csv_obj.data(),csv_obj.size(),1,fp);
    fclose(fp);
  }
  else
  {	
    exit(-1);
  }

  //csv_obj="1,2,3,4,5,6,7,8,9,10\n10,20,30,40,50,60,70,80,90,100\n";
  csv_obj="1,2,3,4\n";

  csv_to_parquet(csv_obj);

  return 0;
}

int main(int argc,char **argv)
{
  return csv_file_to_parquet(argc,argv);
}

