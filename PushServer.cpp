#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <was/common.h>
#include <was/table.h>

#include "TableCache.h"
#include "make_unique.h"

#include "azure_keys.h"
#include "ClientUtils.h"

using azure::storage::storage_exception;
using azure::storage::cloud_table;
using azure::storage::cloud_table_client;
using azure::storage::edm_type;
using azure::storage::entity_property;
using azure::storage::table_entity;
using azure::storage::table_operation;
using azure::storage::table_request_options;
using azure::storage::table_result;
using azure::storage::table_shared_access_policy;
// Added these two in
using azure::storage::table_query;
using azure::storage::table_query_iterator;

using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::make_pair;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_vals_t = vector<pair<string,value>>;
using prop_str_vals_t = vector<pair<string,string>>;

const string push_status {"PushStatus"};

constexpr const char* def_url = "http://localhost:34574";

// TableCache table_cache {};

unordered_map<string,string> get_json_body(http_request message) {  
  unordered_map<string,string> results {};
  const http_headers& headers {message.headers()};
  auto content_type (headers.find("Content-Type"));
  if (content_type == headers.end() ||
      content_type->second != "application/json")
    return results;

  value json{};
  message.extract_json(true)
    .then([&json](value v) -> bool
	  {
            json = v;
	    return true;
	  })
    .wait();

  if (json.is_object()) {
    for (const auto& v : json.as_object()) {
      if (v.second.is_string()) {
	results[v.first] = v.second.as_string();
      }
      else {
	results[v.first] = v.second.serialize();
      }
    }
  }
  return results;
}

void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** POST " << path << endl;
  auto paths = uri::split_path(path);
	
	if( paths[0] == push_status ){
		unordered_map<string,string> stored_message = get_json_body(message);
		if(stored_message.size() == 0){ // The user has no friends
			message.reply(status_codes::OK);
			return;
		}
		else{
			unordered_map<string,string>::const_iterator got = stored_message.find("Friends");
			string all_friends {got->second};
			string current_friend { all_friends.substr( 0, all_friends.find("|") ) };
			string current_country {};
			string current_name {};
			while( current_friend != "" ){
				current_country = current_friend.substr( 0, current_friend.find(";") );
				current_name = current_friend.substr( current_friend.find(";"), current_friend.length() );
				
				all_friends.erase( 0, all_friends.find("|") );
				current_friend = all_friends.substr(0, all_friends.find("|"));
			}
		}
		
	}
	
	// If the code reaches here, then a Malformed Request was done (eg. paths[0] == "DoSomething")
	message.reply(status_codes::BadRequest);
	return;
}

int main (int argc, char const * argv[]) {
  cout << "PushServer: Parsing connection string" << endl;
  // table_cache.init (storage_connection_string);

  cout << "PushServer: Opening listener" << endl;
  http_listener listener {def_url};
  //listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  //listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop PushServer." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "PushServer closed" << endl;
}
