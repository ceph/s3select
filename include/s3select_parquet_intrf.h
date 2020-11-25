
#pragma once

#include <iostream>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>
#include <set>
#include <parquet/column_reader.h>

/******************************************/
class column_reader_wrap
{

private:

  int64_t m_rownum;
  parquet::Type::type m_type;
  std::shared_ptr<parquet::RowGroupReader> m_row_group_reader;
  uint32_t m_row_grouop_id;
  uint16_t m_col_id;
  parquet::ParquetFileReader* m_parquet_reader;
  std::shared_ptr<parquet::ColumnReader> m_ColumnReader;
  bool m_end_of_stream;
  bool m_read_last_value;
  

public:

  enum class parquet_type
  {
    STRING,
    INT32,
    INT64,
    DOUBLE
  };

  typedef struct
  {
    int64_t num;
    char *str; //str is pointing to offset in string which is NOT null terminated.
    uint16_t str_len;
    double dbl;
    parquet_type type;
  } parquet_value_t;

  private:
  parquet_value_t m_last_value;

  public:
  column_reader_wrap(std::unique_ptr<parquet::ParquetFileReader> & parquet_reader,uint16_t col_id);

  parquet::Type::type get_type();

  bool HasNext();//TODO template 

  int64_t ReadBatch(int64_t batch_size, int16_t* def_levels, int16_t* rep_levels,
                            parquet_value_t* values, int64_t* values_read);

  int64_t Skip(int64_t rows_to_skip);

  int Read(uint64_t rownum,parquet_value_t & value);

};

class parquet_file_parser
{

public:

  typedef std::vector<std::pair<std::string, column_reader_wrap::parquet_type>> schema_t;
  typedef std::set<uint16_t> column_pos_t;
  typedef std::vector<column_reader_wrap::parquet_value_t> row_values_t;

  typedef column_reader_wrap::parquet_value_t parquet_value_t;
  typedef column_reader_wrap::parquet_type parquet_type;

private:

  std::string m_parquet_file_name;
  uint32_t m_num_of_columms;
  uint64_t m_num_of_rows;
  uint64_t m_rownum;
  schema_t m_schm;
  int m_num_row_groups;
  std::shared_ptr<parquet::FileMetaData> m_file_metadata;
  std::unique_ptr<parquet::ParquetFileReader> m_parquet_reader;
  std::vector<column_reader_wrap*> m_column_readers;

  public:

  parquet_file_parser(std::string parquet_file_name) : 
                                   m_parquet_file_name(parquet_file_name),
                                   m_num_of_columms(0),
                                   m_num_of_rows(0),
                                   m_rownum(0),
                                   m_num_row_groups(0)
                                   
                                   
  {
    load_meta_data();
  }

  int load_meta_data()
  {
    m_parquet_reader = parquet::ParquetFileReader::OpenFile(m_parquet_file_name, false);
    m_file_metadata = m_parquet_reader->metadata();
    m_num_of_columms = m_parquet_reader->metadata()->num_columns();
    m_num_row_groups = m_file_metadata->num_row_groups();
    m_num_of_rows = m_file_metadata->num_rows();

    for (int i = 0; i < m_num_of_columms; i++)
    {
      parquet::Type::type tp = m_file_metadata->schema()->Column(i)->physical_type();
      std::pair<std::string, column_reader_wrap::parquet_type> elm;

      switch (tp)
      {
      case parquet::Type::type::INT32:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::INT32);
        m_schm.push_back(elm);
        break;

      case parquet::Type::type::INT64:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::INT64);
        m_schm.push_back(elm);
        break;

      case parquet::Type::type::DOUBLE:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::DOUBLE);
        m_schm.push_back(elm);
        break;

      case parquet::Type::type::BYTE_ARRAY:
        elm = std::pair<std::string, column_reader_wrap::parquet_type>(m_file_metadata->schema()->Column(i)->name(), column_reader_wrap::parquet_type::STRING);
        m_schm.push_back(elm);
        break;

      default:
        //throw base_s3select_exception("some parquet type not supported",s3selectEngine::base_s3select_exception::s3select_exp_en_t::FATAL);//TODO better message
        return -1;
      }

      m_column_readers.push_back(new column_reader_wrap(m_parquet_reader,i));
    }

    return 0;
  }

  bool end_of_stream()
  {

    if (m_rownum >= m_num_of_rows)
      return true;
    return false;
  }

  uint64_t get_number_of_rows()
  {
    return m_num_of_rows;
  }

  bool increase_rownum()
  {
    if (end_of_stream())
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
    column_reader_wrap::parquet_value_t column_value;
    row_values.clear();

    for(auto col : positions)
    {
      if((col)>=m_num_of_columms)
      {//TODO should verified upon syntax phase 
        //TODO throw exception
        return -1;
      }
      m_column_readers[col]->Read(m_rownum,column_value);
      row_values.push_back(column_value);//TODO intensive (should move)
    }
    return 0;
  }

  schema_t get_schema()
  {
    return m_schm;
  }
};

/******************************************/


  column_reader_wrap::column_reader_wrap(std::unique_ptr<parquet::ParquetFileReader> & parquet_reader,uint16_t col_id):
  m_rownum(0),
  m_type(parquet::Type::type::UNDEFINED),
  m_row_grouop_id(0),
  m_col_id(col_id),
  m_end_of_stream(false),
  m_read_last_value(false)
  {
    m_parquet_reader = parquet_reader.get();
    m_row_group_reader = m_parquet_reader->RowGroup(m_row_grouop_id);
    m_ColumnReader = m_row_group_reader->Column(m_col_id);
  }

  parquet::Type::type column_reader_wrap::get_type()
  {//TODO if UNDEFINED 
    return m_parquet_reader->metadata()->schema()->Column(m_col_id)->physical_type();
  }

  bool column_reader_wrap::HasNext()//TODO template 
  {
    parquet::Int32Reader* int32_reader;
    parquet::Int64Reader* int64_reader;
    parquet::DoubleReader* double_reader;
    parquet::ByteArrayReader* byte_array_reader;

    switch (get_type())
    {
    case parquet::Type::type::INT32:
      int32_reader = static_cast<parquet::Int32Reader *>(m_ColumnReader.get());
      return int32_reader->HasNext();
      break;

    case parquet::Type::type::INT64:
      int64_reader = static_cast<parquet::Int64Reader *>(m_ColumnReader.get());
      return int64_reader->HasNext();
      break;

    case parquet::Type::type::DOUBLE:
      double_reader = static_cast<parquet::DoubleReader *>(m_ColumnReader.get());
      return double_reader->HasNext();
      break;

    case parquet::Type::type::BYTE_ARRAY:
      byte_array_reader = static_cast<parquet::ByteArrayReader *>(m_ColumnReader.get());
      return byte_array_reader->HasNext();
      break;

    default:
      return false;
      //TODO throw exception
    }

    return false;
  }

  int64_t column_reader_wrap::ReadBatch(int64_t batch_size, int16_t* def_levels, int16_t* rep_levels,
                            parquet_value_t* values, int64_t* values_read)
  {
    parquet::Int32Reader* int32_reader;
    parquet::Int64Reader* int64_reader;
    parquet::DoubleReader* double_reader;
    parquet::ByteArrayReader* byte_array_reader;

    parquet::ByteArray str_value;
    int64_t rows_read;
    int32_t i32_val;


    switch (get_type())
    {
    case parquet::Type::type::INT32:
      int32_reader = static_cast<parquet::Int32Reader *>(m_ColumnReader.get());
      rows_read = int32_reader->ReadBatch(1, nullptr, nullptr,&i32_val, values_read);
      values->num = i32_val;
      values->type = column_reader_wrap::parquet_type::INT32;
      break;

    case parquet::Type::type::INT64:
      int64_reader = static_cast<parquet::Int64Reader *>(m_ColumnReader.get());
      rows_read = int64_reader->ReadBatch(1, nullptr, nullptr, (int64_t *)&(values->num), values_read);
      values->type = column_reader_wrap::parquet_type::INT64;
      break;

    case parquet::Type::type::DOUBLE:
      double_reader = static_cast<parquet::DoubleReader *>(m_ColumnReader.get());
      rows_read = double_reader->ReadBatch(1, nullptr, nullptr, (double *)&(values->dbl), values_read);
      values->type = column_reader_wrap::parquet_type::DOUBLE;
      break;

    case parquet::Type::type::BYTE_ARRAY:
      byte_array_reader = static_cast<parquet::ByteArrayReader *>(m_ColumnReader.get());
      rows_read = byte_array_reader->ReadBatch(1, nullptr, nullptr, &str_value , values_read);
      values->str = (char*)str_value.ptr;
      values->str_len = str_value.len;
      values->type = column_reader_wrap::parquet_type::STRING;
      break;
    //TODO default; exception
    }

    return rows_read;
  }

  int64_t column_reader_wrap::Skip(int64_t rows_to_skip)
  {
    parquet::Int32Reader* int32_reader;
    parquet::Int64Reader* int64_reader;
    parquet::DoubleReader* double_reader;
    parquet::ByteArrayReader* byte_array_reader;

    parquet::ByteArray str_value;
    int64_t rows_read;
    int32_t i32_val;


    switch (get_type())
    {
    case parquet::Type::type::INT32:
      int32_reader = static_cast<parquet::Int32Reader *>(m_ColumnReader.get());
      rows_read = int32_reader->Skip(rows_to_skip);
      break;

    case parquet::Type::type::INT64:
      int64_reader = static_cast<parquet::Int64Reader *>(m_ColumnReader.get());
      rows_read = int64_reader->Skip(rows_to_skip);
      break;

    case parquet::Type::type::DOUBLE:
      double_reader = static_cast<parquet::DoubleReader *>(m_ColumnReader.get());
      rows_read = double_reader->Skip(rows_to_skip);
      break;

    case parquet::Type::type::BYTE_ARRAY:
      byte_array_reader = static_cast<parquet::ByteArrayReader *>(m_ColumnReader.get());
      rows_read = byte_array_reader->Skip(rows_to_skip);
      break;
    //TODO default; exception
    }

    return rows_read;
  }

  int column_reader_wrap::Read(const uint64_t rownum,parquet_value_t & value)
  {
    int64_t values_read = 0;
    int64_t rows_read = 0;

    if (m_rownum < (int64_t)rownum)
    { //should skip
      m_read_last_value = false;

      uint64_t skipped_rows = Skip(rownum - m_rownum -1);
      m_rownum += skipped_rows;

      while (((m_rownum+1) < (int64_t)rownum) || HasNext() == false)
      {
        uint64_t skipped_rows = Skip(rownum - m_rownum -1);
        m_rownum += skipped_rows;

        if (HasNext() == false)
        {
          if ((m_row_grouop_id + 1) >= m_parquet_reader->metadata()->num_row_groups())
          {
            m_end_of_stream = true;
            return -2; //end-of-stream
          }
          else
          {
            m_row_grouop_id++;
            m_row_group_reader = m_parquet_reader->RowGroup(m_row_grouop_id);
            m_ColumnReader = m_row_group_reader->Column(m_col_id);
          }
        }
      } //end-while

      rows_read = ReadBatch(1, nullptr, nullptr, &m_last_value, &values_read);
      m_read_last_value = true;
      m_rownum++;
      value = m_last_value;
    }
    else
    {
      if (m_read_last_value == false)
      {
        rows_read = ReadBatch(1, nullptr, nullptr, &m_last_value, &values_read);
        m_read_last_value = true;
        m_rownum++;
      }

      value = m_last_value;
    }

    //TODO rows_read == 1?
    //TODO values_read == 1?

    return 0;
  }


