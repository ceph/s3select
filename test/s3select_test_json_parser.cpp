#include "s3select.h"
#include "s3select_json_parser.h"
#include <gtest/gtest.h>
#include <cassert>
#include <sstream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <iostream>
#include "s3select_oper.h"
#include <boost/algorithm/string/predicate.hpp>


// ===== base64 encode/decode

typedef unsigned char uchar;
static const std::string b = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";//=
static std::string base64_encode(const std::string &in) {
    std::string out;

    int val=0, valb=-6;
    for (uchar c : in) {
        val = (val<<8) + c;
        valb += 8;
        while (valb>=0) {
            out.push_back(b[(val>>valb)&0x3F]);
            valb-=6;
        }
    }
    if (valb>-6) out.push_back(b[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}


static std::string base64_decode(const std::string &in) {

    std::string out;

    std::vector<int> T(256,-1);
    for (int i=0; i<64; i++) T[b[i]] = i;

    int val=0, valb=-8;
    for (uchar c : in) {
        if (T[c] == -1) break;
        val = (val<<6) + T[c];
        valb += 6;
        if (valb>=0) {
            out.push_back(char((val>>valb)&0xFF));
            valb-=8;
        }
    }
    return out;
}

//=============================================

class dom_traverse_v2
{
	public:
		std::stringstream ss;
		void print(const rapidjson::Value &v, std::string);
		void traverse(rapidjson::Document &d);
		void traverse_object(const rapidjson::Value &v,std::string path);
		void traverse_array(const rapidjson::Value &v,std::string path);
};

void dom_traverse_v2::print(const rapidjson::Value &v, std::string key_name)
{
	ss << key_name << " : ";
	if(v.IsString())
	{
		ss << v.GetString() << std::endl;
	}
	else
		if(v.IsInt())
		{
				ss << v.GetInt() << std::endl;
		}
		else
			if(v.IsBool())
			{
				ss << (v.GetBool() ? "true" : "false" ) << std::endl;
			}
			else
				if(v.IsNull())
				{
					ss << "null" << std::endl;
				}
				else
					if(v.IsDouble())
					{
						ss << v.GetDouble() << std::endl;
					}
					else
					{
						ss << "value not exist" << std::endl;
					}

}

void dom_traverse_v2::traverse(rapidjson::Document &d)
{
	std::string path="";

	for (rapidjson::Value::ConstMemberIterator itr = d.MemberBegin(); itr != d.MemberEnd(); ++itr)
	{
		const rapidjson::Value &v = itr->value;

		if(v.IsArray())
		{
			std::string path="";
			path.append( itr->name.GetString() );
			path.append( "/" );

			traverse_array(v, path);
		}
		else if (v.IsObject())
		{
			std::string path="";
			path.append( itr->name.GetString() );
			path.append( "/" );

			traverse_object(v, path);
		}
		else
		{
			std::string tmp = path;
			path.append( itr->name.GetString() );
			path.append( "/" );
			print(v, path);
			path = tmp;
		}

	}
}

void dom_traverse_v2::traverse_array(const rapidjson::Value &v,std::string path)
{
	std::string object_key = path;

	for (rapidjson::Value::ConstValueIterator itr = v.Begin(); itr != v.End(); ++itr)
	{
		const rapidjson::Value& array_item = *itr;
		if(array_item.IsArray())
		{
			traverse_array(array_item,object_key);
		}
		else if(array_item.IsObject())
		{
			traverse_object(array_item,object_key);
		}
		else
		{
			print(array_item, object_key);
		}
	}
}

void dom_traverse_v2::traverse_object(const rapidjson::Value &v,std::string path)
{
	std::string object_key = path;

	for (rapidjson::Value::ConstMemberIterator itr = v.MemberBegin(); itr != v.MemberEnd(); ++itr)
	{
		const rapidjson::Value& v_itr = itr->value;
		if (itr->value.IsObject())
		{
			std::string tmp = object_key;
			object_key.append( itr->name.GetString() );
			object_key.append("/");
			traverse_object(v_itr,object_key);
			object_key = tmp;
		}
		else
			if (itr->value.IsArray())
			{
				object_key.append( itr->name.GetString() );
				object_key.append("/");
				traverse_array(v_itr,object_key);
			}
			else
			{
				std::string tmp = object_key;
				object_key.append( itr->name.GetString() );
				object_key.append("/");
				print(v_itr, object_key);
				object_key = tmp;
			}
	}
}


std::string parse_json_dom(const char* file_name)
{//purpose: for testing only. dom vs sax.

	std::string final_result;
	const char* dom_input_file_name = file_name;
	std::fstream dom_input_file(dom_input_file_name, std::ios::in | std::ios::binary);
	dom_input_file.seekg(0, std::ios::end);

	// get file size
	auto sz = dom_input_file.tellg();
	// place the position at the begining
	dom_input_file.seekg(0, std::ios::beg);
	//read whole file content into allocated buffer
	std::string file_content(sz, '\0');
	dom_input_file.read((char*)file_content.data(),sz);

	rapidjson::Document document;
	document.Parse(file_content.data());

	if (document.HasParseError()) {
		std::cout<<"parsing error"<< std::endl;
		return "parsing error";
	}

	if (!document.IsObject())
	{
		std::cout << " input is not an object " << std::endl;
		return "object error";
	}

	dom_traverse_v2 td2;
	td2.traverse( document );
	final_result = (td2.ss).str();
	return final_result;
}


int RGW_send_data(const char* object_name, std::string & result)
{//purpose: simulate RGW streaming an object into s3select

	std::ifstream input_file_stream;
	JsonParserHandler handler;
	size_t buff_sz{1024*1024*4};
	char* buff = (char*)malloc(buff_sz);
	std::function<int(std::pair < std::string, s3selectEngine::value>)> fp;
	size_t no_of = 0;

	try {
		input_file_stream = std::ifstream(object_name, std::ios::in | std::ios::binary);
	}
	catch( ... ){
		std::cout << "failed to open file " << std::endl;  
		exit(-1);
	}

	//read first chunk;
	auto read_size = input_file_stream.readsome(buff, buff_sz);
	while(read_size)
	{
		//the handler is processing any buffer size
		std::cout << "processing buffer " << no_of++ << " size " << buff_sz << std::endl;
		int status = handler.process_json_buffer(buff, read_size);
		if(status<0) return -1;

		//read next chunk
		read_size = input_file_stream.readsome(buff, buff_sz);
	}
	handler.process_json_buffer(0, 0, true);

	free(buff);
	//result = handler.get_full_result();
	return 0;
}

int test_compare(int argc, char* argv[])
{
	std::string res;
	std::ofstream o1,o2;

	RGW_send_data(argv[1],res);
	std::string res2 = parse_json_dom(argv[1]);
	o1.open(std::string(argv[1]).append(".sax.out"));
	o2.open(std::string(argv[1]).append(".dom.out"));

	o1 << res;
	o2 << res2;

	o1.close();
	o2.close();

	return 0;
}

std::string json2 = R"({
"row" : [
	{
		"color": "red",
		"value": "#f00"
	},
	{
		"color": "green",
		"value": "#0f0"
	},
	{
		"color": "blue",
		"value": "#00f"
	},
	{
		"color": "cyan",
		"value": "#0ff"
	},
	{
		"color": "magenta",
		"value": "#f0f"
	},
	{
		"color": "yellow",
		"value": "#ff0"
	},
	{
		"color": "black",
		"value": "#000"
	}
]
}
)";


#define TEST2 \
"ewoicm93IiA6IFsKCXsKCQkiY29sb3IiOiAicmVkIiwKCQkidmFsdWUiOiAiI2YwMCIKCX0sCgl7\
CgkJImNvbG9yIjogImdyZWVuIiwKCQkidmFsdWUiOiAiIzBmMCIKCX0sCgl7CgkJImNvbG9yIjog\
ImJsdWUiLAoJCSJ2YWx1ZSI6ICIjMDBmIgoJfSwKCXsKCQkiY29sb3IiOiAiY3lhbiIsCgkJInZh\
bHVlIjogIiMwZmYiCgl9LAoJewoJCSJjb2xvciI6ICJtYWdlbnRhIiwKCQkidmFsdWUiOiAiI2Yw\
ZiIKCX0sCgl7CgkJImNvbG9yIjogInllbGxvdyIsCgkJInZhbHVlIjogIiNmZjAiCgl9LAoJewoJ\
CSJjb2xvciI6ICJibGFjayIsCgkJInZhbHVlIjogIiMwMDAiCgl9Cl0KfQo="

#define TEST3 \
"ewogICJoZWxsbyI6ICJ3b3JsZCIsCiAgICAidCI6ICJ0cnVlIiAsCiAgICAiZiI6ICJmYWxzZSIs\
CiAgICAibiI6ICJudWxsIiwKICAgICJpIjogMTIzLAogICAgInBpIjogMy4xNDE2LAoKICAgICJu\
ZXN0ZWRfb2JqIiA6IHsKICAgICAgImhlbGxvMiI6ICJ3b3JsZCIsCiAgICAgICJ0MiI6IHRydWUs\
CiAgICAgICJuZXN0ZWQyIiA6IHsKICAgICAgICAiYzEiIDogImMxX3ZhbHVlIiAsCiAgICAgICAg\
ImFycmF5X25lc3RlZDIiOiBbMTAsIDIwLCAzMCwgNDBdCiAgICAgIH0sCiAgICAgICJuZXN0ZWQz\
IiA6ewogICAgICAgICJoZWxsbzMiOiAid29ybGQiLAogICAgICAgICJ0MiI6IHRydWUsCiAgICAg\
ICAgIm5lc3RlZDQiIDogewogICAgICAgICAgImMxIiA6ICJjMV92YWx1ZSIgLAogICAgICAgICAg\
ImFycmF5X25lc3RlZDMiOiBbMTAwLCAyMDAsIDMwMCwgNDAwXQogICAgICAgIH0KICAgICAgfQog\
ICAgfSwKICAgICJhcnJheV8xIjogWzEsIDIsIDMsIDRdCn0K"

#define TEST4 \
"ewoKICAgICJnbG9zc2FyeSI6IHsKICAgICAgICAidGl0bGUiOiAiZXhhbXBsZSBnbG9zc2FyeSIs\
CgkJIkdsb3NzRGl2IjogewogICAgICAgICAgICAidGl0bGUiOiAiUyIsCgkJCSJHbG9zc0xpc3Qi\
OiB7CiAgICAgICAgICAgICAgICAiR2xvc3NFbnRyeSI6IHsKICAgICAgICAgICAgICAgICAgICAi\
SUQiOiAiU0dNTCIsCgkJCQkJIlNvcnRBcyI6ICJTR01MIiwKCQkJCQkiR2xvc3NUZXJtIjogIlN0\
YW5kYXJkIEdlbmVyYWxpemVkIE1hcmt1cCBMYW5ndWFnZSIsCgkJCQkJIkFjcm9ueW0iOiAiU0dN\
TCIsCgkJCQkJIkFiYnJldiI6ICJJU08gODg3OToxOTg2IiwKCQkJCQkiR2xvc3NEZWYiOiB7CiAg\
ICAgICAgICAgICAgICAgICAgICAgICJwYXJhIjogIkEgbWV0YS1tYXJrdXAgbGFuZ3VhZ2UsIHVz\
ZWQgdG8gY3JlYXRlIG1hcmt1cCBsYW5ndWFnZXMgc3VjaCBhcyBEb2NCb29rLiIsCgkJCQkJCSJH\
bG9zc1NlZUFsc28iOiBbIkdNTCIsICJYTUwiXQogICAgICAgICAgICAgICAgICAgIH0sCgkJCQkJ\
Ikdsb3NzU2VlIjogIm1hcmt1cCIKICAgICAgICAgICAgICAgIH0KICAgICAgICAgICAgfQogICAg\
ICAgIH0KICAgIH0KfQoK"

#define TEST5 \
"ewoKICAgICJnbG9zc2FyeSI6IHsKICAgICAgICAidGl0bGUiOiAiZXhhbXBsZSBnbG9zc2FyeSIsCiAgICAgICAgICAgICAgICAiR2xvc3NEaXYiOiB7CiAgICAgICAgICAgICJ0aXRsZSI6ICJTIi\
wKICAgICAgICAgICAgICAgICAgICAgICAgIkdsb3NzTGlzdCI6IHsKICAgICAgICAgICAgICAgICJHbG9zc0VudHJ5IjogewogICAgICAgICAgICAgICAgICAgICJJRCI6ICJTR01MIiwKICAgICAgI\
CAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICJTb3J0QXMiOiAiU0dNTCIsCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAiR2xvc3NUZXJtIjogIlN0YW5kYXJk\
IEdlbmVyYWxpemVkIE1hcmt1cCBMYW5ndWFnZSIsCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAiQWNyb255bSI6ICJTR01MIiwKICAgICAgICAgICAgICAgICAgICAgIC\
AgICAgICAgICAgICAgICAgICJBYmJyZXYiOiAiSVNPIDg4Nzk6MTk4NiIsCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAiR2xvc3NEZWYiOiB7CiAgICAgICAgICAgICAg\
ICAgICAgICAgICJwYXJhIjogIkEgbWV0YS1tYXJrdXAgbGFuZ3VhZ2UsIHVzZWQgdG8gY3JlYXRlIG1hcmt1cCBsYW5ndWFnZXMgc3VjaCBhcyBEb2NCb29rLiIsCiAgICAgICAgICAgICAgICAgIC\
AgICAgICAgICAgICAgICAgICAgICAgICAgICAgICJHbG9zc1NlZUFsc28iOiBbIkdNTCIsICJYTUwiXSwKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgInBv\
c3RhcnJheSI6IHsKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICJhIjoxMTEsCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIC\
AgICAgICAgICAgICAgICAgICAgICAgICAiYiI6MjIyCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIH0KICAgICAgICAgICAgICAgICAgICB9LAogICAgICAg\
ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIkdsb3NzU2VlIjogIm1hcmt1cCIKICAgICAgICAgICAgICAgIH0KICAgICAgICAgICAgfQogICAgICAgIH0KICAgIH0KfQo="

#define TEST6 \
"ewoicm9vdCIgOiBbCnsKCiAgICAiZ2xvc3NhcnkiOiB7CiAgICAgICAgInRpdGxlIjogImV4YW1w\
bGUgZ2xvc3NhcnkiLAoJCSJHbG9zc0RpdiI6IHsKICAgICAgICAgICAgInRpdGxlIjogIlMiLAoJ\
CQkiR2xvc3NMaXN0IjogewogICAgICAgICAgICAgICAgIkdsb3NzRW50cnkiOiB7CiAgICAgICAg\
ICAgICAgICAgICAgIklEIjogIlNHTUwiLAoJCQkJCSJTb3J0QXMiOiAiU0dNTCIsCgkJCQkJIkds\
b3NzVGVybSI6ICJTdGFuZGFyZCBHZW5lcmFsaXplZCBNYXJrdXAgTGFuZ3VhZ2UiLAoJCQkJCSJB\
Y3JvbnltIjogIlNHTUwiLAoJCQkJCSJBYmJyZXYiOiAiSVNPIDg4Nzk6MTk4NiIsCgkJCQkJIkds\
b3NzRGVmIjogewogICAgICAgICAgICAgICAgICAgICAgICAicGFyYSI6ICJBIG1ldGEtbWFya3Vw\
IGxhbmd1YWdlLCB1c2VkIHRvIGNyZWF0ZSBtYXJrdXAgbGFuZ3VhZ2VzIHN1Y2ggYXMgRG9jQm9v\
ay4iLAoJCQkJCQkiR2xvc3NTZWVBbHNvIjogWyJHTUwiLCAiWE1MIl0sCgkJCQkJCSJwb3N0YXJy\
YXkiOiB7CgkJCQkJCQkgICJhIjoxMTEsCgkJCQkJCQkgICJiIjoyMjIKCQkJCQkJfQogICAgICAg\
ICAgICAgICAgICAgIH0sCgkJCQkJIkdsb3NzU2VlIjogIm1hcmt1cCIKICAgICAgICAgICAgICAg\
IH0sCiAgICAgICAgICAgICAgICAiR2xvc3NFbnRyeSI6IAoJCXsKICAgICAgICAgICAgICAgICAg\
ICAiSUQiOiAiU0dNTCIsCgkJCQkJIlNvcnRBcyI6ICJTR01MIiwKCQkJCQkiR2xvc3NUZXJtIjog\
IlN0YW5kYXJkIEdlbmVyYWxpemVkIE1hcmt1cCBMYW5ndWFnZSIsCgkJCQkJIkFjcm9ueW0iOiAi\
U0dNTCIsCgkJCQkJIkFiYnJldiI6ICJJU08gODg3OToxOTg2IiwKCQkJCQkiR2xvc3NEZWYiOiB7\
CiAgICAgICAgICAgICAgICAgICAgICAgICJwYXJhIjogIkEgbWV0YS1tYXJrdXAgbGFuZ3VhZ2Us\
IHVzZWQgdG8gY3JlYXRlIG1hcmt1cCBsYW5ndWFnZXMgc3VjaCBhcyBEb2NCb29rLiIsCgkJCQkJ\
CSJHbG9zc1NlZUFsc28iOiBbIkdNTCIsICJYTUwiXSwKCQkJCQkJInBvc3RhcnJheSI6IHsKCQkJ\
CQkJCSAgImEiOjExMSwKCQkJCQkJCSAgImIiOjIyMgoJCQkJCQl9CiAgICAgICAgICAgICAgICAg\
ICAgfSwKCQkJCQkiR2xvc3NTZWUiOiAibWFya3VwIgogICAgICAgICAgICAgICAgfQogICAgICAg\
ICAgICB9CiAgICAgICAgfQogICAgfQp9CiwKewoKICAgICJnbG9zc2FyeSI6IHsKICAgICAgICAi\
dGl0bGUiOiAiZXhhbXBsZSBnbG9zc2FyeSIsCgkJIkdsb3NzRGl2IjogewogICAgICAgICAgICAi\
dGl0bGUiOiAiUyIsCgkJCSJHbG9zc0xpc3QiOiB7CiAgICAgICAgICAgICAgICAiR2xvc3NFbnRy\
eSI6IHsKICAgICAgICAgICAgICAgICAgICAiSUQiOiAiU0dNTCIsCgkJCQkJIlNvcnRBcyI6ICJT\
R01MIiwKCQkJCQkiR2xvc3NUZXJtIjogIlN0YW5kYXJkIEdlbmVyYWxpemVkIE1hcmt1cCBMYW5n\
dWFnZSIsCgkJCQkJIkFjcm9ueW0iOiAiU0dNTCIsCgkJCQkJIkFiYnJldiI6ICJJU08gODg3OTox\
OTg2IiwKCQkJCQkiR2xvc3NEZWYiOiB7CiAgICAgICAgICAgICAgICAgICAgICAgICJwYXJhIjog\
IkEgbWV0YS1tYXJrdXAgbGFuZ3VhZ2UsIHVzZWQgdG8gY3JlYXRlIG1hcmt1cCBsYW5ndWFnZXMg\
c3VjaCBhcyBEb2NCb29rLiIsCgkJCQkJCSJHbG9zc1NlZUFsc28iOiBbIkdNTCIsICJYTUwiXQog\
ICAgICAgICAgICAgICAgICAgIH0sCgkJCQkJIkdsb3NzU2VlIjogIm1hcmt1cCIKICAgICAgICAg\
ICAgICAgIH0KICAgICAgICAgICAgfQogICAgICAgIH0KICAgIH0KfQpdCn0K"

#define TEST7 \
"ewogICJsZXZlbDEiIDogewogICAgImxldmVsMiIgOiB7CiAgICAgICJsZXZlbDMiIDogewoJImxldmVsNCIgOiAidmFsdWU0IgogICAgICB9CiAgICB9CiAgfSwKICAgI\
CJsZXZlbDFfMiIgOnsKICAgICAgImxldmVsMiIgOiB7CgkibGV2ZWwzIiA6IHsKCSAgImxldmVsNCIgOiAidmFsdWU0XzIiCgl9CiAgICAgIH0KICAgIH0KfSAK"

#define TEST8 \
"ewogICAiZmlyc3ROYW1lIjogIkpvZSIsCiAgICJsYXN0TmFtZSI6ICJKYWNrc29uIiwKICAgImdlbmRlciI6ICJtYWxlIiwKICAgImFnZSI6ICJ0d2VudHkiLAogICAiYWRkcmV\
zcyI6IHsKICAgICAgICJzdHJlZXRBZGRyZXNzIjogIjEwMSIsCiAgICAgICAiY2l0eSI6ICJTYW4gRGllZ28iLAogICAgICAgInN0YXRlIjogIkNBIgogICB9LAogICAicGhvbm\
VOdW1iZXJzIjogWwogICAgICAgeyAidHlwZSI6ICJob21lIiwgIm51bWJlciI6ICI3MzQ5MjgyMzgyIiB9CiAgIF0KfQo="

#define TEST9 \
"WwogIHsKICAgICJfaWQiOiAiNjIwYjUyNzFiMzkyYjc0NTYyZTM4NzAwIiwKICAgICJpbmRleCI6IDAsCiAgICAiZ3VpZCI6ICI0YzgyNzk0Ni0yNDJlLTQzYTctODcxNy03MmJiMmFmM2ZkZTIiLAogI\
CAgImlzQWN0aXZlIjogdHJ1ZSwKICAgICJiYWxhbmNlIjogIiQzLDA1Ny41MyIsCiAgICAicGljdHVyZSI6ICJodHRwOi8vcGxhY2Vob2xkLml0LzMyeDMyIiwKICAgICJhZ2UiOiAyMCwKICAgICJleW\
VDb2xvciI6ICJibHVlIiwKICAgICJuYW1lIjogIk1vbnRnb21lcnkgR3JlZW5lIiwKICAgICJnZW5kZXIiOiAibWFsZSIsCiAgICAiY29tcGFueSI6ICJWRU5EQkxFTkQiLAogICAgImVtYWlsIjogIm1\
vbnRnb21lcnlncmVlbmVAdmVuZGJsZW5kLmNvbSIsCiAgICAicGhvbmUiOiAiKzEgKDg5NCkgNTgyLTI1MzAiLAogICAgImFkZHJlc3MiOiAiNzAzIEJheXZpZXcgQXZlbnVlLCBDYXJyc3ZpbGxlLCBW\
aXJnaW4gSXNsYW5kcywgMjYyMiIsCiAgICAiYWJvdXQiOiAiQXV0ZSB1bGxhbWNvIGV4Y2VwdGV1ciBsYWJvcnVtIG1pbmltIGFuaW0gcXVpcyBhdXRlIGFkLiBFc3NlIG5vbiBlc3NlIGlydXJlIGFkI\
HNpbnQgZXQgdWxsYW1jbyB0ZW1wb3IgcXVpIGN1bHBhIGNvbnNlcXVhdCBleGVyY2l0YXRpb24gTG9yZW0gdWxsYW1jby4gUHJvaWRlbnQgYW5pbSBlbGl0IGV0IG51bGxhIGN1cGlkYXRhdCBlc3NlLi\
BWZWxpdCBleGNlcHRldXIgYWxpcXVpcCBldCByZXByZWhlbmRlcml0IHF1aXMgY3VscGEgcHJvaWRlbnQgbGFib3J1bSBlc3NlIHVsbGFtY28gZWEgZWxpdCBub24uIE5vc3RydWQgaWQgbGFib3JpcyB\
tYWduYSBpbmNpZGlkdW50IHV0IHRlbXBvciBjdXBpZGF0YXQgZWxpdCBleGNlcHRldXIgaW4gc2l0IGxhYm9ydW0uIElydXJlIHZlbmlhbSBlc3NlIGF1dGUgYWRpcGlzaWNpbmcgZWxpdCBlc3NlLiBU\
ZW1wb3Igbm9uIHVsbGFtY28gZXhjZXB0ZXVyIGN1cGlkYXRhdCByZXByZWhlbmRlcml0IHJlcHJlaGVuZGVyaXQgaWQgY29tbW9kbyBkdWlzIHVsbGFtY28gc2ludCBpbmNpZGlkdW50IGluIHZlbGl0L\
lxyXG4iLAogICAgInJlZ2lzdGVyZWQiOiAiMjAxNi0wOC0yM1QwODozMTowOCAtMDY6LTMwIiwKICAgICJsYXRpdHVkZSI6IC0xNS4zOTU4ODUsCiAgICAibG9uZ2l0dWRlIjogLTYuNzMwMDE3LAogIC\
AgInRhZ3MiOiBbCiAgICAgICJlaXVzbW9kIiwKICAgICAgImFsaXF1YSIsCiAgICAgICJpcHN1bSIsCiAgICAgICJpcnVyZSIsCiAgICAgICJlbGl0IiwKICAgICAgInF1aXMiLAogICAgICAic2l0Igo\
gICAgXSwKICAgICJmcmllbmRzIjogWwogICAgICB7CiAgICAgICAgImlkIjogMCwKICAgICAgICAibmFtZSI6ICJLYW5lIENoZW4iCiAgICAgIH0sCiAgICAgIHsKICAgICAgICAiaWQiOiAxLAogICAg\
ICAgICJuYW1lIjogIkRpYW5uYSBMYXdyZW5jZSIKICAgICAgfSwKICAgICAgewogICAgICAgICJpZCI6IDIsCiAgICAgICAgIm5hbWUiOiAiTGVpbGEgSnVhcmV6IgogICAgICB9CiAgICBdLAogICAgI\
mdyZWV0aW5nIjogIkhlbGxvLCBNb250Z29tZXJ5IEdyZWVuZSEgWW91IGhhdmUgNSB1bnJlYWQgbWVzc2FnZXMuIiwKICAgICJmYXZvcml0ZUZydWl0IjogImJhbmFuYSIKICB9LAogIHsKICAgICJfaW\
QiOiAiNjIwYjUyNzE4N2IyOTUyOTAxMDU1MTY5IiwKICAgICJpbmRleCI6IDEsCiAgICAiZ3VpZCI6ICIzZjQ3Nzk4MC03MzAwLTRmODktYTJiMS01ZTQ2N2QxMjc4ZWUiLAogICAgImlzQWN0aXZlIjo\
gZmFsc2UsCiAgICAiYmFsYW5jZSI6ICIkMiwxNTYuODgiLAogICAgInBpY3R1cmUiOiAiaHR0cDovL3BsYWNlaG9sZC5pdC8zMngzMiIsCiAgICAiYWdlIjogMzAsCiAgICAiZXllQ29sb3IiOiAiYnJv\
d24iLAogICAgIm5hbWUiOiAiU3Rld2FydCBDYWluIiwKICAgICJnZW5kZXIiOiAibWFsZSIsCiAgICAiY29tcGFueSI6ICJYU1BPUlRTIiwKICAgICJlbWFpbCI6ICJzdGV3YXJ0Y2FpbkB4c3BvcnRzL\
mNvbSIsCiAgICAicGhvbmUiOiAiKzEgKDgyNSkgNTk5LTI4NDUiLAogICAgImFkZHJlc3MiOiAiNzAzIEhpZ2hsYW5kIEF2ZW51ZSwgQmVsZmFpciwgSGF3YWlpLCAyMTciLAogICAgImFib3V0IjogIk\
N1bHBhIG1vbGxpdCB1bGxhbWNvIGFkIGV4ZXJjaXRhdGlvbi4gU2ludCBtb2xsaXQgaW4gaW4gYWQgbWluaW0gbW9sbGl0IGN1bHBhIG5pc2kuIFJlcHJlaGVuZGVyaXQgYWxpcXVhIGRvIHNpdCBuaXN\
pIGFtZXQgZXNzZSBhZCBjb25zZWN0ZXR1ciBudWxsYSBhdXRlIGlkIGFsaXF1YSBtYWduYS5cclxuIiwKICAgICJyZWdpc3RlcmVkIjogIjIwMTctMDMtMjBUMTA6Mjg6MjQgLTA2Oi0zMCIsCiAgICAi\
bGF0aXR1ZGUiOiA1OC40NzU4OTIsCiAgICAibG9uZ2l0dWRlIjogMTQxLjM1NjkzNSwKICAgICJ0YWdzIjogWwogICAgICAicGFyaWF0dXIiLAogICAgICAiZHVpcyIsCiAgICAgICJsYWJvcmlzIiwKI\
CAgICAgIm1vbGxpdCIsCiAgICAgICJpcnVyZSIsCiAgICAgICJlaXVzbW9kIiwKICAgICAgInNpbnQiCiAgICBdLAogICAgImZyaWVuZHMiOiBbCiAgICAgIHsKICAgICAgICAiaWQiOiAwLAogICAgIC\
AgICJuYW1lIjogIk5lYWwgTG9wZXoiCiAgICAgIH0sCiAgICAgIHsKICAgICAgICAiaWQiOiAxLAogICAgICAgICJuYW1lIjogIlRpZmZhbnkgQ29jaHJhbiIKICAgICAgfSwKICAgICAgewogICAgICA\
gICJpZCI6IDIsCiAgICAgICAgIm5hbWUiOiAiU3RldmVucyBEYXZlbnBvcnQiCiAgICAgIH0KICAgIF0sCiAgICAiZ3JlZXRpbmciOiAiSGVsbG8sIFN0ZXdhcnQgQ2FpbiEgWW91IGhhdmUgMTAgdW5y\
ZWFkIG1lc3NhZ2VzLiIsCiAgICAiZmF2b3JpdGVGcnVpdCI6ICJhcHBsZSIKICB9LAogIHsKICAgICJfaWQiOiAiNjIwYjUyNzFmZTk4MDViODE1ZmI4NzBiIiwKICAgICJpbmRleCI6IDIsCiAgICAiZ\
3VpZCI6ICIxYTFjY2FiNi0xMDU5LTRmY2MtOTJmMy0yNDhkNzgwZTA4YmIiLAogICAgImlzQWN0aXZlIjogdHJ1ZSwKICAgICJiYWxhbmNlIjogIiQyLDgyNy4xNSIsCiAgICAicGljdHVyZSI6ICJodH\
RwOi8vcGxhY2Vob2xkLml0LzMyeDMyIiwKICAgICJhZ2UiOiAyNSwKICAgICJleWVDb2xvciI6ICJicm93biIsCiAgICAibmFtZSI6ICJEYXZpZHNvbiBQcmluY2UiLAogICAgImdlbmRlciI6ICJtYWx\
lIiwKICAgICJjb21wYW55IjogIk1JVFJPQyIsCiAgICAiZW1haWwiOiAiZGF2aWRzb25wcmluY2VAbWl0cm9jLmNvbSIsCiAgICAicGhvbmUiOiAiKzEgKDgzNCkgNTAxLTIxNjciLAogICAgImFkZHJl\
c3MiOiAiMjUxIFBvcnRsYW5kIEF2ZW51ZSwgRm9zdG9yaWEsIE1pbm5lc290YSwgOTE3OSIsCiAgICAiYWJvdXQiOiAiVWxsYW1jbyBtb2xsaXQgYW5pbSBkb2xvcmUgbGFib3JpcyBjdXBpZGF0YXQuI\
EFsaXF1aXAgbm9uIGRvbG9yIGRvbG9yZSB2ZWxpdCBhbGlxdWlwIGNvbnNlY3RldHVyLiBOb24gY3VscGEgbm9uIGF1dGUgZXNzZSB2b2x1cHRhdGUgZWxpdCBlc3NlIGNvbnNlY3RldHVyIHNpdCBhZC\
Bjb25zZXF1YXQuIERlc2VydW50IGlwc3VtIG5pc2kgYWxpcXVhIGFtZXQgbm9uIGxhYm9yaXMgY2lsbHVtIHJlcHJlaGVuZGVyaXQgTG9yZW0gbGFib3J1bSBjb21tb2RvIHVsbGFtY28gbGFib3J1bS5\
cclxuIiwKICAgICJyZWdpc3RlcmVkIjogIjIwMjAtMDctMDFUMTA6MDg6MTMgLTA2Oi0zMCIsCiAgICAibGF0aXR1ZGUiOiA0OC40ODMzMjIsCiAgICAibG9uZ2l0dWRlIjogMTUzLjcyMzU3NCwKICAg\
ICJ0YWdzIjogWwogICAgICAiaXJ1cmUiLAogICAgICAib2NjYWVjYXQiLAogICAgICAiZG9sb3JlIiwKICAgICAgInRlbXBvciIsCiAgICAgICJtb2xsaXQiLAogICAgICAiZXN0IiwKICAgICAgImxhY\
m9yaXMiCiAgICBdLAogICAgImZyaWVuZHMiOiBbCiAgICAgIHsKICAgICAgICAiaWQiOiAwLAogICAgICAgICJuYW1lIjogIkx1Y3kgQ29ucmFkIgogICAgICB9LAogICAgICB7CiAgICAgICAgImlkIj\
ogMSwKICAgICAgICAibmFtZSI6ICJDdXJ0aXMgVHlsZXIiCiAgICAgIH0sCiAgICAgIHsKICAgICAgICAiaWQiOiAyLAogICAgICAgICJuYW1lIjogIlRhcmEgVGFsbGV5IgogICAgICB9CiAgICBdLAo\
gICAgImdyZWV0aW5nIjogIkhlbGxvLCBEYXZpZHNvbiBQcmluY2UhIFlvdSBoYXZlIDMgdW5yZWFkIG1lc3NhZ2VzLiIsCiAgICAiZmF2b3JpdGVGcnVpdCI6ICJzdHJhd2JlcnJ5IgogIH0sCiAgewog\
ICAgIl9pZCI6ICI2MjBiNTI3MTc5MDBiMzRhZWI4OTkwNTEiLAogICAgImluZGV4IjogMywKICAgICJndWlkIjogIjhkMTFjMjljLWNmYWItNDEwNS1hYmY0LWM3YjQ1NzZlYjg5YiIsCiAgICAiaXNBY\
3RpdmUiOiBmYWxzZSwKICAgICJiYWxhbmNlIjogIiQxLDg2MS4wMiIsCiAgICAicGljdHVyZSI6ICJodHRwOi8vcGxhY2Vob2xkLml0LzMyeDMyIiwKICAgICJhZ2UiOiAyOCwKICAgICJleWVDb2xvci\
I6ICJncmVlbiIsCiAgICAibmFtZSI6ICJQZXJyeSBDbGFya2UiLAogICAgImdlbmRlciI6ICJtYWxlIiwKICAgICJjb21wYW55IjogIlpJTExBRFlORSIsCiAgICAiZW1haWwiOiAicGVycnljbGFya2V\
AemlsbGFkeW5lLmNvbSIsCiAgICAicGhvbmUiOiAiKzEgKDg4NykgNDM5LTM3NDMiLAogICAgImFkZHJlc3MiOiAiNTk3IFRoYW1lcyBTdHJlZXQsIEJsZW5kZSwgR2VvcmdpYSwgODIxMiIsCiAgICAi\
YWJvdXQiOiAiSW5jaWRpZHVudCB0ZW1wb3IgbWluaW0gYWxpcXVhIGRvbG9yZSBvZmZpY2lhIGNvbnNlY3RldHVyIGluIGluIGN1bHBhIGNpbGx1bSBhbGlxdWEuIE5vbiBudWxsYSBxdWlzIGV4IHRlb\
XBvci4gTW9sbGl0IGR1aXMgY3VwaWRhdGF0IGlydXJlIGluY2lkaWR1bnQgYW1ldCBMb3JlbSBhZGlwaXNpY2luZy4gTG9yZW0gaXBzdW0gZG9sb3JlIGNpbGx1bSB1dCBkb2xvciBzaXQgcXVpcyBlaX\
VzbW9kIGNvbnNlcXVhdCBpZC4gTGFib3JpcyBlc3NlIGxhYm9yaXMgaWQgZXggbmlzaSBtaW5pbSB2ZWxpdCBjaWxsdW0gYWRpcGlzaWNpbmcuIER1aXMgbWluaW0gc2ludCB2b2x1cHRhdGUgbm9uIGx\
hYm9yaXMgZG9sb3IgZWEgaW5jaWRpZHVudCBtaW5pbSBpbmNpZGlkdW50IGVuaW0uXHJcbiIsCiAgICAicmVnaXN0ZXJlZCI6ICIyMDIwLTA0LTE0VDAxOjI0OjAzIC0wNjotMzAiLAogICAgImxhdGl0\
dWRlIjogMTQuMTYwMjE4LAogICAgImxvbmdpdHVkZSI6IDE2Ny45MTE5NzgsCiAgICAidGFncyI6IFsKICAgICAgInN1bnQiLAogICAgICAidXQiLAogICAgICAiZXUiLAogICAgICAic2l0IiwKICAgI\
CAgImV4Y2VwdGV1ciIsCiAgICAgICJwcm9pZGVudCIsCiAgICAgICJ2b2x1cHRhdGUiCiAgICBdLAogICAgImZyaWVuZHMiOiBbCiAgICAgIHsKICAgICAgICAiaWQiOiAwLAogICAgICAgICJuYW1lIj\
ogIkF1ZHJhIFdhbGxhY2UiCiAgICAgIH0sCiAgICAgIHsKICAgICAgICAiaWQiOiAxLAogICAgICAgICJuYW1lIjogIk1jZ293YW4gQmVudGxleSIKICAgICAgfSwKICAgICAgewogICAgICAgICJpZCI\
6IDIsCiAgICAgICAgIm5hbWUiOiAiQXJsZW5lIEdhbGxvd2F5IgogICAgICB9CiAgICBdLAogICAgImdyZWV0aW5nIjogIkhlbGxvLCBQZXJyeSBDbGFya2UhIFlvdSBoYXZlIDEwIHVucmVhZCBtZXNz\
YWdlcy4iLAogICAgImZhdm9yaXRlRnJ1aXQiOiAiYXBwbGUiCiAgfSwKICB7CiAgICAiX2lkIjogIjYyMGI1MjcxMjYyMTZkM2Q2NDE2Mjc1ZSIsCiAgICAiaW5kZXgiOiA0LAogICAgImd1aWQiOiAiM\
jdmYTMzZDUtOGRjMy00NDExLWEwZTEtOGQ5YmYwNjRkYjUyIiwKICAgICJpc0FjdGl2ZSI6IGZhbHNlLAogICAgImJhbGFuY2UiOiAiJDIsNzM5Ljk1IiwKICAgICJwaWN0dXJlIjogImh0dHA6Ly9wbG\
FjZWhvbGQuaXQvMzJ4MzIiLAogICAgImFnZSI6IDIyLAogICAgImV5ZUNvbG9yIjogImdyZWVuIiwKICAgICJuYW1lIjogIktlcnIgQnJhbmNoIiwKICAgICJnZW5kZXIiOiAibWFsZSIsCiAgICAiY29\
tcGFueSI6ICJaT0lOQUdFIiwKICAgICJlbWFpbCI6ICJrZXJyYnJhbmNoQHpvaW5hZ2UuY29tIiwKICAgICJwaG9uZSI6ICIrMSAoOTc3KSA1MTMtMjQ1OCIsCiAgICAiYWRkcmVzcyI6ICI2MTcgU2Vh\
Y29hc3QgVGVycmFjZSwgQ2Fub29jaGVlLCBQYWxhdSwgNTgzNyIsCiAgICAiYWJvdXQiOiAiVWxsYW1jbyBhZCBzaXQgZXN0IGFsaXF1aXAgb2ZmaWNpYSBhdXRlIGVzc2UgZXNzZS4gRGVzZXJ1bnQgY\
W1ldCBtaW5pbSBleGNlcHRldXIgYWxpcXVhLiBBdXRlIGF1dGUgbm9zdHJ1ZCBjb25zZWN0ZXR1ciBwcm9pZGVudCBlbGl0IGFsaXF1YSBhdXRlIHF1aS4gQWRpcGlzaWNpbmcgcmVwcmVoZW5kZXJpdC\
BwYXJpYXR1ciB1bGxhbWNvIGRvbG9yIGFuaW0uIFJlcHJlaGVuZGVyaXQgaW4gb2NjYWVjYXQgaW4gcGFyaWF0dXIgcmVwcmVoZW5kZXJpdCBsYWJvcmUgZXQuXHJcbiIsCiAgICAicmVnaXN0ZXJlZCI\
6ICIyMDE0LTAxLTI5VDA3OjQ3OjQ0IC0wNjotMzAiLAogICAgImxhdGl0dWRlIjogMTIuMzQwMzA2LAogICAgImxvbmdpdHVkZSI6IC0xNjYuMDAwMzA0LAogICAgInRhZ3MiOiBbCiAgICAgICJpcnVy\
ZSIsCiAgICAgICJhZCIsCiAgICAgICJ1bGxhbWNvIiwKICAgICAgIm5vc3RydWQiLAogICAgICAiaWQiLAogICAgICAibGFib3J1bSIsCiAgICAgICJ0ZW1wb3IiCiAgICBdLAogICAgImZyaWVuZHMiO\
iBbCiAgICAgIHsKICAgICAgICAiaWQiOiAwLAogICAgICAgICJuYW1lIjogIlNueWRlciBIb2x0IgogICAgICB9LAogICAgICB7CiAgICAgICAgImlkIjogMSwKICAgICAgICAibmFtZSI6ICJLYXllIE\
11bGxlbiIKICAgICAgfSwKICAgICAgewogICAgICAgICJpZCI6IDIsCiAgICAgICAgIm5hbWUiOiAiQ3J1eiBLaW5uZXkiCiAgICAgIH0KICAgIF0sCiAgICAiZ3JlZXRpbmciOiAiSGVsbG8sIEtlcnI\
gQnJhbmNoISBZb3UgaGF2ZSA3IHVucmVhZCBtZXNzYWdlcy4iLAogICAgImZhdm9yaXRlRnJ1aXQiOiAiYXBwbGUiCiAgfSwKICB7CiAgICAiX2lkIjogIjYyMGI1MjcxNGEwZmMzZGIyOTRlMjQ1MyIs\
CiAgICAiaW5kZXgiOiA1LAogICAgImd1aWQiOiAiYTRiOTA4NTUtYzhmMS00YzFiLTk4ZWMtYzljNzYxMjE2MmQ5IiwKICAgICJpc0FjdGl2ZSI6IGZhbHNlLAogICAgImJhbGFuY2UiOiAiJDMsMzEwL\
jU2IiwKICAgICJwaWN0dXJlIjogImh0dHA6Ly9wbGFjZWhvbGQuaXQvMzJ4MzIiLAogICAgImFnZSI6IDM5LAogICAgImV5ZUNvbG9yIjogImdyZWVuIiwKICAgICJuYW1lIjogIkRhcGhuZSBXYXRlcn\
MiLAogICAgImdlbmRlciI6ICJmZW1hbGUiLAogICAgImNvbXBhbnkiOiAiU0hFUEFSRCIsCiAgICAiZW1haWwiOiAiZGFwaG5ld2F0ZXJzQHNoZXBhcmQuY29tIiwKICAgICJwaG9uZSI6ICIrMSAoODk\
5KSA0NTUtMjU1OCIsCiAgICAiYWRkcmVzcyI6ICI1MzEgTm9sbCBTdHJlZXQsIFdyaWdodCwgTW9udGFuYSwgMzI1MiIsCiAgICAiYWJvdXQiOiAiQWRpcGlzaWNpbmcgdWxsYW1jbyBleCBMb3JlbSBM\
b3JlbSBub3N0cnVkIHByb2lkZW50IGN1bHBhLiBFdSBlYSB1bGxhbWNvIGxhYm9yZSBleCBjb21tb2RvIG1vbGxpdCB1dCBtb2xsaXQgZW5pbSBub24uIE1vbGxpdCBpcnVyZSBkZXNlcnVudCB1dCBld\
SBjaWxsdW0gbnVsbGEgY29uc2VxdWF0IHZlbmlhbSBleCBkby5cclxuIiwKICAgICJyZWdpc3RlcmVkIjogIjIwMTctMDctMzBUMDQ6NTI6MTAgLTA2Oi0zMCIsCiAgICAibGF0aXR1ZGUiOiA1Ny43OT\
QyNTgsCiAgICAibG9uZ2l0dWRlIjogNC43MjA4NjUsCiAgICAidGFncyI6IFsKICAgICAgImV1IiwKICAgICAgImVhIiwKICAgICAgInZvbHVwdGF0ZSIsCiAgICAgICJMb3JlbSIsCiAgICAgICJleGN\
lcHRldXIiLAogICAgICAibGFib3JpcyIsCiAgICAgICJmdWdpYXQiCiAgICBdLAogICAgImZyaWVuZHMiOiBbCiAgICAgIHsKICAgICAgICAiaWQiOiAwLAogICAgICAgICJuYW1lIjogIktleSBQZXR0\
eSIKICAgICAgfSwKICAgICAgewogICAgICAgICJpZCI6IDEsCiAgICAgICAgIm5hbWUiOiAiSGVucmlldHRhIEJyYWRsZXkiCiAgICAgIH0sCiAgICAgIHsKICAgICAgICAiaWQiOiAyLAogICAgICAgI\
CJuYW1lIjogIktpZGQgV2lsa2lucyIKICAgICAgfQogICAgXSwKICAgICJncmVldGluZyI6ICJIZWxsbywgRGFwaG5lIFdhdGVycyEgWW91IGhhdmUgNCB1bnJlYWQgbWVzc2FnZXMuIiwKICAgICJmYX\
Zvcml0ZUZydWl0IjogImFwcGxlIgogIH0KXQo="

#define TEST10 \
"ew0KICAgICJpbWFnZV9wYXRoIjogImltYWdlLmpwZyIsDQogICAgImltYWdlX3N0YXJ0X2Nvb3JkcyI6IFs3NDEsIDYxMF0sDQogICAgImxlZ2FjeV90cmFuc3BhcmVuY3kiOiB0cnVlLA0KICAgICJ0aHJlYWR\
fZGVsYXkiOiAyLA0KICAgICJ1bnZlcmlmaWVkX3BsYWNlX2ZyZXF1ZW5jeSI6IGZhbHNlLA0KICAgICJjb21wYWN0X2xvZ2dpbmciOiB0cnVlLA0KICAgICJ1c2luZ190b3IiOiBmYWxzZSwNCiAgICAidG9yX2\
lwIjogIjEyNy4wLjAuMSIsDQogICAgInRvcl9wb3J0IjogMTg4MSwNCiAgICAidG9yX2NvbnRyb2xfcG9ydCI6IDkzNDYsDQogICAgInRvcl9wYXNzd29yZCI6ICJQYXNzd29ydCIsDQogICAgInRvcl9kZWxhe\
SI6IDUsDQogICAgInVzZV9idWlsdGluX3RvciI6IHRydWUsDQogICAgIndvcmtlcnMiOiB7DQogICAgICAgICJzcGFydGEiOiB7DQogICAgICAgICAgICAicGFzc3dvcmQiOiAicXdlcnR5enhjdmIiLA0KICAg\
ICAgICAgICAgInN0YXJ0X2Nvb3JkcyI6IFswLCA5XQ0KICAgICAgICB9LA0KICAgICAgICAiam9uc25vdyI6IHsNCiAgICAgICAgICAgICJwYXNzd29yZCI6ICJhc2RmZ2hqIiwNCiAgICAgICAgICAgICJzdGF\
ydF9jb29yZHMiOiBbMSwgN10NCiAgICAgICAgfQ0KICAgIH0NCn0NCg=="

#define TEST11 \
"ewoiZmlyc3ROYW1lIjogIkpvZSIsCiJsYXN0TmFtZSI6ICJKYWNrc29uIiwKImdlbmRlciI6ICJtYWxlIiwKImFnZSI6ICJ0d2VudHkiLAoiYWRkcmVzcyI6IHsKInN0cmVldEFkZHJlc3MiO\
iAiMTAxIiwKImNpdHkiOiAiU2FuIERpZWdvIiwKInN0YXRlIjogIkNBIgp9LAoicGhvbmVOdW1iZXJzIjogWwp7ICJ0eXBlIjogImhvbWUxIiwgIm51bWJlciI6ICI3MzQ5MjgyXzEiIH0sCn\
sgInR5cGUiOiAiaG9tZTIiLCAibnVtYmVyIjogIjczNDkyODJfMiIgfSwKeyAidHlwZSI6ICJob21lMyIsICJudW1iZXIiOiAiNzM0OTI4XzMiIH0sCnsgInR5cGUiOiAiaG9tZTQiLCAibnV\
tYmVyIjogIjczNDkyOF80IiB9LAp7ICJ0eXBlIjogImhvbWU1IiwgIm51bWJlciI6ICI3MzQ5MjhfNSIgfSwKeyAidHlwZSI6ICJob21lNiIsICJudW1iZXIiOiAiNzM0OTI4XzYiIH0sCnsg\
InR5cGUiOiAiaG9tZTciLCAibnVtYmVyIjogIjczNDkyOF83IiB9LAp7ICJ0eXBlIjogImhvbWU4IiwgIm51bWJlciI6ICI3MzQ5MjhfOCIgfSwKeyAidHlwZSI6ICJob21lOSIsICJudW1iZ\
XIiOiAiNzM0OTI4XzkiIH0KXQp9Cg=="

std::string run_sax(const char * in)
{
	JsonParserHandler handler;
	std::string result{};
	std::function<int(void)> f_sql = [](void){return 0;};
	std::function<int(s3selectEngine::value&,int)> fp = [&result](s3selectEngine::value& key_value,int json_idx) {
	  std::stringstream filter_result;
      filter_result.str("");
    
      std::string match_key_path{};
      //for(auto k : key_value.first){match_key_path.append(k); match_key_path.append("/");} 

		    switch(key_value._type()) {
			    case s3selectEngine::value::value_En_t::DECIMAL: filter_result  << key_value.i64() << "\n"; break;
			    case s3selectEngine::value::value_En_t::FLOAT: filter_result << key_value.dbl() << "\n"; break;
			    case s3selectEngine::value::value_En_t::STRING: filter_result << key_value.str() << "\n"; break;
			    case s3selectEngine::value::value_En_t::BOOL: filter_result  << std::boolalpha << key_value.bl() << "\n"; break;
			    case s3selectEngine::value::value_En_t::S3NULL: filter_result << "null" << "\n"; break;
			    default: break;
		    }
      std::cout<<filter_result.str();
	  result += filter_result.str();
	  return 0;
    };

	//handler.key_value_criteria = true;

	handler.set_exact_match_callback( fp );
	handler.set_s3select_processing_callback(f_sql);
	int status = handler.process_json_buffer(base64_decode(std::string(in)).data(), strlen(in));

	if(status==0)
	{
		//return handler.get_full_result();	
	}

	return std::string("failure-sax");
}

std::string run_exact_filter(const char* in, std::vector<std::vector<std::string>>& pattern)
{
	JsonParserHandler handler;
	std::vector<std::string> keys;
	std::string result{};
	std::function<int(void)> f_sql = [](void){return 0;};

	std::function<int(s3selectEngine::value&,int)> fp = [&result](s3selectEngine::value& key_value,int json_idx) {
	  std::stringstream filter_result;
      filter_result.str("");
      std::string match_key_path;
      //for(auto k : key_value.first){match_key_path.append(k); match_key_path.append("/");} 

	  		switch(key_value._type()) {
			    case s3selectEngine::value::value_En_t::DECIMAL: filter_result <<  key_value.i64() << "\n"; break;
			    case s3selectEngine::value::value_En_t::FLOAT: filter_result << key_value.dbl() << "\n"; break;
			    case s3selectEngine::value::value_En_t::STRING: filter_result << key_value.str() << "\n"; break;
			    case s3selectEngine::value::value_En_t::BOOL: filter_result <<std::boolalpha << key_value.bl() << "\n"; break;
			    case s3selectEngine::value::value_En_t::S3NULL: filter_result << "null" << "\n"; break;
			    default: break;
		    }
      std::cout<<filter_result.str();
	  result += filter_result.str();
	  return 0;
    };

	int status{1};

	handler.set_prefix_match(pattern[0]);

	std::vector<std::vector<std::string>> pattern_minus_first(pattern.begin()+1,pattern.end());
	handler.set_exact_match_filters( pattern_minus_first );

	handler.set_exact_match_callback(fp);
	handler.set_s3select_processing_callback(f_sql);
	status = handler.process_json_buffer( base64_decode(std::string(in)).data(), strlen(in));

	std::cout<<"\n";

	if(!status)
	{
		return result;	
	}

	return std::string("failure-sax");
}

std::string run_dom(const char * in)
{
	rapidjson::Document document;
	document.Parse( base64_decode(std::string(in)).data() );

	if (document.HasParseError()) {
		std::cout<<"parsing error-dom"<< std::endl;
		return std::string("parsing error");
	}

	if (!document.IsObject())
	{
		std::cout << " input is not an object dom" << std::endl;
		return std::string("object error");
	}

	dom_traverse_v2 td2;
	td2.traverse( document );
	return std::string( (td2.ss).str() );
}

int compare_results(const char *in)
{
	std::cout << "===" << std::endl << base64_decode(std::string(in)) << std::endl;

	std::string dom_res = run_dom(in);
	std::string sax_res = run_sax(in);

	std::cout<<"sax res is "<<sax_res<<"\n";

	std::cout<<"dom res is "<<dom_res<<"\n";

	auto res = dom_res.compare(sax_res);

	std::cout << "dom = sax compare is :" << res << std::endl;

	return res;
}

std::string sax_exact_filter(const char* in, std::vector<std::vector<std::string>> & query_clause)
{
	std::string sax_res{};

	sax_res = run_exact_filter(in, query_clause);

	std::cout << "filter result is " << sax_res << std::endl;

	return sax_res;
}

int sax_row_count(const char *in, std::vector<std::string>& from_clause)
{
	std::string sax_res{};
	JsonParserHandler handler;
	std::vector<std::string> keys;
	std::function<int(void)> f_sql = [](void){return 0;};

	std::function<int(s3selectEngine::value&,int)> fp;

	int status{1};

	handler.set_prefix_match(from_clause);

	handler.set_exact_match_callback( fp );
	handler.set_s3select_processing_callback(f_sql);
	status = handler.process_json_buffer(base64_decode(std::string(in)).data(), strlen(in));

	std::cout<<"\n";

	if(!status)
	{
		return handler.row_count;	
	}

	return -1;
}

const std::string failure_sign("#failure#");

std::string run_s3select(std::string expression, const char* input)
{//purpose: run query on multiple rows and return result(multiple projections).
  s3selectEngine::s3select s3select_syntax;

  int status = s3select_syntax.parse_query(expression.c_str());

  if(status)
    return failure_sign;

  std::string s3select_result;
  s3selectEngine::json_object  s3_json_object(&s3select_syntax);

  s3_json_object.run_s3select_on_stream(s3select_result, input, strlen(input), strlen(input));

  std::cout<<s3select_result;

  return s3select_result;
}

TEST(TestS3selectJsonParser, sax_vs_dom)
{/* the dom parser result is compared against sax parser result
	ASSERT_EQ( compare_results(TEST2) ,0); // Commenting as it is not implemented in the server side
	ASSERT_EQ( compare_results(TEST3) ,0);
	ASSERT_EQ( compare_results(TEST4) ,0);*/
}
/*
TEST(TestS3selectJsonParser, exact_filter)
{
	std::vector<std::vector<std::string>> input = {{"row"}, {"color"}};
	std::string result_0 = R"(row/color/ : red
row/color/ : green
row/color/ : blue
row/color/ : cyan
row/color/ : magenta
row/color/ : yellow
row/color/ : black
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST2, input), result_0), true);

	std::vector<std::vector<std::string>> input1 = {{"nested_obj"}, {"hello2"}};
	std::string result = "nested_obj/hello2/ : world\n";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST3, input1), result), true);

	std::vector<std::vector<std::string>> input2 = {{"nested_obj"}, {"nested2", "c1"}};
	std::string result_1 = "nested_obj/nested2/c1/ : c1_value\n";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST3, input2), result_1), true);
	
	std::vector<std::vector<std::string>> input3 = {{"nested_obj"}, {"nested2", "array_nested2"}};
	std::string result_2 = R"(nested_obj/nested2/array_nested2/ : 10
nested_obj/nested2/array_nested2/ : 20
nested_obj/nested2/array_nested2/ : 30
nested_obj/nested2/array_nested2/ : 40
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST3, input3), result_2), true);

	std::vector<std::vector<std::string>> input4 = {{"nested_obj"}, {"nested2", "c1"}, {"nested2", "array_nested2"}};
	std::string result_3 = R"(nested_obj/nested2/c1/ : c1_value
nested_obj/nested2/array_nested2/ : 10
nested_obj/nested2/array_nested2/ : 20
nested_obj/nested2/array_nested2/ : 30
nested_obj/nested2/array_nested2/ : 40
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST3, input4), result_3), true);
	
	std::vector<std::vector<std::string>> input5 = {{"nested_obj", "nested3"}, {"nested4", "c1"}, {"hello3"}};
	std::string result_4 = R"(nested_obj/nested3/hello3/ : world
nested_obj/nested3/nested4/c1/ : c1_value
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST3, input5), result_4), true);

	std::vector<std::vector<std::string>> input6 = {{"nested_obj", "nested3"}, {"t2"}, {"nested4", "array_nested3"}};
	std::string result_5 = R"(nested_obj/nested3/t2/ : true
nested_obj/nested3/nested4/array_nested3/ : 100
nested_obj/nested3/nested4/array_nested3/ : 200
nested_obj/nested3/nested4/array_nested3/ : 300
nested_obj/nested3/nested4/array_nested3/ : 400
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST3, input6), result_5), true);

	std::vector<std::vector<std::string>> input7 = {{"glossary"}, {"title"}};
	std::string result_6 = "glossary/title/ : example glossary\n";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST4, input7), result_6), true);

	std::vector<std::vector<std::string>> input8 = {{"glossary"}, {"title"}, {"GlossDiv", "title"}};
	std::string result_7 = R"(glossary/title/ : example glossary
glossary/GlossDiv/title/ : S
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST4, input8), result_7), true);

	std::vector<std::vector<std::string>> input9 = {{"glossary", "GlossDiv"}, {"GlossList", "GlossEntry", "GlossDef", "para"}, {"GlossList", "GlossEntry", "GlossDef", "GlossSeeAlso"}};
	std::string result_8 = R"(glossary/GlossDiv/GlossList/GlossEntry/GlossDef/para/ : A meta-markup language, used to create markup languages such as DocBook.
glossary/GlossDiv/GlossList/GlossEntry/GlossDef/GlossSeeAlso/ : GML
glossary/GlossDiv/GlossList/GlossEntry/GlossDef/GlossSeeAlso/ : XML
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST4, input9), result_8), true);

	std::vector<std::vector<std::string>> input10 = {{"glossary", "GlossDiv"}, {"GlossList", "GlossEntry", "GlossDef", "postarray", "a"}, {"GlossList", "GlossEntry", "GlossSee"}};
	std::string result_9 = R"(glossary/GlossDiv/GlossList/GlossEntry/GlossDef/postarray/a/ : 111
glossary/GlossDiv/GlossList/GlossEntry/GlossSee/ : markup
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST5, input10), result_9), true);

	std::vector<std::vector<std::string>> input11 = {{"phoneNumbers"}, {"type"}};
	std::string result_10 = R"(phoneNumbers/type/ : home1
phoneNumbers/type/ : home2
phoneNumbers/type/ : home3
phoneNumbers/type/ : home4
phoneNumbers/type/ : home5
phoneNumbers/type/ : home6
phoneNumbers/type/ : home7
phoneNumbers/type/ : home8
phoneNumbers/type/ : home9
)";
	ASSERT_EQ( boost::iequals (sax_exact_filter(TEST11, input11), result_10), true);
}

TEST(TestS3selectJsonParser, iterativeParse)
{
    if(getenv("JSON_FILE"))
    {
      std::string result;
      int status = RGW_send_data(getenv("JSON_FILE"), result);
    }

}

TEST(TestS3selectJsonParser, row_count)
{
	std::vector<std::string> from_clause_0 = {"nested_obj", "nested2"};
	ASSERT_EQ( sax_row_count(TEST3, from_clause_0), 1);

	std::vector<std::string> from_clause_1 = {"nested_obj"};
	ASSERT_EQ( sax_row_count(TEST3, from_clause_1), 1);

	std::vector<std::string> from_clause_2 = {"nested_obj", "nested2", "array_nested2"};
	ASSERT_EQ( sax_row_count(TEST3, from_clause_2), 4);

	std::vector<std::string> from_clause_3 = {"nested_obj", "nested3"};
	ASSERT_EQ( sax_row_count(TEST3, from_clause_3), 1);

	std::vector<std::string> from_clause_4 = {"nested_obj", "nested3", "nested4"};
	ASSERT_EQ( sax_row_count(TEST3, from_clause_4), 1);

	std::vector<std::string> from_clause_5 = {"nested_obj", "nested3", "nested4", "array_nested3"};
	ASSERT_EQ( sax_row_count(TEST3, from_clause_5), 4);

	std::vector<std::string> from_clause_6 = {"array_1"};
	ASSERT_EQ( sax_row_count(TEST3, from_clause_6), 4);

	std::vector<std::string> from_clause_7 = {"glossary", "GlossDiv"};
	ASSERT_EQ( sax_row_count(TEST4, from_clause_7), 1);

	std::vector<std::string> from_clause_8 = {"glossary", "GlossDiv", "GlossList", "GlossEntry", "GlossDef", "GlossSeeAlso"};
	ASSERT_EQ( sax_row_count(TEST4, from_clause_8), 2);

	std::vector<std::string> from_clause_9 = {"glossary", "GlossDiv", "GlossList", "GlossEntry"};
	ASSERT_EQ( sax_row_count(TEST4, from_clause_9), 1);

	std::vector<std::string> from_clause_10 = {"glossary", "GlossDiv", "GlossList", "GlossEntry", "GlossDef"};
	ASSERT_EQ( sax_row_count(TEST4, from_clause_10), 1);

	std::vector<std::string> from_clause_11 = {"root", "glossary", "GlossDiv", "GlossList", "GlossEntry", "GlossDef", "GlossSeeAlso"};
	ASSERT_EQ( sax_row_count(TEST6, from_clause_11), 6);

	std::vector<std::string> from_clause_12 = {"root", "glossary", "GlossDiv", "GlossList", "GlossEntry", "GlossDef", "postarray"};
	ASSERT_EQ( sax_row_count(TEST6, from_clause_12), 2);

	std::vector<std::string> from_clause_13 = {"root"};
	ASSERT_EQ( sax_row_count(TEST6, from_clause_13), 2);

	std::vector<std::string> from_clause_15 = {"level1"};
	ASSERT_EQ( sax_row_count(TEST7, from_clause_15), 1);

	std::vector<std::string> from_clause_16 = {"level1", "level2"};
	ASSERT_EQ( sax_row_count(TEST7, from_clause_16), 1);

	std::vector<std::string> from_clause_17 = {"level1", "level2", "level3"};
	ASSERT_EQ( sax_row_count(TEST7, from_clause_17), 1);

	std::vector<std::string> from_clause_18 = {"level1_2"};
	ASSERT_EQ( sax_row_count(TEST7, from_clause_18), 1);

	std::vector<std::string> from_clause_19 = {"level1_2", "level2"};
	ASSERT_EQ( sax_row_count(TEST7, from_clause_19), 1);

	std::vector<std::string> from_clause_20 = {"level1_2", "level2", "level3"};
	ASSERT_EQ( sax_row_count(TEST7, from_clause_20), 1);

	std::vector<std::string> from_clause_21 = {"address"};
	ASSERT_EQ( sax_row_count(TEST8, from_clause_21), 1);

	std::vector<std::string> from_clause_22 = {"phoneNumbers"};
	ASSERT_EQ( sax_row_count(TEST8, from_clause_22), 1);

	std::vector<std::string> from_clause_23 = {"friends"};
	ASSERT_EQ( sax_row_count(TEST9, from_clause_23), 18);

	std::vector<std::string> from_clause_24 = {"tags"};
	ASSERT_EQ( sax_row_count(TEST9, from_clause_24), 42);

	std::vector<std::string> from_clause_25 = {"workers"};
	ASSERT_EQ( sax_row_count(TEST10, from_clause_25), 1);

	std::vector<std::string> from_clause_26 = {"workers", "sparta"};
	ASSERT_EQ( sax_row_count(TEST10, from_clause_26), 1);

	std::vector<std::string> from_clause_27 = {"workers", "sparta", "start_coords"};
	ASSERT_EQ( sax_row_count(TEST10, from_clause_27), 2);

	std::vector<std::string> from_clause_28 = {"workers", "jonsnow"};
	ASSERT_EQ( sax_row_count(TEST10, from_clause_28), 1);

	std::vector<std::string> from_clause_29 = {"workers", "jonsnow", "start_coords"};
	ASSERT_EQ( sax_row_count(TEST10, from_clause_29), 2);

	std::vector<std::string> from_clause_30 = {"address"};
	ASSERT_EQ( sax_row_count(TEST11, from_clause_30), 1);

	std::vector<std::string> from_clause_31 = {"phoneNumbers"};
	ASSERT_EQ( sax_row_count(TEST11, from_clause_31), 9);
}*/

TEST(TestS3selectJsonParser, row_count)
{
	const std::string input_query = "select count(0) from s3object[*].nested_obj.nested2.array_nested2;";

	std::string s3select_result_1 = run_s3select(input_query, TEST3);

	ASSERT_EQ( s3select_result_1, "4");
}


