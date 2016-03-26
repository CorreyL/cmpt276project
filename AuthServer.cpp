/*
 Authorization Server code for CMPT 276, Spring 2016.
 */

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

constexpr const char* def_url = "http://localhost:34570";

const string auth_table_name {"AuthTable"};
const string auth_table_userid_partition {"Userid"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};
const string data_table_name {"DataTable"};

const string get_read_token_op {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};

/*
  Cache of opened tables
 */
TableCache table_cache {};

/*
  Convert properties represented in Azure Storage type
  to prop_str_vals_t type.
 */
prop_str_vals_t get_string_properties (const table_entity::properties_type& properties) {
  prop_str_vals_t values {};
  for (const auto v : properties) {
    if (v.second.property_type() == edm_type::string) {
      values.push_back(make_pair(v.first,v.second.string_value()));
    }
    else {
      // Force the value as string in any case
      values.push_back(make_pair(v.first, v.second.str()));
    }
  }
  return values;
}

/*
  Given an HTTP message with a JSON body, return the JSON
  body as an unordered map of strings to strings.

  Note that all types of JSON values are returned as strings.
  Use C++ conversion utilities to convert to numbers or dates
  as necessary.
 */
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

/*
  Return a token for 24 hours of access to the specified table,
  for the single entity defind by the partition and row.

  permissions: A bitwise OR ('|')  of table_shared_access_policy::permission
    constants.

    For read-only:
      table_shared_access_policy::permissions::read
    For read and update: 
      table_shared_access_policy::permissions::read |
      table_shared_access_policy::permissions::update
 */
pair<status_code,string> do_get_token (const cloud_table& data_table,
                   const string& partition,
                   const string& row,
                   uint8_t permissions) {

  utility::datetime exptime {utility::datetime::utc_now() + utility::datetime::from_days(1)};
  try {
    string limited_access_token {
      data_table.get_shared_access_signature(table_shared_access_policy {
                                               exptime,
                                               permissions},
                                             string(), // Unnamed policy
                                             // Start of range (inclusive)
                                             partition,
                                             row,
                                             // End of range (inclusive)
                                             partition,
                                             row)
        // Following token allows read access to entire table
        //table.get_shared_access_signature(table_shared_access_policy {exptime, permissions})
      };
    cout << "Token " << limited_access_token << endl;
    return make_pair(status_codes::OK, limited_access_token);
  }
  catch (const storage_exception& e) {
    cout << "Azure Table Storage error: " << e.what() << endl;
    cout << e.result().extended_error().message() << endl;
    return make_pair(status_codes::InternalError, string{});
  }
}

/*
  Top-level routine for processing all HTTP GET requests.
 */
void handle_get(http_request message) { 
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** AuthServer GET " << path << endl;
  auto paths = uri::split_path(path);
  // Need at least an operation and userid
  if (paths.size() < 2) {
    message.reply(status_codes::BadRequest);
    return;
  }
	cloud_table table {table_cache.lookup_table("AuthTable")};
	cloud_table data_table {table_cache.lookup_table("DataTable")};
	
	unordered_map<string,string> json_body {get_json_body (message)};
	// paths[0] = GetReadToken | paths[1] = <table name> | paths[2] = <partition> | paths[3] = <row>
	if( paths[0] == get_read_token_op ){
		table_query query {};
		table_query_iterator end;
		table_query_iterator it = table.execute_query(query);
		if( json_body.size() < 1 ){ // No JSON body passed in
			message.reply( status_codes::BadRequest );
			return;
		}
		if( json_body.size() > 1 ){ // Extra properties passed in
			message.reply( status_codes::BadRequest );
			return;
		}
		while(it != end){ // This while loop iterates through the table until it finds the requested partition
			if( it->partition_key() == auth_table_userid_partition && it->row_key() == paths[1] ){ // Find Partition: Userid && Row: <The Userid passed in>
				const table_entity::properties_type& properties = it->properties();
				for (auto prop_it = properties.begin(); prop_it != properties.end(); ++prop_it) // Cycles through the properties of the current entity
				{
					if(prop_it->first == "Password"){
						//cout << ", " << prop_it->first << ": " << prop_it->second.str() << endl;
						unordered_map<string,string>::const_iterator got = json_body.find(prop_it->first); // Looking for the property Password in the JSON Body
						if( got == json_body.end() ){ // The property "Password" was not found in the JSON body
							message.reply( status_codes::BadRequest );
							return;
						}
						bool cond1 {false};
						bool cond2 {false};
						string partition;
						string row;
						if( got->second == prop_it->second.str() ){ // The password passed in from the JSON Body matched the password in this entity in AuthTable
							for (auto prop_it2 = properties.begin(); prop_it2 != properties.end(); ++prop_it2){
								if(prop_it2->first == "DataPartition"){
									partition = prop_it2->second.str();
									cond1 = true;
								}
								if(prop_it2->first == "DataRow"){
									row = prop_it2->second.str();
									cond2 = true;
								}
							}
							if(cond1 == true && cond2 == true){ // The Partition and Row this Userid/Password combination allows for are found in the Properties of the entity in AuthTable
								pair<status_code,string> result = do_get_token (data_table, partition, row, table_shared_access_policy::permissions::read);
								if(result.first == status_codes::InternalError){
									message.reply( status_codes::InternalError );
									return;
								}
								else if(result.first == status_codes::OK){
									prop_vals_t keys { make_pair("Password",value::string(result.second)) };
									vector<value> key_vec;
									key_vec.push_back(value::object(keys));
									message.reply( status_codes::OK, value::array(key_vec) );
									return;
								}
							}
							else{ // The DataPartition or DataRow passed in was not found in DataTable
								message.reply( status_codes::BadRequest );
								return;
							}
						}
						else{
							message.reply( status_codes::NotFound ); // The password does not match the Userid
							return;
						}
					}
				}
			}
			++it;
		}
		message.reply( status_codes::NotFound ); // Userid was not found
		return;
	}
	
	if( paths[0] == get_update_token_op ){
		table_query query {};
		table_query_iterator end;
		table_query_iterator it = table.execute_query(query);
		if( json_body.size() < 1 ){ // No JSON body passed in
			message.reply( status_codes::BadRequest );
			return;
		}
		if( json_body.size() > 1 ){ // Extra properties passed in
			message.reply( status_codes::BadRequest );
			return;
		}
		while(it != end){ // This while loop iterates through the table until it finds the requested partition
			if( it->partition_key() == auth_table_userid_partition && it->row_key() == paths[1] ){ // Find Partition: Userid && Row: <The Userid passed in>
				const table_entity::properties_type& properties = it->properties();
				for (auto prop_it = properties.begin(); prop_it != properties.end(); ++prop_it) // Cycles through the properties of the current entity
				{
					if(prop_it->first == "Password"){
						//cout << ", " << prop_it->first << ": " << prop_it->second.str() << endl;
						unordered_map<string,string>::const_iterator got = json_body.find(prop_it->first); // Looking for the property Password in the JSON Body
						if( got == json_body.end() ){ // The property "Password" was not found in the JSON body
							message.reply( status_codes::BadRequest );
							return;
						}
						bool cond1 {false};
						bool cond2 {false};
						string partition;
						string row;
						if( got->second == prop_it->second.str() ){ // The password passed in from the JSON Body matched the password in this entity in AuthTable
							for (auto prop_it2 = properties.begin(); prop_it2 != properties.end(); ++prop_it2){
								if(prop_it2->first == "DataPartition"){
									partition = prop_it2->second.str();
									cond1 = true;
								}
								if(prop_it2->first == "DataRow"){
									row = prop_it2->second.str();
									cond2 = true;
								}
							}
							if(cond1 == true && cond2 == true){ // The Partition and Row this Userid/Password combination allows for are found in the Properties of the entity in AuthTable
								pair<status_code,string> result = do_get_token (data_table, partition, row, table_shared_access_policy::permissions::read | table_shared_access_policy::permissions::update);
								if(result.first == status_codes::InternalError){
									message.reply( status_codes::InternalError );
									return;
								}
								else if(result.first == status_codes::OK){
									prop_vals_t keys { make_pair("Password",value::string(result.second)) };
									vector<value> key_vec;
									key_vec.push_back(value::object(keys));
									message.reply( status_codes::OK, value::array(key_vec) );
									return;
								}
							}
							else{ // The DataPartition or DataRow passed in was not found in DataTable
								message.reply( status_codes::BadRequest );
								return;
							}
						}
						else{
							message.reply( status_codes::NotFound ); // The password does not match the Userid
							return;
						}
					}
				}
			}
			++it;
		}
		message.reply( status_codes::NotFound ); // Userid was not found
		return;
  //message.reply(status_codes::NotImplemented);
	}
}

/*
  Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** POST " << path << endl;
}

/*
  Top-level routine for processing all HTTP PUT requests.
 */
void handle_put(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PUT " << path << endl;
}

/*
  Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** DELETE " << path << endl;
}

/*
  Main authentication server routine

  Install handlers for the HTTP requests and open the listener,
  which processes each request asynchronously.

  Note that, unlike BasicServer, AuthServer only
  installs the listeners for GET. Any other HTTP
  method will produce a Method Not Allowed (405)
  response.

  If you want to support other methods, uncomment
  the call below that hooks in a the appropriate 
  listener.
  
  Wait for a carriage return, then shut the server down.
 */
int main (int argc, char const * argv[]) {
  cout << "AuthServer: Parsing connection string" << endl;
  table_cache.init (storage_connection_string);

  cout << "AuthServer: Opening listener" << endl;
  http_listener listener {def_url};
  listener.support(methods::GET, &handle_get);
  //listener.support(methods::POST, &handle_post);
  //listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop AuthServer." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "AuthServer closed" << endl;
}
