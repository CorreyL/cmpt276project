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
// #include "ServerUtils.h"
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
static constexpr const char* push_addr {"http://localhost:34574/"};

const string read_entity_auth {"ReadEntityAuth"};
const string read_entity_admin {"ReadEntityAdmin"};
const string get_update_token_op {"GetUpdateToken"};
const string update_entity_auth {"UpdateEntityAuth"};
const string sign_on {"SignOn"};
const string sign_off {"SignOff"};
const string add_friend {"AddFriend"};
const string unfriend {"UnFriend"};
const string update_status {"UpdateStatus"};
const string read_friend_list {"ReadFriendList"};
const string push_status {"PushStatus"};

// To ensure multiple users can be logged on at once, the UserID will be the key, and the vector will hold the following information in this order:
// vector<string>[0] = (token)
// vector<string>[1] = (partition) (Country)
// vector<string>[2] = (row) (Full Name)
unordered_map< string, vector<string> > active_users = {};

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

pair<status_code,value> push_user_status(const string& partition, const string& row, const string& status, const value& props){
	pair<status_code,value> result { do_request( methods::POST, push_addr + push_status + "/" + partition + "/" + row + "/" + status, props ) };
	return result;
}

void handle_get(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** GET " << path << endl;
  auto paths = uri::split_path(path);
	const string DataTable {"DataTable"};
	
	// paths[0] == ReadFriendList | paths[1] == <UserID>
	if(paths[0]==read_friend_list){
		if( active_users.find(paths[1]) == active_users.end() ){
			message.reply(status_codes::Forbidden);
			return;
		}
		
		pair<status_code,value> read_result {get_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2])};
		
		string current_friends;
		for (const auto& v : read_result.second.as_object()){
			if(v.first == "Friends") current_friends = v.second.as_string();
		}
		
		value props { build_json_object(vector<pair<string,string>> { make_pair(string("Friends"),string(current_friends))})};
		
		if( read_result.first == status_codes::OK ){
			message.reply(status_codes::OK, props); // Needs to be tested for whether or not the JSON property is being passed back properly
			return;
		}
	}
	
	// If the code reaches here, then a Malformed Request was done (eg. paths[0] == "DoSomething")
	message.reply(status_codes::BadRequest);
	return;
}

void handle_put(http_request message){
	string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PUT " << path << endl;
  auto paths = uri::split_path(path);
	const string DataTable {"DataTable"};
	// paths[0] == AddFriend | paths[1] == <UserID> | paths[2] == <Friend's Country> | paths[3] == <<Friend's Last Name>,<Friend's First Name>>
	if(paths[0]==add_friend){
		if( active_users.find(paths[1]) == active_users.end() ){
			message.reply(status_codes::Forbidden);
			return;
		}

		if(paths.size() < 3){ // We require a UserID, Friend Country and Full Friend Name
			message.reply(status_codes::BadRequest);
			return;
		}
		
		pair<status_code,value> check_friends {get_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2])};
		string current_friends;
		string new_friend;
		string check_for_no_friends;
		for (const auto& v : check_friends.second.as_object()){
			if(v.first == "Friends") check_for_no_friends = v.second.as_string();
		}
		if( (check_friends.second.size() == 0) || (check_for_no_friends == "") ){ // User has no friends
			new_friend = paths[2] + ";" + paths[3];
		}
		else{
			for (const auto& v : check_friends.second.as_object()){
				if(v.first == "Friends") current_friends = v.second.as_string();
			}
			new_friend = current_friends + "|" + paths[2] + ";" + paths[3];
		}
		
		string already_exists {paths[2]+";"+paths[3]};
		
		if(current_friends.find(already_exists) != string::npos){
			message.reply(status_codes::OK);
			return;
		}
		
		value props { build_json_object(vector<pair<string,string>> { make_pair(string("Friends"),string(new_friend))})};
		int add_friend_result = put_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2], props);
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
		if( active_users.find(paths[1]) == active_users.end() ){
			message.reply(status_codes::Forbidden);
			return;
		}

		pair<status_code,value> check_friends {get_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2])};
		
		string current_friends;
		string check_for_no_friends;
		string passed_in {paths[2] + ";" + paths[3]};
		for (const auto& v : check_friends.second.as_object()){
			if(v.first == "Friends") check_for_no_friends = v.second.as_string();
		}
		if( (check_friends.second.size() == 0) || (check_for_no_friends == "") ){ // User has no friends
			message.reply(status_codes::OK);
			return;
		}
		else{
			for (const auto& v : check_friends.second.as_object()){
				if( v.first == "Friends") current_friends = v.second.as_string();
			}
		}

		if( current_friends.find(passed_in+"|") != string::npos ){ // This friend is the first entry in the friends list
			current_friends.erase( current_friends.find(passed_in), passed_in.length()+1 );
		}
		else if( current_friends.find("|"+passed_in+"|") != string::npos ){ // This friend is a middle entry in the friends list
			current_friends.erase( current_friends.find(passed_in), passed_in.length()+1 );
		}
		else if (current_friends.find("|"+passed_in) != string::npos ){ // This friend is the last entry in the friends list
			current_friends.erase( current_friends.find(passed_in)-1, passed_in.length()+1 );
		}
		else if(current_friends.find(passed_in) != string::npos){ // This user only has one friend, and that friend is the friend being looked for
			current_friends.erase( current_friends.find(passed_in), passed_in.length() );
		}
		
		value props { build_json_object(vector<pair<string,string>> { make_pair(string("Friends"),string(current_friends))})};
		int unfriend_result = put_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2], props);
		
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
		if( active_users.find(paths[1]) == active_users.end() ){ // User is not signed in
			message.reply(status_codes::Forbidden);
			return;
		}
		
		value status_prop { build_json_object(vector<pair<string,string>> { make_pair(string("Status"),string(paths[2])) } ) };
		
		int status_change_result = put_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2], status_prop);
		
		pair<status_code,value> check_friends {get_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2])};
		
		string current_friends;
		string check_for_no_friends;
		for (const auto& v : check_friends.second.as_object()){
			if(v.first == "Friends") check_for_no_friends = v.second.as_string();
		}
		if( (check_friends.second.size() == 0) || (check_for_no_friends == "") ){ // User has no friends
			message.reply(status_codes::OK);
			return;
		}
		else{
			for (const auto& v : check_friends.second.as_object()){
				if(v.first=="Friends") current_friends = v.second.as_string();
			}
		}
		// cout << "Current friends is: " << current_friends << endl;
		
		// pair<status_code,value> check_status {get_entity_auth(basic_addr, DataTable, active_users[paths[1]][0], active_users[paths[1]][1], active_users[paths[1]][2])};
		
		value props { build_json_object(vector<pair<string,string>> { make_pair(string("Status"),string(paths[2])), make_pair(string("Friends"),string(current_friends) ) } ) };
		
		pair<status_code,value> push_status_result = do_request( methods::POST, push_addr + push_status + "/" + active_users[paths[1]][1] + "/" + active_users[paths[1]][2] + "/" + paths[2], props );
		
		if(push_status_result.first == status_codes::InternalError){
			message.reply(status_codes::ServiceUnavailable);
			return;
		}
		/*
		try{
			// push_user_status(active_users[paths[1]][1], active_users[paths[1]][2], paths[2], props);
			do_request( methods::POST, push_addr + push_status + "/" + active_users[paths[1]][1] + "/" + active_users[paths[1]][2] + "/" + paths[2], props );
		}catch(const web::uri_exception& e){
			message.reply(status_codes::ServiceUnavailable);
			return;
		}
		*/
		message.reply(status_codes::OK);
		return;
	}
	// If the code reaches here, then a Malformed Request was done (eg. paths[0] == "DoSomething")
	message.reply(status_codes::BadRequest);
	return;
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
		string pwd {};
		unordered_map<string,string>::const_iterator got = stored_message.find("Password");
		if( got != stored_message.end() ){
			pwd = got->second;
		}
		pair<status_code,string> auth_result {get_update_token(auth_addr, userID, pwd)};
		if(auth_result.first == status_codes::OK){
			const string DataTable = "DataTable";
			// Begin parsing the partition and row from the token
			string row = auth_result.second.substr(auth_result.second.find("&erk=")+5, auth_result.second.length());
			string partition = auth_result.second.substr(auth_result.second.find("&epk=")+5, auth_result.second.find("&erk="));
			partition.erase(partition.length()-(row.length()+5), partition.length());
			
			// If there's a "," present in the DataRow, then Azure generates a token with "%2C" in place of ",". Need to change "%2C" with ",".
			if( row.find("%2C") != string::npos ){
				row.insert(row.find("%2C"),",");
				string html_comma = "%2C";
				row.erase(row.find("%2C"), html_comma.length());
			}
			/*
			if( partition.find("%2C") != string::npos ){
				partition.insert(partition.find("%2C"),",");
				partition.erase(partition.find("%2C"), partition.find("%2C")+3);
			}
			*/
			pair<status_code,value> data_result {get_entity_auth(basic_addr, DataTable, auth_result.second, partition, row)};
			
			if(data_result.first == status_codes::OK){
				active_users.insert( { paths[1], {auth_result.second, partition, row} } ); // Adding the user to the unordered_map of active users
				message.reply(status_codes::OK);
				return;
			}
			else{ // No record exists in DataTable for this user
				message.reply(status_codes::NotFound);
				return;
			}
		}
		else{
			message.reply(status_codes::NotFound); // AuthServer responded NotFound
			return;
		}
		
	}
	
	if(paths[0] == sign_off){
		if(paths.size() < 2){ // UserID not passed in
			message.reply(status_codes::BadRequest);
			return;
		}
		if( active_users.find(paths[1]) != active_users.end() ){
			active_users.erase(paths[1]);
			message.reply(status_codes::OK);
			return;
		}
		else{
			message.reply(status_codes::NotFound);
			return;
		}
	}
	// If the code reaches here, then a Malformed Request was done (eg. paths[0] == "DoSomething")
	message.reply(status_codes::BadRequest);
	return;
}

int main (int argc, char const * argv[]) {

  http_listener listener {def_url}; // Acknowledges the requests sent to the server; If the below did not exist, it would receive the requests but wouldn't do anything

  cout << "Parsing connection string" << endl;
  // table_cache.init (storage_connection_string);

  cout << "Opening listener" << endl;
  listener.support(methods::GET, &handle_get);
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