#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cpprest/base_uri.h>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <pplx/pplxtasks.h>

#include <was/common.h>
#include <was/storage_account.h>
#include <was/table.h>

#include "TableCache.h"
#include "make_unique.h"

#include "azure_keys.h"
#include "ServerUtils.h"
#include "ClientUtils.h"

using pplx::extensibility::critical_section_t;
using pplx::extensibility::scoped_critical_section_t;

using std::cin;
using std::cerr;
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

constexpr const char* def_url = "http://localhost:34572";

static constexpr const char* auth_addr {"http://localhost:34570/"};
static constexpr const char* basic_addr {"http://localhost:34568/"};

const string read_entity_auth {"ReadEntityAuth"};
const string get_update_token_op {"GetUpdateToken"};
const string update_entity_auth {"UpdateEntityAuth"};
const string sign_on {"SignOn"};
const string sign_off {"SignOff"};
const string add_friend {"AddFriend"};
const string unfriend {"UnFriend"};
const string update_status {"UpdateStatus"};

class active_user{
	public:
		bool active {false};
		string token {};
		string partition {};
		string row {};
};

active_user active_user; // Does it need to be rebuilt so that multiple users can be on at once?
/*
  Return true if an HTTP request has a JSON body

  This routine can be called multiple times on the same message.
 */
bool has_json_body (http_request message) {
  return message.headers()["Content-type"] == "application/json";
}

/*
  Given an HTTP message with a JSON body, return the JSON
  body as an unordered map of strings to strings.

  If the message has no JSON body, return an empty map.

  THIS ROUTINE CAN ONLY BE CALLED ONCE FOR A GIVEN MESSAGE
  (see http://microsoft.github.io/cpprestsdk/classweb_1_1http_1_1http__request.html#ae6c3d7532fe943de75dcc0445456cbc7
  for source of this limit).

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
  Utility to create JSON object value from vector of properties
*/
value build_json_object (const vector<pair<string,string>>& properties) {
    value result {value::object ()};
    for (auto& prop : properties) {
      result[prop.first] = value::string(prop.second);
    }
    return result;
}

/*
  Utility to get a token good for updating a specific entry
  from a specific table for one day.
 */
pair<status_code,string> get_update_token(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_update_token_op + "/" +
                                              userid,
                                              pwd
                                              )};
  cerr << "token " << result.second << endl;
  if (result.first != status_codes::OK)
    return make_pair (result.first, "");
  else {
    string token {result.second["token"].as_string()};
    return make_pair (result.first, token);
  }
}

pair<status_code,value> get_entity_auth (const string& addr, const string& table, const string& tok, const string& partition, const string&row){
  pair<status_code,value> result {
    do_request(methods::GET,
      addr + read_entity_auth + "/" + table + "/" + tok + "/" + partition + "/" + row)};
  return result;
}

int put_entity_auth (const string& addr, const string& table, const string& tok, const string& partition, const string& row, const value& props){
  pair<status_code,value> result {
    do_request(methods::PUT,
      addr + update_entity_auth + "/" + table + "/" + tok + "/" + partition + "/" + row, props)};
  return result.first;
}

void handle_put(http_request message){
	string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PUT " << path << endl;
  auto paths = uri::split_path(path);
	
	if(paths[0]==add_friend){
		if(!active_user.active){
			message.reply(status_codes::Forbidden);
			return;
		}
		
		if(paths.size() < 3){ // We require a UserID, Friend Country and Full Friend Name
			message.reply(status_codes::BadRequest);
			return;
		}
		pair<status_code,value> check_friends {get_entity_auth(basic_addr, "DataTable", active_user.token, active_user.partition, active_user.row)};
		string new_friend;
		string check_for_no_friends;
		for (const auto& v : check_friends.second.as_object()){
			check_for_no_friends = v.second.as_string();
		}
		if( (check_friends.second.size() == 0) || (check_for_no_friends == "") ){ // User has no friends
			new_friend = paths[2] + ";" + paths[3];
		}
		else{
			string current_friends;
			for (const auto& v : check_friends.second.as_object()){
				current_friends = v.second.as_string();
			}
			new_friend = current_friends + "|" + paths[2] + ";" + paths[3];
		}
		
		value props { build_json_object(vector<pair<string,string>> { make_pair(string("Friends"),string(new_friend))})};
		int add_friend_result = put_entity_auth(basic_addr, "DataTable", active_user.token, active_user.partition, active_user.row, props);
		if(add_friend_result == status_codes::OK){
			message.reply(status_codes::OK);
			return;
		}
		else{
			message.reply(add_friend_result);
			return;
		}
	}
	// paths[0] == UnFriend | paths[1] == <UserID> | paths[2] == <Country> | paths[3] == <Last Name, First Name>
	// "USA;Shinoda,Mike|Canada;Edwards,Kathleen|Korea;Bae,Doona"
	if(paths[0]==unfriend){
		if(!active_user.active){ // User is not signed in
			message.reply(status_codes::Forbidden);
			return;
		}
		
		pair<status_code,value> check_friends {get_entity_auth(basic_addr, "DataTable", active_user.token, active_user.partition, active_user.row)};
		
		string current_friends;
		string check_for_no_friends;
		for (const auto& v : check_friends.second.as_object()){
			check_for_no_friends = v.second.as_string();
		}
		if( (check_friends.second.size() == 0) || (check_for_no_friends == "") ){ // User has no friends
			message.reply(status_codes::OK);
			return;
		}
		else{
			for (const auto& v : check_friends.second.as_object()){
				current_friends = v.second.as_string();
			}
		}
		if(current_friends.find("|") == string::npos){ // This user only has one friend
			string passed_in {paths[2] + ";" + paths[3]};
			// cout << "Passed In Friend Is: " << passed_in << endl;
			current_friends.erase( current_friends.find(passed_in), passed_in.length() );
		}
		else{
			string passed_in {paths[2] + ";" + paths[3] + "|"};
			current_friends.erase( current_friends.find(passed_in), passed_in.length() );
		}
		
		value props { build_json_object(vector<pair<string,string>> { make_pair(string("Friends"),string(current_friends))})};
		int unfriend_result = put_entity_auth(basic_addr, "DataTable", active_user.token, active_user.partition, active_user.row, props);
		
		if(unfriend_result == status_codes::OK){
			message.reply(status_codes::OK);
			return;
		}
		else{
			message.reply(unfriend_result);
			return;
		}
	}
	// paths[0] == UpdateStatus | paths[1] == <UserID> | paths[2] == <User Status>
	if(paths[0]==update_status){
		if(!active_user.active){ // User is not signed in
			message.reply(status_codes::Forbidden);
			return;
		}
		
		value props { build_json_object(vector<pair<string,string>> { make_pair(string("Status"),string(paths[2]))}) };
		
		pair<status_code,value> check_status {get_entity_auth(basic_addr, "DataTable", active_user.token, active_user.partition, active_user.row)};
		
		int status_change_result = put_entity_auth(basic_addr, "DataTable", active_user.token, active_user.partition, active_user.row, props);
		
		// Operations for the Push Server goes here; update all friends of your status
		
		message.reply(status_codes::OK);
		return;
	}
}

void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** POST " << path << endl;
  auto paths = uri::split_path(path);
			
	if(paths[0] == sign_on){
		if(paths.size() < 2){ // UserID not passed in
			message.reply(status_codes::BadRequest);
			return;
		}
		
		unordered_map<string,string> stored_message = get_json_body(message);
		
		if(stored_message.size() == 0){ // No password given
			message.reply(status_codes::NotFound);
			return;
		}
		const string userID {paths[1]};
		unordered_map<string,string>::const_iterator got = stored_message.find("Password");
		const string pwd {got->second};
		pair<status_code,string> auth_result {get_update_token(auth_addr, userID, pwd)};
		if(auth_result.first == status_codes::OK){
			const string DataTable = "DataTable";
			// Begin parsing the partition and row from the token
			const string row = auth_result.second.substr(auth_result.second.find("&erk=")+5, auth_result.second.length());
			string partition = auth_result.second.substr(auth_result.second.find("&epk=")+5, auth_result.second.find("&erk="));
			partition.erase(partition.length()-(row.length()+5), partition.length());
			pair<status_code,value> data_result {get_entity_auth(basic_addr, DataTable, auth_result.second, partition, row)};
			if(data_result.first == status_codes::OK){
				active_user.active = true;
				active_user.token = auth_result.second;
				active_user.partition = partition;
				active_user.row = row;
				message.reply(status_codes::OK);
				return;
			}
			else{
				message.reply(status_codes::NotFound);
				return;
			}
		}
		else{
			message.reply(status_codes::NotFound);
			return;
		}
		
	}
	
	if(paths[0] == sign_off){
		if(paths.size() < 2){ // UserID not passed in
			message.reply(status_codes::BadRequest);
			return;
		}
		if(active_user.active){
			active_user.active = false;
			message.reply(status_codes::OK);
			return;
		}
		else{
			message.reply(status_codes::NotFound);
			return;
		}
		
	}
}

int main (int argc, char const * argv[]) {

  http_listener listener {def_url}; // Acknowledges the requests sent to the server; If the below did not exist, it would receive the requests but wouldn't do anything

  cout << "Parsing connection string" << endl;
  // table_cache.init (storage_connection_string);

  cout << "Opening listener" << endl;
  //listener.support(methods::GET, &handle_get);
  listener.support(methods::POST, &handle_post);
  listener.support(methods::PUT, &handle_put);
  //listener.support(methods::DEL, &handle_delete);
  listener.open().wait(); // Wait for listener to complete starting

  cout << "Enter carriage return to stop server." << endl;
  string line;
  getline(std::cin, line);

  // Shut it down
  listener.close().wait();
  cout << "Closed" << endl;
}