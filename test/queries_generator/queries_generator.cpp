#include <iostream>
#include <fstream>
#include <vector>
#include <bits/stdc++.h>
#define TOTAL_COLUMN 9
#define NUM_COLUMN_INT 3
#define TIMESTAMP_COLUMN 9

using namespace std;

enum Return_type { INTEGER = 0,
            STRING = 1,
            TIMESTAMP = 2,
	    MIX_COL_NUM = 3,
	    COLUMN = 4,
	    NUMBER = 5};

auto random_arth_op = [](){std::string op="+-*/";return op[rand()%op.size()];};

auto random_compare_op = []()
{vector<string> op={">", "<", ">=", "<=", "==", "!="};
  return op[ rand() % op.size() ];
};

auto random_date_part = []()
{vector<string> op={"year", "month", "day", "hour", "minute", "second"};
  return op[ rand() % op.size() ];
};

auto random_date_part_extract = []()
{vector<string> op={"year", "month", "day", "hour", "minute", "second",
        "timezone_hour", "timezone_minute"};
  return op[ rand() % op.size() ];
};

string random_timestamp_string()
{
    auto year = [](){return rand()%100 + 1900;};
    auto month = [](){return 1 + rand()%12;};
    auto day = [](){return 1 + rand()%28;};
    auto hours = [](){return rand()%24;};
    auto minutes = [](){return rand()%60;};
    auto seconds = [](){return rand()%60;};
    auto fraction_sec = [](){return rand()%1000000;};
    stringstream timestamp_str;

    timestamp_str << year() << "-" << std::setw(2) << std::setfill('0') << month() << "-" << std::setw(2) << std::setfill('0') << day() << "T" <<std::setw(2) << std::setfill('0') << hours() << ":" << std::setw(2) << std::setfill('0') << minutes() << ":" << std::setw(2) << std::setfill('0') <<seconds() << "." << fraction_sec() << "Z";

    return timestamp_str.str();
}

string random_tm_format_string()
{
    auto random_format = []()
    {vector<string> op={"yyyyy ", "yyyy ", "yyy ", "yy ", "y ", "MMMMM ", "MMMM ", "MMM ", "MM ", "M ", "dd ", "d ", "a ", "hh ", "h ", "HH ", "H ", "mm ", "m ", "ss ", "s ", "SSSSSSSSS ", "SSSSSS ", "SSSSS ", "SSS ", "SS ", "S ", "n ", "X ", "XX ", "XXX ", "XXXX ", "XXXXX ", "x ", "xx ", "xxx ", "xxxx ", "xxxxx ", ": ", "- "};
     return op[ rand() % op.size() ];
    };
    int loop = rand() % 10;
    string frmt;
    while(loop)
    {
        frmt += random_format();
	loop--;
    }
    return frmt;
}

string random_col()
{
    int num = 1 + (rand() % NUM_COLUMN_INT);
    return "cast(_" + to_string(num) + " as int)";
}

string random_number()
{
    int num = rand() % 10 + 1;
    return to_string(num);
}

string random_num_expr(int depth)
{
    if (depth == 0)
    {
        return random_number();
    }

    return random_num_expr(depth-1) + random_arth_op() + random_num_expr(depth-1);
}

string random_num_col_expr(int depth)
{
    if (depth == 0)
    {
        if ((rand() % 2) == 0)
        {
            return random_col();
        }
        else
        {
            return random_number();
        }
    }

    return random_num_col_expr(depth-1) + random_arth_op() + random_num_col_expr(depth-1);
}

string random_query_expr(int depth, int type)
{
    string expr;
    if (depth == 0)
    {
        switch (type)
        {
            case INTEGER:
            case NUMBER:
                {
                    expr = random_number();
                    break;
                }
            case STRING:
                {
                    expr = "_" + to_string(1 + (rand() % (TOTAL_COLUMN)));
                    break;
                }
            case MIX_COL_NUM:
                {
                    expr = random_num_col_expr(depth);
                    break;
                }
            case TIMESTAMP:
                {
                    if ((rand() % 2) == 0)
                    {
                        expr = "to_timestamp(\'" + random_timestamp_string() + "\')";
                    }
                    else
                    {
                        expr = "to_timestamp(_" +  to_string(TIMESTAMP_COLUMN) + ")";
                    }
                    break;
                }
        }
        return expr;
    }

    int option;
    if (type == INTEGER)  //return type is int
    {
        switch (option = rand() % 9)
        {
            case 0:
                expr = "cast((avg(" + random_col() + random_arth_op() + random_num_col_expr(depth-1)
                        + ") " + random_arth_op() + " " + random_num_expr(depth-1) + ") as int)";
                break;
            case 1:
                expr = "count(" + random_col() + random_arth_op() + random_num_col_expr(depth-1)
                        + ") " + random_arth_op() + " " + random_num_expr(depth-1);
                break;
            case 2:
                expr = "max(" + random_col() + random_arth_op() + random_num_col_expr(depth-1) +
                        ") " + random_arth_op() + " " + random_num_expr(depth-1);
                break;
            case 3:
                expr = "min(" + random_col() + random_arth_op() + random_num_col_expr(depth-1) +
                        ") " + random_arth_op() + " " + random_num_expr(depth-1);
                break;
            case 4:
                expr = "sum(" + random_col() + random_arth_op() + random_num_col_expr(depth-1) +
                        ") " + random_arth_op() + " " + random_num_expr(depth-1);
                break;
	    case 5:
                expr = "char_length(" + random_query_expr(depth-1, STRING) + ")";
                break;
	    case 6:
                expr = "character_length(" + random_query_expr(depth-1, STRING) + ")";
                break;
            case 7:
                expr = "extract(" + random_date_part_extract() + " from " +
                        random_query_expr(depth-1, TIMESTAMP) + ")";
                break;
            case 8:
                expr = "date_diff(" + random_date_part() + ", " +
                        random_query_expr(depth-1, TIMESTAMP) + ", " +
                        random_query_expr(depth-1, TIMESTAMP) +  ")";
                break;
        }
    }
    else if (type == STRING)  // return type is string
    {
        switch (option = rand() % 4)
        {
            case 0:
                expr = "lower(" + random_query_expr(depth-1, STRING) + ")";
                break;
            case 1:
                expr = "upper(" + random_query_expr(depth-1, STRING) + ")";
                break;
            case 2:
                expr = "substring(" + random_query_expr(depth-1, STRING) + ", " +
                        random_query_expr(depth-1, NUMBER) + ", " +
                        random_query_expr(depth-1, NUMBER) + ")";
                break;
	    case 3:
                expr = "to_string(" + random_query_expr(depth-1, TIMESTAMP) + ", \'" + random_tm_format_string() + "\')";
                break;
        }
    }
    else if (type == TIMESTAMP)  // return type is TIMESTAMP
    {
        //switch (option = rand() % 2)
        //{
        //    case 0:
                expr = "date_add(" +  random_date_part()+ ", " + random_number() + ", " + random_query_expr(depth-1, TIMESTAMP) +  ")";
        //        break;
        //    case 1:  //for to_timestamp function
        //        expr = random_query_expr(0, TIMESTAMP);
        //        break;
        //}
    }
    else if (type == MIX_COL_NUM)
    {
        expr = random_num_col_expr(depth-1);
    }
    else if (type == COLUMN)  // return type integer column number
    {
        expr = random_col();
    }
    else if (type == NUMBER)  // return type random number
    {
        expr = random_number();
    }
    else
    {
        expr = "error";
    }
    return expr;
}

int main()
{
    srand(time(0));
    int reps, depth;
    fstream query_file;

    query_file.open("queries.txt", ios::out);
    cout << "Enter number of quries to be generated: ";
    cin >> reps;
    cout << "Enter depth of queries to be generated: ";
    cin >> depth;

    if(query_file.is_open()) //checking whether the file is open
    {
        while (reps)
        {
            int type;
            string query = "select ";
            // For multiple projections
            //int projection = rand() % 4;
            //while (projection > 1)
            //{
            //    type = rand() % 4;
            //    query = query + random_query_expr(depth, type) + ", ";
            //    projection--;
            //}
            type = rand() % 4;
            query = query + random_query_expr(depth, type) + " from s3object;";
            query_file << query << endl;
            reps--;
        }
        query_file.close();
    }
    return 0;
}

