#include "csvparser/csv.h"

namespace io{

    namespace error{
        struct escaped_char_missing :
                base,
                with_file_name,
                with_file_line{
                void format_error_message()const override{
                        std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                                "Escaped character missing in line %d in file \"%s\"."
                                , file_line, file_name);
                }
        };

	struct missmatch_of_begin_end :
		base,
                with_file_name,
                with_file_line{
		int begin=-1,end=-1;	
		void set_begin_end(int b,int e){
		  begin=b;
		  end=e;
		}

                void format_error_message()const override{
                      std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                      "***missmatch_of_begin_end*** Line number %d in file \"%s\" begin{%d} > end{%d}"
                      ,file_line, file_name,begin,end);
                }
    };

      struct missmatch_end :
	      base,
	      with_file_name,
	      with_file_line{
	      int end=-1;
	      int block_size=-1;
	      void set_end_block(int e,int b){	
		end = e;
		block_size = b;	
	      }
	      void format_error_message()const override{
                    std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                    "***missmatch_end*** Line number %d in file \"%s\" end{%d} block{%d}"
                    ,file_line, file_name, end, block_size);
             }
    };
    

    struct line_is_null :
	    base,
	    with_file_name,
	    with_file_line{
	    void format_error_message()const override{
                    std::snprintf(error_message_buffer, sizeof(error_message_buffer),
                    "***line is NULL*** Line number %d in file \"%s\"" 
                    ,file_line, file_name);
	    }
    };

    }

    namespace detail{
        static void unescape(char*&col_begin, char*&col_end, char& quote, char& escape_char)
        {
            if(col_end - col_begin >= 2)
            {
                while(*col_begin == quote && *(col_begin + 1) == quote)
                {
                    ++col_begin;
                    ++col_begin;
                }
                char*out = col_begin;
                char* in = col_begin;
                bool init = true;

                while(in != col_end)
                {
                    if(*in != quote && *in != escape_char)
                    {
                        if(init)
                        {
                            init = false;
                        }
                        else
                        {
                            *out = *in;
                        }
                        ++in;
                        ++out;
                    }
                    else
                    {
                        if(*in == escape_char)
                        {
                            ++in;
                            if(init)
                            {
                                ++col_begin;
                                ++out;
                                init = false;
                            }
                            else
                            {
                                *out = *in;
                            }
                            ++in;
                            ++out;
                        }
                        else
                        {
                            ++in;
                            while(*in != quote)
                            {
                                if(init)
                                {
                                    ++col_begin;
                                    ++out;
                                    init = false;
                                }
                                else
                                {
                                    *out = *in;
                                }
                                ++in;
                                ++out;
                            }
                            ++in;
                        }
                    }
                }
                *out = '\0';
                col_end = out;
            }
        }

        static void trim(char*&str_begin, char*&str_end, std::vector<char>& trim_chars)
        {
            while(str_begin != str_end && std::find(trim_chars.begin(), trim_chars.end(), *str_begin) != trim_chars.end())
                ++str_begin;
            while(str_begin != str_end && std::find(trim_chars.begin(), trim_chars.end(), *(str_end-1)) != trim_chars.end())
                --str_end;
            *str_end = '\0';
        }

        static const char*find_next_column_end(const char*col_begin, char& sep, char& quote, char& escape_char)
        {
            while(*col_begin != sep && *col_begin != '\0')
            {
                if(*col_begin != quote && *col_begin != escape_char)
                    ++col_begin;
                else
                {
                    if(*col_begin == escape_char)
                    {
                        if(*(col_begin+1) == '\0')
                            throw error::escaped_char_missing();
                        col_begin += 2;
                    }
                    else
                    {
                        do
                        {
                            ++col_begin;
                            while(*col_begin != quote)
                            {
                                if(*col_begin == '\0')
                                    throw error::escaped_string_not_closed();
                                ++col_begin;
                            }
                            ++col_begin;
                        }while(*col_begin == quote);
                    }
                }
            }
            return col_begin;
        }

        void chop_next_column(char*&line, char*&col_begin, char*&col_end, char& col_delimiter, char& quote, char& escape_char)
        {
            if(line == NULL)
	    {
	      io::error::line_is_null err;
	      throw err;
	    }

            col_begin = line;
            // the col_begin + (... - col_begin) removes the constness
            col_end = col_begin + (find_next_column_end(col_begin, col_delimiter, quote, escape_char) - col_begin);

            if(*col_end == '\0')
            {
                line = nullptr;
            }
            else
            {
                *col_end = '\0';
                 line = col_end + 1;
            }
        }

        void parse_line(char*line, std::vector<char*>& sorted_col, char& col_delimiter, char& quote, char& escape_char, std::vector<char>& trim_chars)
        {
            while (line != nullptr)
            {
                char*col_begin, *col_end;
                chop_next_column(line, col_begin, col_end, col_delimiter, quote, escape_char);
                if (!trim_chars.empty())
                    trim(col_begin, col_end, trim_chars);
                if (!(quote == '\0' && escape_char == '\0'))
                    unescape(col_begin, col_end, quote, escape_char);
                sorted_col.push_back(col_begin);
            }
        }


        bool empty_comment_line(char* line)
        {
            if(*line == '\0')
                return true;
            while(*line == ' ' || *line == '\t')
            {
                ++line;
                if(*line == '\0')
                    return true;
            }
	    return false;
        }

        bool single_line_comment(char start_char, std::vector<char>& comment_chars)
        {
            if(std::find(comment_chars.begin(), comment_chars.end(), start_char) != comment_chars.end())
                return true;
            else
                return false;
        }

        bool is_comment(char*&line, bool& comment_empty_line, std::vector<char>& comment_chars)
        {
            if(!comment_empty_line && comment_chars.empty())
                return false;
            else if(comment_empty_line && comment_chars.empty())
                return empty_comment_line(line);
            else if(!comment_empty_line && !comment_chars.empty())
                return single_line_comment(*line, comment_chars);
            else
                return empty_comment_line(line) || single_line_comment(*line, comment_chars);
        }

    }
}


class CSVParser
{
    private:
        char row_delimiter;
        char col_delimiter;
        char quote;
        char escape_char;
        bool comment_empty_line;
        std::vector<char> comment_characters;
        std::vector<char> trim_characters;

        static const int block_len = 1<<20;
        std::unique_ptr<char[]>buffer; // must be constructed before (and thus destructed after) the reader!
        #ifdef CSV_IO_NO_THREAD
        io::detail::SynchronousReader reader;
        #else
        io::detail::AsynchronousReader reader;
        #endif
        int data_begin;
        int data_end;

        char file_name[io::error::max_file_name_length+1];
        unsigned file_line;

        void init(std::unique_ptr<io::ByteSourceBase>byte_source)
        {
            file_line = 0;

            buffer = std::unique_ptr<char[]>(new char[3*block_len]);
            data_begin = 0;
            data_end = byte_source->read(buffer.get(), 2*block_len);

            // Ignore UTF-8 BOM
            if(data_end >= 3 && buffer[0] == '\xEF' && buffer[1] == '\xBB' && buffer[2] == '\xBF')
                data_begin = 3;

            if(data_end == 2*block_len){
                reader.init(std::move(byte_source));
                reader.start_read(buffer.get() + 2*block_len, block_len);
            }
        }

    public:
        CSVParser() = delete;
        CSVParser(const CSVParser&) = delete;
        CSVParser&operator=(const CSVParser&);

        CSVParser(const char*file_name, const char*data_begin, const char*data_end)
        {
            set_file_name(file_name);
            init(std::unique_ptr<io::ByteSourceBase>(new io::detail::NonOwningStringByteSource(data_begin, data_end-data_begin)));
        }

        CSVParser(const std::string&file_name, const char*data_begin, const char*data_end)
        {
            set_file_name(file_name.c_str());
            init(std::unique_ptr<io::ByteSourceBase>(new io::detail::NonOwningStringByteSource(data_begin, data_end-data_begin)));
        }

        void set_file_name(const std::string&file_name)
        {
            set_file_name(file_name.c_str());
        }

        void set_file_name(const char*file_name)
        {
            if(file_name != nullptr)
            {
                strncpy(this->file_name, file_name, sizeof(this->file_name));
                this->file_name[sizeof(this->file_name)-1] = '\0';
            }
            else
            {
                this->file_name[0] = '\0';
            }
        }

        const char*get_truncated_file_name()const
        {
            return file_name;
        }

        void set_file_line(unsigned file_line)
        {
            this->file_line = file_line;
        }

        unsigned get_file_line()const
        {
            return file_line;
        }

        void set_csv_def(char& row_delimit, char& col_delimit, char& quote_char, char& escp_char, bool& cmnt_empty_line, std::vector<char>& comment_chars , std::vector<char>& trim_chars)
        {
            row_delimiter = row_delimit;
	    col_delimiter = col_delimit;
	    quote = quote_char;
	    escape_char = escp_char;
	    comment_empty_line  = cmnt_empty_line;
	    comment_characters.assign(comment_chars.begin(), comment_chars.end());
	    trim_characters.assign(trim_chars.begin(), trim_chars.end());
        }

        char*next_line()
        {
            if(data_begin == data_end)
                return nullptr;

            ++file_line;

	    if(data_begin > data_end)
	    {
	      io::error::missmatch_of_begin_end err;
	      err.set_begin_end(data_begin,data_end);
	      throw err;
	    }
	    if(data_end > block_len*2)
	    {
	      io::error::missmatch_end err;
	      err.set_end_block(data_end,block_len*2);
	      throw err;
	    }

            if(data_begin >= block_len)
            {
                std::memcpy(buffer.get(), buffer.get()+block_len, block_len);
                data_begin -= block_len;
                data_end -= block_len;
                if(reader.is_valid())
                {
                    data_end += reader.finish_read();
                    std::memcpy(buffer.get()+block_len, buffer.get()+2*block_len, block_len);
                    reader.start_read(buffer.get() + 2*block_len, block_len);
                }
            }

            int line_end = data_begin;
            while(line_end != data_end && buffer[line_end] != row_delimiter)
            {
                if(buffer[line_end] == quote || buffer[line_end] == escape_char)
                {
                    if(buffer[line_end] == escape_char)
                    {
                        ++line_end;
                        if(line_end == data_end)
                        {
                            throw io::error::escaped_char_missing();
                        }
                        else if(buffer[line_end] == '\r' && buffer[line_end + 1] == '\n')  // handle windows \r\n-line breaks
                        {
                            ++line_end;
                        }
                    }
                    else
                    {
                        ++line_end;
                        while(buffer[line_end] != quote)
                        {
                            if(line_end == data_end)
                                throw io::error::escaped_string_not_closed();
                            ++line_end;
                        }
                    }
                }
                ++line_end;
            }

            if(line_end - data_begin + 1 > block_len)
            {
                io::error::line_length_limit_exceeded err;
                err.set_file_name(file_name);
                err.set_file_line(file_line);
                throw err;
            }

            if(line_end != data_end && buffer[line_end] == row_delimiter)
            {
                buffer[line_end] = '\0';
            }
            else
            {
                // some files are missing the newline at the end of the
                // last line
                ++data_end;
                buffer[line_end] = '\0';
            }

            // handle windows \r\n-line breaks
            if(row_delimiter == '\n')
            {
                if(line_end != data_begin && buffer[line_end-1] == '\r')
                    buffer[line_end-1] = '\0';
            }

            char*ret = buffer.get() + data_begin;
            data_begin = line_end+1;
            return ret;
        }

        bool read_row(std::vector<char*>& cols)
        {
            try{
                try{
                    char*line;
                    do{
                        line = next_line();
                        if(!line)
                            return false;
                    }while(io::detail::is_comment(line, comment_empty_line, comment_characters));

                    io::detail::parse_line(line, cols, col_delimiter, quote, escape_char, trim_characters);

                }catch(io::error::with_file_name&err){
                    err.set_file_name(get_truncated_file_name());
                    throw;
                }
            }catch(io::error::with_file_line&err){
                err.set_file_line(get_file_line());
                throw;
            }

            return true;
        }
};
