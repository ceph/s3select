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
#include "s3select.h"


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

std::string json3 = R"({
  "hello": "world",
    "t": "true" ,
    "f": "false",
    "n": "null",
    "i": 123,
    "pi": 3.1416,

    "nested_obj" : {
      "hello2": "world",
      "t2": true,
      "nested2" : {
        "c1" : "c1_value" ,
        "array_nested2": [10, 20, 30, 40]
      },
      "nested3" :{
        "hello3": "world",
        "t2": true,
        "nested4" : {
          "c1" : "c1_value" ,
          "array_nested3": [100, 200, 300, 400]
        }
      }
    },
    "array_1": [1, 2, 3, 4]
}
)";

std::string json4 = R"({

    "glossary": {
        "title": "example glossary",
		"GlossDiv": {
            "title": "S",
			"GlossList": {
                "GlossEntry": {
                    "ID": "SGML",
					"SortAs": "SGML",
					"GlossTerm": "Standard Generalized Markup Language",
					"Acronym": "SGML",
					"Abbrev": "ISO 8879:1986",
					"GlossDef": {
                        "para": "A meta-markup language, used to create markup languages such as DocBook.",
						"GlossSeeAlso": ["GML", "XML"]
                    },
					"GlossSee": "markup"
                }
            }
        }
    }
}
)";

std::string json6 = R"({
"root" : [
{

    "glossary": {
        "title": "example glossary",
		"GlossDiv": {
            "title": "S",
			"GlossList": {
                "GlossEntry": {
                    "ID": "SGML",
					"SortAs": "SGML",
					"GlossTerm": "Standard Generalized Markup Language",
					"Acronym": "SGML",
					"Abbrev": "ISO 8879:1986",
					"GlossDef": {
                        "para": "A meta-markup language, used to create markup languages such as DocBook.",
						"GlossSeeAlso": ["GML", "XML"],
						"postarray": {
							  "a":111,
							  "b":222
						}
                    },
					"GlossSee": "markup"
                },
                "GlossEntry": 
		{
                    "ID": "SGML",
					"SortAs": "SGML",
					"GlossTerm": "Standard Generalized Markup Language",
					"Acronym": "SGML",
					"Abbrev": "ISO 8879:1986",
					"GlossDef": {
                        "para": "A meta-markup language, used to create markup languages such as DocBook.",
						"GlossSeeAlso": ["GML", "XML"],
						"postarray": {
							  "a":111,
							  "b":222
						}
                    },
					"GlossSee": "markup"
                }
            }
        }
    }
}
,
{

    "glossary": {
        "title": "example glossary",
		"GlossDiv": {
            "title": "S",
			"GlossList": {
                "GlossEntry": {
                    "ID": "SGML",
					"SortAs": "SGML",
					"GlossTerm": "Standard Generalized Markup Language",
					"Acronym": "SGML",
					"Abbrev": "ISO 8879:1986",
					"GlossDef": {
                        "para": "A meta-markup language, used to create markup languages such as DocBook.",
						"GlossSeeAlso": ["GML", "XML"]
                    },
					"GlossSee": "markup"
                }
            }
        }
    }
}
]
}
)";

std::string json7 = R"({
  "level1" : {
    "level2" : {
      "level3" : {
	"level4" : "value4"
      }
    }
  },
    "level1_2" :{
      "level2" : {
	"level3" : {
	  "level4" : "value4_2"
	}
      }
    }
}
)";

std::string json8 = R"({
   "firstName": "Joe",
   "lastName": "Jackson",
   "gender": "male",
   "age": "twenty",
   "address": {
       "streetAddress": "101",
       "city": "San Diego",
       "state": "CA"
   },
   "phoneNumbers": [
       { "type": "home", "number": "7349282382" }
   ]
}
)";

std::string json9 = R"([
  {
    "_id": "620b5271b392b74562e38700",
    "index": 0,
    "guid": "4c827946-242e-43a7-8717-72bb2af3fde2",
    "isActive": true,
    "balance": "$3,057.53",
    "picture": "http://placehold.it/32x32",
    "age": 20,
    "eyeColor": "blue",
    "name": "Montgomery Greene",
    "gender": "male",
    "company": "VENDBLEND",
    "email": "montgomerygreene@vendblend.com",
    "phone": "+1 (894) 582-2530",
    "address": "703 Bayview Avenue, Carrsville, Virgin Islands, 2622",
    "about": "Aute ullamco excepteur laborum minim anim quis aute ad. Esse non esse irure ad sint et ullamco tempor qui culpa consequat exercitation Lorem ullamco. Proident anim elit et nulla cupidatat esse. Velit excepteur aliquip et reprehenderit quis culpa proident laborum esse ullamco ea elit non. Nostrud id laboris magna incididunt ut tempor cupidatat elit excepteur in sit laborum. Irure veniam esse aute adipisicing elit esse. Tempor non ullamco excepteur cupidatat reprehenderit reprehenderit id commodo duis ullamco sint incididunt in velit.\r\n",
    "registered": "2016-08-23T08:31:08 -06:-30",
    "latitude": -15.395885,
    "longitude": -6.730017,
    "tags": [
      "eiusmod",
      "aliqua",
      "ipsum",
      "irure",
      "elit",
      "quis",
      "sit"
    ],
    "friends": [
      {
        "id": 0,
        "name": "Kane Chen"
      },
      {
        "id": 1,
        "name": "Dianna Lawrence"
      },
      {
        "id": 2,
        "name": "Leila Juarez"
      }
    ],
    "greeting": "Hello, Montgomery Greene! You have 5 unread messages.",
    "favoriteFruit": "banana"
  },
  {
    "_id": "620b527187b2952901055169",
    "index": 1,
    "guid": "3f477980-7300-4f89-a2b1-5e467d1278ee",
    "isActive": false,
    "balance": "$2,156.88",
    "picture": "http://placehold.it/32x32",
    "age": 30,
    "eyeColor": "brown",
    "name": "Stewart Cain",
    "gender": "male",
    "company": "XSPORTS",
    "email": "stewartcain@xsports.com",
    "phone": "+1 (825) 599-2845",
    "address": "703 Highland Avenue, Belfair, Hawaii, 217",
    "about": "Culpa mollit ullamco ad exercitation. Sint mollit in in ad minim mollit culpa nisi. Reprehenderit aliqua do sit nisi amet esse ad consectetur nulla aute id aliqua magna.\r\n",
    "registered": "2017-03-20T10:28:24 -06:-30",
    "latitude": 58.475892,
    "longitude": 141.356935,
    "tags": [
      "pariatur",
      "duis",
      "laboris",
      "mollit",
      "irure",
      "eiusmod",
      "sint"
    ],
    "friends": [
      {
        "id": 0,
        "name": "Neal Lopez"
      },
      {
        "id": 1,
        "name": "Tiffany Cochran"
      },
      {
        "id": 2,
        "name": "Stevens Davenport"
      }
    ],
    "greeting": "Hello, Stewart Cain! You have 10 unread messages.",
    "favoriteFruit": "apple"
  },
  {
    "_id": "620b5271fe9805b815fb870b",
    "index": 2,
    "guid": "1a1ccab6-1059-4fcc-92f3-248d780e08bb",
    "isActive": true,
    "balance": "$2,827.15",
    "picture": "http://placehold.it/32x32",
    "age": 25,
    "eyeColor": "brown",
    "name": "Davidson Prince",
    "gender": "male",
    "company": "MITROC",
    "email": "davidsonprince@mitroc.com",
    "phone": "+1 (834) 501-2167",
    "address": "251 Portland Avenue, Fostoria, Minnesota, 9179",
    "about": "Ullamco mollit anim dolore laboris cupidatat. Aliquip non dolor dolore velit aliquip consectetur. Non culpa non aute esse voluptate elit esse consectetur sit ad consequat. Deserunt ipsum nisi aliqua amet non laboris cillum reprehenderit Lorem laborum commodo ullamco laborum.\r\n",
    "registered": "2020-07-01T10:08:13 -06:-30",
    "latitude": 48.483322,
    "longitude": 153.723574,
    "tags": [
      "irure",
      "occaecat",
      "dolore",
      "tempor",
      "mollit",
      "est",
      "laboris"
    ],
    "friends": [
      {
        "id": 0,
        "name": "Lucy Conrad"
      },
      {
        "id": 1,
        "name": "Curtis Tyler"
      },
      {
        "id": 2,
        "name": "Tara Talley"
      }
    ],
    "greeting": "Hello, Davidson Prince! You have 3 unread messages.",
    "favoriteFruit": "strawberry"
  },
  {
    "_id": "620b52717900b34aeb899051",
    "index": 3,
    "guid": "8d11c29c-cfab-4105-abf4-c7b4576eb89b",
    "isActive": false,
    "balance": "$1,861.02",
    "picture": "http://placehold.it/32x32",
    "age": 28,
    "eyeColor": "green",
    "name": "Perry Clarke",
    "gender": "male",
    "company": "ZILLADYNE",
    "email": "perryclarke@zilladyne.com",
    "phone": "+1 (887) 439-3743",
    "address": "597 Thames Street, Blende, Georgia, 8212",
    "about": "Incididunt tempor minim aliqua dolore officia consectetur in in culpa cillum aliqua. Non nulla quis ex tempor. Mollit duis cupidatat irure incididunt amet Lorem adipisicing. Lorem ipsum dolore cillum ut dolor sit quis eiusmod consequat id. Laboris esse laboris id ex nisi minim velit cillum adipisicing. Duis minim sint voluptate non laboris dolor ea incididunt minim incididunt enim.\r\n",
    "registered": "2020-04-14T01:24:03 -06:-30",
    "latitude": 14.160218,
    "longitude": 167.911978,
    "tags": [
      "sunt",
      "ut",
      "eu",
      "sit",
      "excepteur",
      "proident",
      "voluptate"
    ],
    "friends": [
      {
        "id": 0,
        "name": "Audra Wallace"
      },
      {
        "id": 1,
        "name": "Mcgowan Bentley"
      },
      {
        "id": 2,
        "name": "Arlene Galloway"
      }
    ],
    "greeting": "Hello, Perry Clarke! You have 10 unread messages.",
    "favoriteFruit": "apple"
  },
  {
    "_id": "620b527126216d3d6416275e",
    "index": 4,
    "guid": "27fa33d5-8dc3-4411-a0e1-8d9bf064db52",
    "isActive": false,
    "balance": "$2,739.95",
    "picture": "http://placehold.it/32x32",
    "age": 22,
    "eyeColor": "green",
    "name": "Kerr Branch",
    "gender": "male",
    "company": "ZOINAGE",
    "email": "kerrbranch@zoinage.com",
    "phone": "+1 (977) 513-2458",
    "address": "617 Seacoast Terrace, Canoochee, Palau, 5837",
    "about": "Ullamco ad sit est aliquip officia aute esse esse. Deserunt amet minim excepteur aliqua. Aute aute nostrud consectetur proident elit aliqua aute qui. Adipisicing reprehenderit pariatur ullamco dolor anim. Reprehenderit in occaecat in pariatur reprehenderit labore et.\r\n",
    "registered": "2014-01-29T07:47:44 -06:-30",
    "latitude": 12.340306,
    "longitude": -166.000304,
    "tags": [
      "irure",
      "ad",
      "ullamco",
      "nostrud",
      "id",
      "laborum",
      "tempor"
    ],
    "friends": [
      {
        "id": 0,
        "name": "Snyder Holt"
      },
      {
        "id": 1,
        "name": "Kaye Mullen"
      },
      {
        "id": 2,
        "name": "Cruz Kinney"
      }
    ],
    "greeting": "Hello, Kerr Branch! You have 7 unread messages.",
    "favoriteFruit": "apple"
  },
  {
    "_id": "620b52714a0fc3db294e2453",
    "index": 5,
    "guid": "a4b90855-c8f1-4c1b-98ec-c9c7612162d9",
    "isActive": false,
    "balance": "$3,310.56",
    "picture": "http://placehold.it/32x32",
    "age": 39,
    "eyeColor": "green",
    "name": "Daphne Waters",
    "gender": "female",
    "company": "SHEPARD",
    "email": "daphnewaters@shepard.com",
    "phone": "+1 (899) 455-2558",
    "address": "531 Noll Street, Wright, Montana, 3252",
    "about": "Adipisicing ullamco ex Lorem Lorem nostrud proident culpa. Eu ea ullamco labore ex commodo mollit ut mollit enim non. Mollit irure deserunt ut eu cillum nulla consequat veniam ex do.\r\n",
    "registered": "2017-07-30T04:52:10 -06:-30",
    "latitude": 57.794258,
    "longitude": 4.720865,
    "tags": [
      "eu",
      "ea",
      "voluptate",
      "Lorem",
      "excepteur",
      "laboris",
      "fugiat"
    ],
    "friends": [
      {
        "id": 0,
        "name": "Key Petty"
      },
      {
        "id": 1,
        "name": "Henrietta Bradley"
      },
      {
        "id": 2,
        "name": "Kidd Wilkins"
      }
    ],
    "greeting": "Hello, Daphne Waters! You have 4 unread messages.",
    "favoriteFruit": "apple"
  }
]
)";

std::string json10 = R"({
    "image_path": "image.jpg",
    "image_start_coords": [741, 610],
    "legacy_transparency": true,
    "thread_delay": 2,
    "unverified_place_frequency": false,
    "compact_logging": true,
    "using_tor": false,
    "tor_ip": "127.0.0.1",
    "tor_port": 1881,
    "tor_control_port": 9346,
    "tor_password": "Passwort",
    "tor_delay": 5,
    "use_builtin_tor": true,
    "workers": {
        "sparta": {
            "password": "qwertyzxcvb",
            "start_coords": [0, 9]
        },
        "jonsnow": {
            "password": "asdfghj",
            "start_coords": [1, 7]
        }
    }
}
)";

std::string json11 = R"({
"firstName": "Joe",
"lastName": "Jackson",
"gender": "male",
"age": "twenty",
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},
"phoneNumbers": [
{ "type": "home1", "number": "7349282_1" },
{ "type": "home2", "number": "7349282_2" },
{ "type": "home3", "number": "734928_3" },
{ "type": "home4", "number": "734928_4" },
{ "type": "home5", "number": "734928_5" },
{ "type": "home6", "number": "734928_6" },
{ "type": "home7", "number": "734928_7" },
{ "type": "home8", "number": "734928_8" },
{ "type": "home9", "number": "734928_9" }
]
}
)";

std::string json5 = R"({
    "glossary": {
        "title": "example glossary",
                "GlossDiv": {
            "title": "S",
                        "GlossList": {
                "GlossEntry": {
                    "ID": "SGML",
                                        "SortAs": "SGML",
                                        "GlossTerm": "Standard Generalized Markup Language",
                                        "Acronym": "SGML",
                                        "Abbrev": "ISO 8879:1986",
                                        "GlossDef": {
                        "para": "A meta-markup language, used to create markup languages such as DocBook.",
                                                "GlossSeeAlso": ["GML", "XML"],
                                                "postarray": {
                                                          "a":111,
                                                          "b":222
                                                }
                    },
                                        "GlossSee": "markup"
                }
            }
        }
    }
}
)";

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
	int status = handler.process_json_buffer(std::string(in).data(), strlen(in));

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

	//std::vector<std::vector<std::string>> pattern_minus_first(pattern.begin()+1,pattern.end());
	//handler.set_exact_match_filters( pattern_minus_first );

	handler.set_exact_match_callback(fp);
	handler.set_s3select_processing_callback(f_sql);
	status = handler.process_json_buffer( std::string(in).data(), strlen(in));

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
	document.Parse( std::string(in).data() );

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
	std::cout << "===" << std::endl << std::string(in) << std::endl;

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
	status = handler.process_json_buffer(std::string(in).data(), strlen(in));

	std::cout<<"\n";

	if(!status)
	{
		return handler.row_count;	
	}

	return -1;
}

int run_json_query(const char* json_query, std::string& json_input,std::string& result)
{//purpose: run single-chunk json queries

  s3selectEngine::s3select s3select_syntax;
  int status = s3select_syntax.parse_query(json_query);
  if (status != 0)
  {
    std::cout << "failed to parse query " << s3select_syntax.get_error_description() << std::endl;
    return -1;
  }

  s3selectEngine::json_object json_query_processor(&s3select_syntax);
  result.clear();
  status = json_query_processor.run_s3select_on_stream(result, json_input.data(), json_input.size(), json_input.size());
  std::string prev_result = result;
  result.clear();
  status = json_query_processor.run_s3select_on_stream(result, 0, 0, json_input.size());
  result = prev_result + result;

  return status;
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
      ASSERT_EQ(status, 0);
    }
}*/

TEST(TestS3selectJsonParser, row_count)
{
	std::string result{};
	const char* input_query = "select count(0) from s3object[*].row;";

	run_json_query(input_query, json2, result);

	ASSERT_EQ(result, "7");

	const char* input_query_0 = "select count(0) from s3object[*].nested_obj.nested2;";

	run_json_query(input_query_0, json3, result);

	ASSERT_EQ(result, "1");

	const char* input_query_1 = "select count(0) from s3object[*].nested_obj;";

	run_json_query(input_query_1, json3, result);

	ASSERT_EQ(result, "1");

	const char* input_query_2 = "select count(0) from s3object[*].nested_obj.nested2.array_nested2;";

	run_json_query(input_query_2, json3, result);

	ASSERT_EQ(result, "4");

	const char* input_query_3 = "select count(0) from s3object[*].nested_obj.nested3;";

	run_json_query(input_query_3, json3, result);

	ASSERT_EQ(result, "1");

	const char* input_query_4 = "select count(0) from s3object[*].nested_obj.nested3.nested4;";

	run_json_query(input_query_4, json3, result);

	ASSERT_EQ(result, "1");

	const char* input_query_5 = "select count(0) from s3object[*].nested_obj.nested3.nested4.array_nested3;";

	run_json_query(input_query_5, json3, result);

	ASSERT_EQ(result, "4");

	const char* input_query_6 = "select count(0) from s3object[*].array_1;";

	run_json_query(input_query_6, json3, result);

	ASSERT_EQ(result, "4");

	const char* input_query_7 = "select count(0) from s3object[*].glossary.GlossDiv;";

	run_json_query(input_query_7, json4, result);

	ASSERT_EQ(result, "1");

	const char* input_query_8 = "select count(0) from s3object[*].glossary.GlossDiv.GlossList.GlossEntry.GlossDef.GlossSeeAlso;";

	run_json_query(input_query_8, json4, result);

	ASSERT_EQ(result, "2");

	const char* input_query_9 = "select count(0) from s3object[*].glossary.GlossDiv.GlossList.GlossEntry;";

	run_json_query(input_query_9, json4, result);

	ASSERT_EQ(result, "1");

	const char* input_query_10 = "select count(0) from s3object[*].glossary.GlossDiv.GlossList.GlossEntry.GlossDef;";

	run_json_query(input_query_10, json4, result);

	ASSERT_EQ(result, "1");

	const char* input_query_11 = "select count(0) from s3object[*].root.glossary.GlossDiv.GlossList.GlossEntry.GlossDef.GlossSeeAlso;";

	run_json_query(input_query_11, json6, result);

	ASSERT_EQ(result, "6");

	const char* input_query_12 = "select count(0) from s3object[*].root.glossary.GlossDiv.GlossList.GlossEntry.GlossDef.postarray;";

	run_json_query(input_query_12, json6, result);

	ASSERT_EQ(result, "2");

	const char* input_query_13 = "select count(0) from s3object[*].root;";

	run_json_query(input_query_13, json6, result);

	ASSERT_EQ(result, "2");

	const char* input_query_14 = "select count(0) from s3object[*].level1;";

	run_json_query(input_query_14, json7, result);

	ASSERT_EQ(result, "1");

	const char* input_query_15 = "select count(0) from s3object[*].level1.level2;";

	run_json_query(input_query_15, json7, result);

	ASSERT_EQ(result, "1");

	const char* input_query_16 = "select count(0) from s3object[*].level1.level2.level3;";

	run_json_query(input_query_16, json7, result);

	ASSERT_EQ(result, "1");

	const char* input_query_17 = "select count(0) from s3object[*].level1_2;";

	run_json_query(input_query_17, json7, result);

	ASSERT_EQ(result, "1");

	const char* input_query_18 = "select count(0) from s3object[*].level1_2.level2;";

	run_json_query(input_query_18, json7, result);

	ASSERT_EQ(result, "1");

	const char* input_query_19 = "select count(0) from s3object[*].level1_2.level2.level3;";

	run_json_query(input_query_19, json7, result);

	ASSERT_EQ(result, "1");

	const char* input_query_20 = "select count(0) from s3object[*].address;";

	run_json_query(input_query_20, json8, result);

	ASSERT_EQ(result, "1");

	const char* input_query_21 = "select count(0) from s3object[*].phoneNumbers;";

	run_json_query(input_query_21, json8, result);

	ASSERT_EQ(result, "1");

	const char* input_query_22 = "select count(0) from s3object[*].friends;";

	run_json_query(input_query_22, json9, result);

	ASSERT_EQ(result, "18");

	const char* input_query_23 = "select count(0) from s3object[*].tags;";

	run_json_query(input_query_23, json9, result);

	ASSERT_EQ(result, "42");

	const char* input_query_24 = "select count(0) from s3object[*].workers;";

	run_json_query(input_query_24, json10, result);

	ASSERT_EQ(result, "1");

	const char* input_query_25 = "select count(0) from s3object[*].workers.sparta;";

	run_json_query(input_query_25, json10, result);

	ASSERT_EQ(result, "1");

	const char* input_query_26 = "select count(0) from s3object[*].workers.sparta.start_coords;";

	run_json_query(input_query_26, json10, result);

	ASSERT_EQ(result, "2");

	const char* input_query_27 = "select count(0) from s3object[*].workers.jonsnow;";

	run_json_query(input_query_27, json10, result);

	ASSERT_EQ(result, "1");

	const char* input_query_28 = "select count(0) from s3object[*].workers.jonsnow.start_coords;";

	run_json_query(input_query_28, json10, result);

	ASSERT_EQ(result, "2");

	const char* input_query_29 = "select count(0) from s3object[*].address;";

	run_json_query(input_query_29, json11, result);

	ASSERT_EQ(result, "1");

	const char* input_query_30 = "select count(0) from s3object[*].phoneNumbers;";

	run_json_query(input_query_30, json11, result);

	ASSERT_EQ(result, "9");
}

TEST(TestS3selectJsonParser, exact_filter)
{
	std::string result{};

	const char* input_query = "select _1.color from s3object[*].row;";

	std::string expected_result = R"(red
green
blue
cyan
magenta
yellow
black
)";

	run_json_query(input_query, json2, result);

	ASSERT_EQ(result, expected_result);

	const char* input_query_1 = "select _1.hello2 from s3object[*].nested_obj;";

	std::string expected_result_1 = R"(world
)";

	run_json_query(input_query_1, json3, result);

	ASSERT_EQ(result, expected_result_1);

	const char* input_query_2 = "select _1.nested2.c1 from s3object[*].nested_obj;";

	std::string expected_result_2 = R"(c1_value
)";

	run_json_query(input_query_2, json3, result);

	ASSERT_EQ(result, expected_result_2);
}

JsonParserHandler* create_handler(std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp)
{//helper for testing the JSON variable 
  JsonParserHandler* p_handler;
  p_handler =  new (JsonParserHandler);
  std::vector<std::string> pattern;
  p_handler->set_prefix_match(pattern);

  //std::vector <std::vector<std::string>> exact_match_filters;
  //p_handler->set_exact_match_filters(exact_match_filters);

  p_handler->set_exact_match_callback(fp);
  p_handler->set_s3select_processing_callback(f_sql);

  return p_handler;
}

void set_test_0(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,5);

  std::vector<std::string> s3={"addr"};
  array_access.push_variable_state(s3,-1); 

  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);

  // key=phoneNumbers[5].addr
  //handler.set_json_array_access(&array_access);

  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_1(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,0);

  std::vector<std::string> s3={"type"};
  array_access.push_variable_state(s3,-1);

  //key=phoneNumbers[0].type
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);

  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_2(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,11);

  //key=phoneNumbers[11]
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);

  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_3(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"firstName"}; //state-1 : search for key=phoneNumbers
  array_access.push_variable_state(s1,-1);

  //key=firstName
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);

  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_4(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,12);

  s1={};
  array_access.push_variable_state(s1,0);

  //key=phoneNumbers[12][0]
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);
  
  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_5(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,12);

  s1={};
  array_access.push_variable_state(s1,2);

  s1={};
  array_access.push_variable_state(s1,1); 

  //key=phoneNumbers[12][2][1]
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);
  
  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_6(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,12);

  s1={};
  array_access.push_variable_state(s1,3);

  //key=phoneNumbers[12][3]
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);
  
  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_7(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,12);

  s1={};
  array_access.push_variable_state(s1,4);

  s1={"key_in_array"};
  array_access.push_variable_state(s1,-1);

  //key=phoneNumbers[12][4].key_in_array
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);
  
  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_8(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,13);

  s1={"classname"}; 
  array_access.push_variable_state(s1,-1);

  //key=phoneNumbers[13].classname
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);

  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_9(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1={"phoneNumbers"};
  array_access.push_variable_state(s1,-1);

  s1={};
  array_access.push_variable_state(s1,14);

  s1={"associatedDrug"}; 
  array_access.push_variable_state(s1,-1);

  s1={}; 
  array_access.push_variable_state(s1,0);

  s1={"strength"}; 
  array_access.push_variable_state(s1,-1);

  // key=phoneNumbers[14].associatedDrug[0].strength 
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);

  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

void set_test_10(JsonParserHandler& handler,
    std::string& INPUT_TEST_ARRAY,
    std::function<int(void)> f_sql,
    std::function<int(s3selectEngine::value&,int)> fp
    )
{
  int status;
  json_variable_access array_access;

  std::vector<std::string> s1;
  s1={"problems"}; 
  array_access.push_variable_state(s1,-1);
  s1={};
  array_access.push_variable_state(s1,0);

  s1={"Diabetes"}; 
  array_access.push_variable_state(s1,-1);
  s1={};
  array_access.push_variable_state(s1,0);

  s1={"medications"}; 
  array_access.push_variable_state(s1,-1);
  s1={};
  array_access.push_variable_state(s1,0);

  s1={"medicationsClasses"}; 
  array_access.push_variable_state(s1,-1);
  s1={};
  array_access.push_variable_state(s1,0);

  s1={"className"}; 
  array_access.push_variable_state(s1,-1);
  s1={};
  array_access.push_variable_state(s1,0);

  s1={"associatedDrug"}; 
  array_access.push_variable_state(s1,-1);
  s1={};
  array_access.push_variable_state(s1,1);

  s1={"name"}; 
  array_access.push_variable_state(s1,-1);

  //key=problems[0].Diabetes[0].medications[0].medicationsClasses[0].className[0].associatedDrug[0].name = asprin
  std::vector<std::pair<json_variable_access*,size_t>> json_match_expression;
  json_match_expression.push_back(std::pair<json_variable_access*,size_t>(&array_access,1));
  handler.set_statement_json_variables(json_match_expression);

  status = handler.process_json_buffer(INPUT_TEST_ARRAY.data(), INPUT_TEST_ARRAY.size());
  handler.process_json_buffer(0, 0, true);
}

TEST(TestS3selectJsonParser, array_access)
{
  //create JSON input
  //create array_access object with specefic setting (e.g a.b[ 1 ].c)

  std::string INPUT_TEST_ARRAY = R"({
"firstName": "Joe",
"lastName": "Jackson",
"gender": "male",
"age": "twenty",
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},

"firstName": "Joe_2",
"lastName": "Jackson_2",
"gender": "male",
"age": 21,
"address": {
"streetAddress": "101",
"city": "San Diego",
"state": "CA"
},

"phoneNumbers": [
{ "type": "home0", "number": "734928_0", "addr": 0 },
{ "type": "home1", "number": "734928_1", "addr": 11 },
{ "type": "home2", "number": "734928_2", "addr": 22 },
{ "type": "home3", "number": "734928_3", "addr": 33 },
{ "type": "home4", "number": "734928_4", "addr": 44 },
{ "type": "home5", "number": "734928_5", "addr": 55 },
{ "type": "home6", "number": "734928_6", "addr": 66 },
{ "type": "home7", "number": "734928_7", "addr": 77 },
{ "type": "home8", "number": "734928_8", "addr": 88 },
{ "type": "home9", "number": "734928_9", "addr": 99 },
{ "type": "home10", "number": "734928_10", "addr": 100 },
"element-11",
  [ 11 , 22 , 
    [ 44, 55] ,"post 3D" , 
    { 
      "first_key_in_object_in_array" : "value_for_irst_key_in_object_in_array", 
      "key_in_array" : "value_per_key_in_array" 
    } 
  ],
  {"classname" : "stam"},
  { "associatedDrug":[{
                        "name":"asprin",
                        "dose":"",
                        "strength":"500 mg"
                    }],
                    "associatedDrug#2":[{
                        "name":"somethingElse",
                        "dose":"",
                        "strength":"500 mg"
                    }]
}
],
"key_after_array": "XXX"
}
)";

  std::string INPUT_TEST_ARRAY_NEDICATIONS = R"(
{
"problems": [{
    "Diabetes":[{
        "medications":[{
            "medicationsClasses":[{
                "className":[{
                    "associatedDrug":[{
                        "name":"asprin",
                        "dose":"",
                        "strength":"500 mg"
                    },
		    { "name":"acamol" } 
		    ],
                    "associatedDrug2":[{
                        "name":"somethingElse",
                        "dose":"",
                        "strength":"500 mg"
                    }]
                }],
                "className2":[{
                    "associatedDrug":[{
                        "name":"asprin",
                        "dose":"",
                        "strength":"500 mg"
                    }],
                    "associatedDrug2":[{
                        "name":"somethingElse",
                        "dose":"",
                        "strength":"500 mg"
                    }]
                }]
            }]
        }],
        "labs":[{
            "missing_field": "missing_value"
        }]
    }],
    "Asthma":[{}]
}]}
)";


  JsonParserHandler* p_handler;
  std::string result{};
std::function<int(void)> f_sql = [](void){return 0;};

std::function<int(s3selectEngine::value&,int)> fp = [&result](s3selectEngine::value& key_value,int json_idx) {
  result = key_value.to_string();
  return 0;
};

p_handler = create_handler(f_sql,fp);
set_test_0(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"55");

p_handler = create_handler(f_sql,fp);
set_test_1(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"home0");

p_handler = create_handler(f_sql,fp);
set_test_2(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"element-11");

p_handler = create_handler(f_sql,fp);
set_test_3(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"Joe_2");

p_handler = create_handler(f_sql,fp);
set_test_4(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"11");

p_handler = create_handler(f_sql,fp);
set_test_5(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"55");

p_handler = create_handler(f_sql,fp);
set_test_6(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"post 3D");

p_handler = create_handler(f_sql,fp);
set_test_7(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"value_per_key_in_array");

p_handler = create_handler(f_sql,fp);
set_test_8(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"stam");

p_handler = create_handler(f_sql,fp);
set_test_9(*p_handler,INPUT_TEST_ARRAY,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"500 mg");

p_handler = create_handler(f_sql,fp);
set_test_10(*p_handler,INPUT_TEST_ARRAY_NEDICATIONS,f_sql,fp);
std::cout << "RESULT: " << result << std::endl;
ASSERT_EQ(result,"acamol");

}

