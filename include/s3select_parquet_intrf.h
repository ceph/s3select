
#pragma once

#include <iostream>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>
#include <set>

/******************************************/
class parquet_file_parser {
 
 public:
  enum class parquet_type {STRING,INT32,INT64,DOUBLE};
  typedef std::vector<std::pair<std::string,parquet_type>> schema_t;
 private:  
 
  std::string parquet_file_name;
  uint32_t m_num_of_columms;
  uint64_t m_num_of_rows;
  uint64_t m_rownum;
  uint64_t m_chunk_num;
  schema_t m_schm;
  std::shared_ptr<arrow::Table> m_table;
  std::shared_ptr<arrow::io::ReadableFile> m_infile;
  std::unique_ptr<parquet::arrow::FileReader> m_reader;

  int load_meta_data() {
    PARQUET_ASSIGN_OR_THROW(m_infile, arrow::io::ReadableFile::Open(
                                        parquet_file_name, arrow::default_memory_pool()));

    PARQUET_THROW_NOT_OK(
        parquet::arrow::OpenFile(m_infile, arrow::default_memory_pool(), &m_reader));

    m_reader->set_use_threads(false);

    std::shared_ptr<::arrow::Schema> scm;
    m_reader->GetSchema(&scm);

    m_num_of_columms = scm.get()->fields().size();

    for (auto x : scm.get()->fields()) {
      //parquet schema is uploaded as pairs <column-name,data-type>
      if(x.get()->type().get()->ToString().compare("string")==0)
      {
          std::pair<std::string, parquet_type> elm(x.get()->name(),parquet_type::STRING);
          m_schm.push_back(elm);
      }
      else if(x.get()->type().get()->ToString().compare("int32")==0)
      {
          std::pair<std::string, parquet_type> elm(x.get()->name(),parquet_type::INT32);
          m_schm.push_back(elm);  
      }
      else if(x.get()->type().get()->ToString().compare("int64")==0)
      {
          std::pair<std::string, parquet_type> elm(x.get()->name(),parquet_type::INT64);
          m_schm.push_back(elm);  
      }
      else if(x.get()->type().get()->ToString().compare("double")==0)
      {
          std::pair<std::string, parquet_type> elm(x.get()->name(),parquet_type::DOUBLE);
          m_schm.push_back(elm);
      }
      else
      {
        return -1;//TODO throw exception
      }
    }

    return 0;
  }

 public:

  typedef std::set<uint16_t> column_pos_t;
   
  typedef struct
  {
    int64_t num;
    char* str;//str is pointing to offset in string which is NOT null terminated.
    uint16_t str_len;
    double dbl;
    parquet_type type;
  } parquet_value_t;

  typedef std::vector<parquet_value_t> row_values_t;

  parquet_file_parser(std::string name) : 
  parquet_file_name(name),
  m_num_of_columms(0),
  m_num_of_rows(0),
  m_rownum(0),
  m_chunk_num(0) 
  {load_meta_data();}

  bool end_of_stream()
  {
    if(m_table.get() == 0)
    {
      PARQUET_THROW_NOT_OK(m_reader->ReadTable(&m_table));
      m_num_of_rows = m_table->num_rows();
    }

    if(m_rownum >= m_num_of_rows)
    {//next chunk if exists
      if(m_table->column(0)->num_chunks()<m_chunk_num)
      {
        m_chunk_num++;
        m_rownum = m_table->column(0)->chunk(m_chunk_num)->length();
      }
    }

    if(m_rownum >= m_num_of_rows)
      return true;
    return false;
  }

  uint64_t get_number_of_rows()
  {
    return m_num_of_rows;
  }

  bool increase_rownum()
  {
    if(end_of_stream())
      return false;

    m_rownum++;
    return true;
  }

  uint64_t get_rownum()
  {
    return m_rownum;
  }

  uint32_t get_num_of_columns()
  {
    return m_num_of_columms;
  }

  int get_column_values_by_positions(column_pos_t positions, row_values_t &row_values)
  {
    //per each position get type , extract and push to row_values (intensive method)
    //TODO next chunk
    parquet_value_t column_value;
    row_values.clear();

    if (end_of_stream())
      return -1;

    for (auto idx : positions)
    {
      switch (m_schm[idx].second)
      {
        case parquet_type::STRING:
        {
          auto string_column =
              std::static_pointer_cast<arrow::StringArray>(m_table->column(idx)->chunk(m_chunk_num));

          column_value.str = (char *)string_column->raw_data() + string_column->value_offset(m_rownum);
          column_value.str_len = string_column->value_length(m_rownum);
          column_value.type = parquet_type::STRING;
        }
        break;

        case parquet_type::INT64:
        {
          column_value.num =
              std::static_pointer_cast<arrow::Int64Array>(m_table->column(idx)->chunk(m_chunk_num))->Value(m_rownum);
          column_value.type = parquet_type::INT64;
        }
        break;

        case parquet_type::INT32:
        {
          column_value.num =
              std::static_pointer_cast<arrow::Int32Array>(m_table->column(idx)->chunk(m_chunk_num))->Value(m_rownum);
          column_value.type = parquet_type::INT32;
        }
        break;

        case parquet_type::DOUBLE:
        {
          column_value.dbl =
              std::static_pointer_cast<arrow::DoubleArray>(m_table->column(idx)->chunk(m_chunk_num))->Value(m_rownum);
          column_value.type = parquet_type::DOUBLE;
        }
        break;

        default:
        //TODO throw exception
        return -1;
      }
        row_values.push_back(column_value);
    }

    return 0;
  }

  uint16_t get_column_id(std::string column_name) {
    uint16_t pos = 0;
    // search meta-data from column-id by name (done once per query)
    for (auto x : m_table->fields()) {
      if (x.get()->name().compare(column_name) == 0) {
        return pos;
      }
      pos++;
    }
    return -1;
  }

  schema_t get_schema()
  {
    return m_schm;
  }

};
