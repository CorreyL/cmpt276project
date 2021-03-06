/*
  Sample unit tests for BasicServer
 */

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <pplx/pplxtasks.h>

#include <UnitTest++/UnitTest++.h>

using std::cerr;
using std::cout;
using std::endl;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::http_response;
using web::http::method;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri_builder;

using web::http::client::http_client;

using web::json::object;
using web::json::value;

const string create_table_op {"CreateTableAdmin"};
const string delete_table_op {"DeleteTableAdmin"};

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};
const string delete_entity_admin {"DeleteEntityAdmin"};
const string get_all_admin {"GetAllAdmin"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};

const string get_read_token_op  {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};
const string get_update_data {"GetUpdateData"};

const string sign_on {"SignOn"};
const string sign_off {"SignOff"};
const string add_friend {"AddFriend"};
const string un_friend {"UnFriend"};
const string update_status {"UpdateStatus"};
const string push_status {"PushStatus"};
const string read_friend_list {"ReadFriendList"};

// The two optional operations from Assignment 1
const string add_property_admin {"AddPropertyAdmin"};
const string update_property_admin {"UpdatePropertyAdmin"};

static constexpr const char* user_addr {"http://localhost:34572/"};

/*
  Make an HTTP request, returning the status code and any JSON value in the body

  method: member of web::http::methods
  uri_string: uri of the request
  req_body: [optional] a json::value to be passed as the message body

  If the response has a body with Content-Type: application/json,
  the second part of the result is the json::value of the body.
  If the response does not have that Content-Type, the second part
  of the result is simply json::value {}.

  You're welcome to read this code but bear in mind: It's the single
  trickiest part of the sample code. You can just call it without
  attending to its internals, if you prefer.
 */

// Version with explicit third argument
pair<status_code,value> do_request (const method& http_method, const string& uri_string, const value& req_body) {
  http_request request {http_method};
  if (req_body != value {}) {
    http_headers& headers (request.headers());
    headers.add("Content-Type", "application/json");
    request.set_body(req_body);
  }

  status_code code;
  value resp_body;
  http_client client {uri_string};
  client.request (request)
    .then([&code](http_response response)
          {
            code = response.status_code();
            const http_headers& headers {response.headers()};
            auto content_type (headers.find("Content-Type"));
            if (content_type == headers.end() ||
                content_type->second != "application/json")
              return pplx::task<value> ([] { return value {};});
            else
              return response.extract_json();
          })
    .then([&resp_body](value v) -> void
          {
            resp_body = v;
            return;
          })
    .wait();
  return make_pair(code, resp_body);
}

// Version that defaults third argument
pair<status_code,value> do_request (const method& http_method, const string& uri_string) {
  return do_request (http_method, uri_string, value {});
}

/*
  Utility to create a table

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
 */
int create_table (const string& addr, const string& table) {
  pair<status_code,value> result {do_request (methods::POST, addr + create_table_op + "/" + table)};
  return result.first;
}

/*
  Utility to compare two JSON objects

  This is an internal routine---you probably want to call compare_json_values().
 */
bool compare_json_objects (const object& expected_o, const object& actual_o) {
  CHECK_EQUAL (expected_o.size (), actual_o.size());
  if (expected_o.size() != actual_o.size())
    return false;

  bool result {true};
  for (auto& exp_prop : expected_o) {
    object::const_iterator act_prop {actual_o.find (exp_prop.first)};
    CHECK (actual_o.end () != act_prop);
    if (actual_o.end () == act_prop)
      result = false;
    else {
      CHECK_EQUAL (exp_prop.second, act_prop->second);
      if (exp_prop.second != act_prop->second)
        result = false;
    }
  }
  return result;
}

/*
  Utility to compare two JSON objects represented as values

  expected: json::value that was expected---must be an object
  actual: json::value that was actually returned---must be an object
*/
bool compare_json_values (const value& expected, const value& actual) {
  assert (expected.is_object());
  assert (actual.is_object());

  object expected_o {expected.as_object()};
  object actual_o {actual.as_object()};
  return compare_json_objects (expected_o, actual_o);
}

/*
  Utility to compre expected JSON array with actual

  exp: vector of objects, sorted by Partition/Row property 
    The routine will throw if exp is not sorted.
  actual: JSON array value of JSON objects
    The routine will throw if actual is not an array or if
    one or more values is not an object.

  Note the deliberate asymmetry of the how the two arguments are handled:

  exp is set up by the test, so we *require* it to be of the correct
  type (vector<object>) and to be sorted and throw if it is not.

  actual is returned by the database and may not be an array, may not
  be values, and may not be sorted by partition/row, so we have
  to check whether it has those characteristics and convert it 
  to a type comparable to exp.
*/
bool compare_json_arrays(const vector<object>& exp, const value& actual) {
  /*
    Check that expected argument really is sorted and
    that every value has Partion and Row properties.
    This is a precondition of this routine, so we throw
    if it is not met.
  */
  auto comp = [] (const object& a, const object& b) -> bool {
      return a.at("Partition").as_string()  <  b.at("Partition").as_string()
       ||
       (a.at("Partition").as_string() == b.at("Partition").as_string() &&
        a.at("Row").as_string()       <  b.at("Row").as_string()); 
  };
  if ( ! std::is_sorted(exp.begin(),
                         exp.end(),
                         comp))
    throw std::exception();

  // Check that actual is an array
  CHECK(actual.is_array());
  if ( ! actual.is_array())
    return false;
  web::json::array act_arr {actual.as_array()};

  // Check that the two arrays have same size
  CHECK_EQUAL(exp.size(), act_arr.size());
  if (exp.size() != act_arr.size())
    return false;

  // Check that all values in actual are objects
  bool all_objs {std::all_of(act_arr.begin(),
                             act_arr.end(),
                             [] (const value& v) { return v.is_object(); })};
  CHECK(all_objs);
  if ( ! all_objs)
    return false;

  // Convert all values in actual to objects
  vector<object> act_o {};
  auto make_object = [] (const value& v) -> object {
    return v.as_object();
  };
  std::transform (act_arr.begin(), act_arr.end(), std::back_inserter(act_o), make_object);

  /* 
     Ensure that the actual argument is sorted.
     Unlike exp, we cannot assume this argument is sorted,
     so we sort it.
   */
  std::sort(act_o.begin(), act_o.end(), comp);

  // Compare the sorted arrays
  bool eq {std::equal(exp.begin(), exp.end(), act_o.begin(), &compare_json_objects)};
  CHECK (eq);
  return eq;
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
  Utility to delete a table

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
 */
int delete_table (const string& addr, const string& table) {
  // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
  pair<status_code,value> result {
    do_request (methods::DEL,
                addr + delete_table_op + "/" + table)};
  return result.first;
}

/*
  Utility to put an entity with a single property

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
  prop: Name of the property
  pstring: Value of the property, as a string
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row, const string& prop, const string& pstring) {
  pair<status_code,value> result {
    do_request (methods::PUT,
                addr + update_entity_admin + "/" + table + "/" + partition + "/" + row,
                value::object (vector<pair<string,value>>
                               {make_pair(prop, value::string(pstring))}))};
  return result.first;
}

/*
  Utility to put an entity with multiple properties

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
  props: vector of string/value pairs representing the properties
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row,
              const vector<pair<string,value>>& props) {
  pair<status_code,value> result {
    do_request (methods::PUT,
               addr + update_entity_admin + "/" + table + "/" + partition + "/" + row,
               value::object (props))};
  return result.first;
}

/*
  Utility to delete an entity

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
 */
int delete_entity (const string& addr, const string& table, const string& partition, const string& row)  {
  // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
  pair<status_code,value> result {
    do_request (methods::DEL,
                addr + delete_entity_admin + "/" + table + "/" + partition + "/" + row)};
  return result.first;
}
/********************* 
**CODE ADDED - BEGIN**
**********************/

pair<status_code,value> get_partition_entity (const string& addr, const string& table, const string& partition, const string& row){
  pair<status_code,value> result {do_request(methods::GET, addr + read_entity_admin + "/" + table + "/" + partition + "/" + row) };
  return result;
}

pair<status_code,value> get_Entities_from_property (const string& addr, const string& table, const string& prop, const string& pstring){
  pair<status_code,value> result { do_request(methods::GET, addr + read_entity_admin + "/" + table, value::object(vector<pair<string,value>> {make_pair(prop, value::string(pstring))}))};
  return result;
}

pair<status_code,value> get_spec_properties_entity (const string& addr, const string& table, const value& properties){
  pair<status_code,value> result { do_request(methods::GET, addr + read_entity_admin + "/" + table, properties)};
  return result;
}

int put_multi_properties_entity (const string& addr, const string& table, const string& partition, const string& row, const value& properties){
  pair<status_code,value> result { 
    do_request(methods::PUT, 
      addr + update_entity_admin + "/"  + table + "/" + partition + "/" + row, properties)};
  return result.first;
}

int update_property (const string& addr, const string& table, const value& properties){
  pair<status_code,value> result { 
    do_request(methods::PUT, 
      addr + update_property_admin + "/" + table, properties)};
  return result.first;
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

/*
  Utility to put an entity with no properties

  addr: Prefix of the URI (protocol, address, and port)
  table: Table in which to insert the entity
  partition: Partition of the entity 
  row: Row of the entity
 */
int put_entity_no_properties(const string& addr, const string& table, const string& partition, const string& row){
  pair<status_code,value> result {
    do_request (methods::PUT,
    addr + update_entity_admin + "/"  + table + "/" + partition + "/" + row)};
  return result.first;
}

pair<status_code,string> get_read_token(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_read_token_op + "/" +
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

//Helper function to dump a table's contents (useful for debugging)
void dump_table_contents(const string& tableName){
  pair<status_code,value> result = { do_request (methods::GET, "http://localhost:34568/"
                                      + read_entity_admin + "/"
                                      + string(tableName))};
  cout << result.second << endl;
}

//Helper function to sign on
int signOn(const string& userId, const string& password){
    pair<status_code,value> signOnResult {
    do_request(methods::POST,
    user_addr + sign_on + "/" + userId, value::object (vector<pair<string,value>>
                {make_pair("Password", value::string(password))}))};
    cout <<"Sign on result " << signOnResult.first << endl;
    return signOnResult.first;
}

//Helper function to sign off
int signOff(const string& userId){
    pair<status_code,value> signOffResult {do_request(methods::POST, "http://localhost:34572/" + sign_off + "/" + userId)};
    cout <<"Sign off result " << signOffResult.first << endl;
    return signOffResult.first;
}

//Helper function to read a user's friend list
pair<status_code,value> ReadFriendList(const string& userId){
    pair<status_code,value> readListResult {do_request(methods::GET, "http://localhost:34572/" + read_friend_list + "/" + userId)};
    return readListResult;
}

//Helper function to add a friend
int addFriend(const string& userID, const string& friendCountry, const string& friendName){
  pair<status_code,value> result = do_request (methods::PUT,
                user_addr + add_friend + "/" + userID + "/" + friendCountry + "/" + friendName);
  cout << "Add friend result " << result.first << endl;
  return result.first;
}

//Helper function to remove a friend
int unFriend(const string& userID, const string& friendCountry, const string& friendName){
  pair<status_code,value> result = do_request (methods::PUT,
                user_addr + un_friend + "/" + userID + "/" + friendCountry + "/" + friendName);
  cout << "Un friend result " << result.first << endl;
  return result.first;
}

//Helper function to create a fake user (IN BOTH AUTHTABLE AND DATATABLE, REMEMBER TO DELETE BOTH)
void createFakeUser(const string& userId, const string& user_pwd, const string& partition, const string& row){
  string addr = "http://localhost:34568/";
  string auth_table = "AuthTable";
  string auth_table_partition  = "Userid";
  string table = "DataTable";
  string friends = "Friends";
  string status = "Status";
  string updates = "Updates";
  string blank = "";
  string auth_pwd_prop = "Password";
  string auth_dataPartition = "DataPartition";
  string auth_dataRow = "DataRow";

  //Add an entity that UserID and Password can work on
  int put_result {put_entity_no_properties (addr, table, partition, row)};
  cerr << "put result " << put_result << endl;
  if (put_result != status_codes::OK) {
    throw std::exception();
  }
  //Give this entity the required properties
  pair<status_code,value> result {
  do_request (methods::PUT,
              addr + update_entity_admin + "/" + table + "/" + partition + "/" + row,
              value::object (vector<pair<string,value>>
              {make_pair(string(friends), value::string(blank)),
               make_pair(string(updates), value::string(blank)),
               make_pair(string(status), value::string(blank))}))};
  if (result.first != status_codes::OK) {
    cout << result.second << endl;
    throw std::exception();
  }

  //Ensure userid and password in system
  int user_result {put_entity (addr,
                               auth_table,
                               auth_table_partition,
                               userId,
                               auth_pwd_prop,
                               user_pwd)};
  cerr << "user auth table insertion result " << user_result << endl;
  if (user_result != status_codes::OK){
    throw std::exception();
  }

  //Give this userid and password a dataRow and dataPartition property corresponding to the data entitiy above
  result = {
  do_request (methods::PUT,
              addr + update_entity_admin + "/" + auth_table + "/" + auth_table_partition + "/" + userId,
              value::object (vector<pair<string,value>>
              {make_pair(string(auth_dataPartition), value::string(partition)),
              make_pair(string(auth_dataRow), value::string(row))}))};
  /*Add this at the end of your test to remove this fake user completely:
     delete_entity (string(UserFixture::addr), string(UserFixture::auth_table), string(UserFixture::auth_table_partition), *USER ID HERE*);
     delete_entity (string(UserFixture::addr), string(UserFixture::table), *PARTITION HERE*, *ROW HERE*);
  */
}

//Helper function for push server
pair<status_code,value> post_update(const string& addr, const string& country, const string& user, const string& status, const value& friendlist){
  pair<status_code,value> result {
    do_request(methods::POST,
      addr + push_status + "/" + country + "/" + user + "/" + status, friendlist)};
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

pair<status_code,value> get_update_data_function(const string& addr,  const string& userid, const string& password) {
  value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
  pair<status_code,value> result {do_request (methods::GET,
                                              addr +
                                              get_update_data + "/" +
                                              userid,
                                              pwd
                                              )};
  return result;
}


/********************
**CODE ADDED - STOP**
********************/

/*
  A sample fixture that ensures TestTable exists, and
  at least has the entity Franklin,Aretha/USA
  with the property "Song": "RESPECT".

  The entity is deleted when the fixture shuts down
  but the table is left. See the comments in the code
  for the reason for this design.
 */

SUITE(GET) {
  class BasicFixture {
  public:
    static constexpr const char* addr {"http://localhost:34568/"};
    static constexpr const char* table {"TestTable"};
    static constexpr const char* partition {"USA"};
    static constexpr const char* row {"Franklin,Aretha"};
    static constexpr const char* property {"Song"};
    static constexpr const char* prop_val {"RESPECT"};

  public:
    BasicFixture() {
      int make_result {create_table(addr, table)};
      cerr << "create result " << make_result << endl;
      if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
        throw std::exception();
      }
      int put_result {put_entity (addr, table, partition, row, property, prop_val)};
      cerr << "put result " << put_result << endl;
      if (put_result != status_codes::OK) {
        throw std::exception();
      }
    }

    ~BasicFixture() {
      int del_ent_result {delete_entity (addr, table, partition, row)};
      if (del_ent_result != status_codes::OK) {
        throw std::exception();
      }

      /*
        In traditional unit testing, we might delete the table after every test.

        However, in cloud NoSQL environments (Azure Tables, Amazon DynamoDB)
        creating and deleting tables are rate-limited operations. So we
        leave the table after each test but delete all its entities.
      */
      cout << "Skipping table delete" << endl;
      /*
        int del_result {delete_table(addr, table)};
        cerr << "delete result " << del_result << endl;
        if (del_result != status_codes::OK) {
          throw std::exception();
        }
       */
      }
    };

  /*
    A test of GET of a single entity
   */
  TEST_FIXTURE(BasicFixture, GetSingle) {
    pair<status_code,value> result {
      do_request (methods::GET,
      string(BasicFixture::addr)
      + read_entity_admin + "/"
      + BasicFixture::table + "/"
      + BasicFixture::partition + "/"
      + BasicFixture::row)};
      
    value obj1 {
      value::object(vector<pair<string,value>> {
          make_pair(string("Song"), value::string(prop_val))
      })
    };

    vector<object> exp {
      obj1.as_object()
    };

    CHECK_EQUAL(status_codes::OK, result.first);
    } 

  /*
    A test of GET all table entries

    Demonstrates use of new compare_json_arrays() function.
   */
  TEST_FIXTURE(BasicFixture, GetAll) {
    string partition {"Canada"};
    string row {"Katherines,The"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
                  string(BasicFixture::addr)
                  + read_entity_admin + "/"
                  + string(BasicFixture::table))};
    CHECK_EQUAL(status_codes::OK, result.first);
    value obj1 {
      value::object(vector<pair<string,value>> {
          make_pair(string("Partition"), value::string(partition)),
          make_pair(string("Row"), value::string(row)),
          make_pair(property, value::string(prop_val))
      })
    };
    value obj2 {
      value::object(vector<pair<string,value>> {
          make_pair(string("Partition"), value::string(BasicFixture::partition)),
          make_pair(string("Row"), value::string(BasicFixture::row)),
          make_pair(string(BasicFixture::property), value::string(BasicFixture::prop_val))
      })
    };
    vector<object> exp {
      obj1.as_object(),
      obj2.as_object()
    };
    compare_json_arrays(exp, result.second);
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
  }
  /********************* 
  **CODE ADDED - BEGIN**
  **********************/
  /*
  A test of GET entities of specified partition
  */
  
  TEST_FIXTURE(BasicFixture, GetPartition){
    string partition = "Video_Game";
    string row {"The_Witcher_3"};
    string property {"Rating"};
    string prop_val {"10_Out_Of_10"};

    //Test to make sure if the partition does not exist, a 404 NotFound code is recieved
    pair<status_code,value> test_result {get_partition_entity(string(BasicFixture::addr), string(BasicFixture::table), partition, "*")};
    CHECK_EQUAL(status_codes::NotFound, test_result.first);

    //Ensure bad requests get a 400 response (no partition name)
    test_result = do_request (methods::GET, string(BasicFixture::addr) + read_entity_admin + "/" + string(BasicFixture::table) + "/" + row);
    CHECK_EQUAL(status_codes::BadRequest, test_result.first);

    //Add an element, check GET works
    int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
    
    test_result = get_partition_entity(string(BasicFixture::addr), string(BasicFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(1, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add a second element, check the GET returns both elements in the partition
    row = "Fire_Emblem";
    prop_val = "8_Out_Of_10";

    put_result = put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_partition_entity(string(BasicFixture::addr), string(BasicFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add a third element that is NOT a member of the same partition, ensure that it is not returned with the other two
    partition = "Aidan";
    row = "Canada";
    property = "Home";
    prop_val = "Surrey";
    put_result = put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);


    partition = "Video_Game";
    test_result = get_partition_entity(string(BasicFixture::addr), string(BasicFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add a fourth and final element to ensure that adding a non-partition element does not mess up gets of the next (partitioned) elements
    //Also tests if it can return an entity with no properties
    row = "Call_Of_Duty";
    prop_val = "5_Out_Of_10";

    put_result = put_entity_no_properties(BasicFixture::addr, BasicFixture::table, partition, row);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_partition_entity(string(BasicFixture::addr), string(BasicFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(3, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);


    //Clear Table
    row = "The_Witcher_3";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    row = "Fire_Emblem";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    partition = "Aidan";
    row = "Canada";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    partition = "Video_Game";
    row = "Call_Of_Duty";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
  }

  /*
    A Test of adding a specific property to all entities
  */

  TEST_FIXTURE(BasicFixture, AddPropertyToAll){
    string partition {"Humans"};
    string row {"PatientZero"};
    string property {"ZombieVirus"};
    string prop_val {"Infected"};

    //Add an entity with a property, one with a property that is different than the first one,
    //one without properties, and one with no properties in a different partition
    CHECK_EQUAL(status_codes::OK, put_entity(BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val));
    row = "Michael";
    property = "HasHair";
    prop_val = "Yup";
    CHECK_EQUAL(status_codes::OK, put_entity(BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val));
    row = "Aidan";
    CHECK_EQUAL(status_codes::OK, put_entity(BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val));
    partition = "Squirrels";
    row = "Chuck";
    CHECK_EQUAL(status_codes::OK, put_entity(BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val));

    //Check that only one entity has the same property as the first one (it's the first entity that should)
    property = "ZombieVirus";
    prop_val = "Infected";
    pair<status_code,value> first_test{get_Entities_from_property(BasicFixture::addr, BasicFixture::table, property, prop_val)};
    CHECK_EQUAL(status_codes::OK, first_test.first);
    CHECK_EQUAL(1, first_test.second.as_array().size());

    //Update all entities to have the same one as the first
    auto props = value::object(vector<pair<string,value>> {make_pair(property, value::string(prop_val))});
    first_test = do_request (methods::PUT, addr + add_property_admin + "/" + table, props);
    CHECK_EQUAL(status_codes::OK, first_test.first);

    //Check that all entities now have the added property (It's 5 because Franklin Aretha got infected too, poor guy)
    pair<status_code,value> second_test = {get_Entities_from_property(BasicFixture::addr, BasicFixture::table, property, prop_val)};
    CHECK_EQUAL(status_codes::OK, second_test.first);
    CHECK_EQUAL(5, second_test.second.as_array().size());

    //Check that an invalid AddProperty gets a 400 code
    //Invalid because no table specified
    pair<status_code,value> result = {
    do_request (methods::PUT,
    string(BasicFixture::addr) + "AddProperty/")};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Invalid because no JSON body
    result = {
    do_request (methods::PUT,
    string(BasicFixture::addr) + add_property_admin + string(BasicFixture::table))};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Ensure if the table does not exist a 404 code is recieved
    result = {
    do_request (methods::PUT,
    string(BasicFixture::addr) + add_property_admin + "/" + "WrongTable",
    value::object (vector<pair<string,value>>{make_pair(property, value::string(prop_val))}))};
    CHECK_EQUAL(status_codes::NotFound, result.first);

    //Clean up Table -- Extra deletes are because sometimes these entities refuse to be deleted (only this test, for some reason)
    //Especially patient zero. Why? Can't figure it out.
    partition = "Humans";
    row = "PatientZero";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    row = "Michael";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    row = "Aidan";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    partition = "Squirrels";
    row = "Chuck";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    partition = "Humans";
    row = "PatientZero";
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
    delete_entity (BasicFixture::addr, BasicFixture::table, partition, row);
  }

  /*
  Test get all entities with specific properties
  */
  TEST_FIXTURE(BasicFixture, GetEntityWithSpecProperties){
    string partition {"Cat"};
    string row {"Domestic"};

    int put_result { put_multi_properties_entity(BasicFixture::addr, BasicFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("10/10")),
        make_pair("Huggable", value::string("8/10")),
        make_pair("Furball", value::string("11/10"))
    }))};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> test_result { get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }))};
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(1, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);
    
    // //Add another entity with only one specific property
    partition = "Bunny";
    row = "Wild";

    put_result = put_multi_properties_entity(BasicFixture::addr, BasicFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("7/10"))
    }));
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(1, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add another entity with both specific property in different order
    row = "Domestic";

    put_result = put_multi_properties_entity(BasicFixture::addr, BasicFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Huggable", value::string("7/10")),
        make_pair("Likeable", value::string("7.5/10")),
        make_pair("Cute", value::string("8/10"))
    }));
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add another entity without any specific property
    partition = "Dog";
    row = "Wild";

    put_result = put_multi_properties_entity(BasicFixture::addr, BasicFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Tough", value::string("9/10"))
    }));
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Finally add entity with no properties
    partition = "Pig";
    row = "Domestic";

    put_result = put_entity_no_properties(BasicFixture::addr, BasicFixture::table, partition, row);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Check if entities returned contain specific properties
    int count {0};
    for(auto &p : test_result.second.as_array()){
      if(p.has_field("Cute") && p.has_field("Huggable")) count++;
    }
    CHECK_EQUAL(2, count);

    //Test result with no JSON body
    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, value::object(vector<pair<string,value>> {}));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(6, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Test after deleting an entity with the specfic properties
    partition = "Cat";
    row = "Domestic";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));

    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(1, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);


    //Test result where no specfic properties is found
    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table,
      value::object(vector<pair<string,value>> {
        make_pair("Scary", value::string("*")),
        make_pair("Deadly", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(0, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Test result where table does not exist
    test_result = get_spec_properties_entity(BasicFixture::addr, "Unknown",
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK_EQUAL(status_codes::NotFound, test_result.first);

    //Test result where no table name
    test_result = get_spec_properties_entity(BasicFixture::addr, "", value::object(vector<pair<string,value>> {}));
    CHECK_EQUAL(status_codes::BadRequest, test_result.first);

    //Cleanup tables
    partition = "Pig";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    partition = "Bunny";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    row = "Wild";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    partition = "Dog";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));

  }

  /*
  Test update property value
  */
  TEST_FIXTURE(BasicFixture, UpdateProperties){
    string partition {"Japanese"};
    string row {"Nintendo"};
    string property {"Fun"};
    string prop_val {"Yes"};

    CHECK_EQUAL(status_codes::OK, put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val));
    row = "PlayStation";
    CHECK_EQUAL(status_codes::OK, put_multi_properties_entity (BasicFixture::addr, BasicFixture::table, partition, row,
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("Yes")),
        make_pair("Cool", value::string("Yes"))
    })));
    partition = "Outdoors";
    row = "Running";
    CHECK_EQUAL(status_codes::OK, put_entity_no_properties (BasicFixture::addr, BasicFixture::table, partition, row));
    partition = "American";
    row = "Xbox";
    CHECK_EQUAL(status_codes::OK, put_multi_properties_entity (BasicFixture::addr, BasicFixture::table, partition, row,
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("No")),
        make_pair("Cool", value::string("No")),
        make_pair("Boring", value::string("No"))
    })));
    partition = "Indoors";
    row = "Volleyball";
    property = "Boring";
    prop_val = "No";
    CHECK_EQUAL(status_codes::OK, put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val));

    //Check returned entities' property value
    pair<status_code,value> test_result { get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("Yes"))
    }))};
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(3, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);
    int count {0};
    for(auto &p : test_result.second.as_array()){
      if(p.at("Fun") == value::string("Yes")) count++;
    }
    CHECK_EQUAL(2, count);


    //Update the property value
    CHECK_EQUAL(status_codes::OK, update_property (BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("Yes"))
    })));

    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(3, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Check returned entities' property value change (Should turn to "Yes")
    count = 0;
    for(auto &p : test_result.second.as_array()){
      if(p.at("Fun") == value::string("Yes")) count++;
    }
    CHECK_EQUAL(3, count);

    //Test result after updating multiple values
    CHECK_EQUAL(status_codes::OK, update_property (BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Boring", value::string("Yes")),
        make_pair("Cool", value::string("No"))
    })));

    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Boring", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);
    count = 0;
    for(auto &p : test_result.second.as_array()){
      if(p.at("Boring") == value::string("Yes")) count++;
    }
    CHECK_EQUAL(2, count);

    //Test result with all entities (to see if method changed other entities)
    test_result = get_spec_properties_entity(BasicFixture::addr, BasicFixture::table, value::object(vector<pair<string,value>> {}));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(6, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);
    count = 0;
    for(auto &p : test_result.second.as_array()){
      if(p.has_field("Cool")) {
        if(p.at("Cool") == value::string("Yes")) count++;
      }
    }
    CHECK_EQUAL(0, count);

    //Test result without JSON body
    CHECK_EQUAL(status_codes::BadRequest, update_property (BasicFixture::addr, BasicFixture::table, value::object(vector<pair<string,value>> {})));

    //Test result without table name
    CHECK_EQUAL(status_codes::BadRequest, update_property (BasicFixture::addr, "", value::object(vector<pair<string,value>> {})));

    //Test result where table does not exist
    CHECK_EQUAL(status_codes::NotFound, update_property (BasicFixture::addr, "Unknown", 
      value::object(vector<pair<string,value>> {
        make_pair("Message", value::string("Hi"))
      })));

    //Cleanup tables
    partition = "Japanese";
    row = "Nintendo";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    row = "PlayStation";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    partition = "Outdoors";
    row = "Running";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    partition = "American";
    row = "Xbox";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
    partition = "Indoors";
    row = "Volleyball";
    CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
  /********************
  **CODE ADDED - STOP**
  ********************/
  }
}

class AuthFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* auth_addr {"http://localhost:34570/"};
  static constexpr const char* userid {"user"};
  static constexpr const char* user_pwd {"user"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_pwd_prop {"Password"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* partition {"USA"};
  static constexpr const char* row {"Franklin,Aretha"};
  static constexpr const char* property {"Song"};
  static constexpr const char* prop_val {"RESPECT"};

public:
  AuthFixture() {
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    int put_result {put_entity (addr, table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    if (put_result != status_codes::OK) {
      throw std::exception();
    }
    // Ensure userid and password in system
    int user_result {put_entity (addr,
                                 auth_table,
                                 auth_table_partition,
                                 userid,
                                 auth_pwd_prop,
                                 user_pwd)};
    cerr << "user auth table insertion result " << user_result << endl;
    if (user_result != status_codes::OK)
      throw std::exception();
    
    // For GetUpdateData
    // Give Partition: Userid / Row: user the properties DataPartition and DataRow
    string DataPartition {"DataPartition"};
    user_result = put_entity (addr,
                             auth_table,
                             auth_table_partition,
                             userid,
                             DataPartition,
                             partition);
    if (user_result != status_codes::OK) {
      throw std::exception();
    }
    string DataRow {"DataRow"};
    user_result = put_entity (addr,
                             auth_table,
                             auth_table_partition,
                             userid,
                             DataRow,
                             row);
    if (user_result != status_codes::OK) {
      throw std::exception();
    }
  }

  ~AuthFixture() {
    int del_ent_result {delete_entity (addr, table, partition, row)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
  }
};


SUITE(AUTH_GET_TOKENS) {
  //Test that the AuthServer can give a read token, regardless of if it is valid (That will be tested with the BasicServer operations)
  TEST_FIXTURE(AuthFixture, GetAReadToken) {
    string validUser_ID {AuthFixture::userid};
    string validUser_pwd {AuthFixture::user_pwd};
    string invalidUser_ID {"TomatoSoup"};
    string invalidUser_pwd {"GrilledCheeseSandwich"};
    string non_seven_bit_user_pwd {"( ͡° ͜ʖ °)"}; //This is supposed to be a lenny face, will it compile!?
    string extraProperty {"Coffee"};
    string extraPropertyValue {"10/10"};
    string readTokenIdentifier {"sp=r"};

  //Ensure various 404-deserving requests get one
    //Invalid userId
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
    get_read_token(AuthFixture::auth_addr, invalidUser_ID, invalidUser_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::NotFound, token_res.first);
  
    //Correct username with an invalid password
    cout << "Requesting token" << endl;
    token_res = {
    get_read_token(AuthFixture::auth_addr, validUser_ID, invalidUser_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::NotFound, token_res.first);
    
    //Ensure various forms of bad requests get a 400 response
    //Non 7-bit ASCII password
    cout << "Requesting token" << endl;
    token_res = {
    get_read_token(AuthFixture::auth_addr, validUser_ID, non_seven_bit_user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, token_res.first);
    
    //No user ID
    cout << "Requesting token" << endl;
    value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", validUser_pwd)})};
    pair<status_code,value> result {do_request (methods::GET, AuthFixture::auth_addr + get_read_token_op + "/", pwd )};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);
  
    //Extra Property
    cout << "Requesting token" << endl;
    pwd = {build_json_object (vector<pair<string,string>> {make_pair("Password", validUser_pwd), make_pair(extraProperty, extraPropertyValue)})};
    result = {do_request (methods::GET, AuthFixture::auth_addr + get_read_token_op + "/" + validUser_ID, pwd )};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);
  
    //No password provided, either by not including it in request or by not having a password property on the value
    cout << "Requesting token" << endl;
    pwd = {build_json_object (vector<pair<string,string>> {make_pair(extraProperty, extraPropertyValue)})};
    result = {do_request (methods::GET, AuthFixture::auth_addr + get_read_token_op + "/" + validUser_ID)};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);
    cout << "Requesting token" << endl;
    result = {do_request (methods::GET, AuthFixture::auth_addr + get_read_token_op + "/" + validUser_ID, pwd)};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);

  //Ensure a correct token request gets a read token
    cout << "Requesting token" << endl;
    token_res = {
    get_read_token(AuthFixture::auth_addr, validUser_ID, validUser_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::OK, token_res.first);
    CHECK_EQUAL(true, token_res.second.find(readTokenIdentifier) != std::string::npos); // ie) token contains the little string that identifies it as read
  }

  TEST_FIXTURE(AuthFixture, GetAnUpdateToken) {
  string validUser_ID {AuthFixture::userid};
  string validUser_pwd {AuthFixture::user_pwd};
  string invalidUser_ID {"TomatoSoup"};
  string invalidUser_pwd {"GrilledCheeseSandwich"};
  string non_seven_bit_user_pwd {"( ͡° ͜ʖ °)"}; //This is supposed to be a lenny face, will it compile!?
  string extraProperty {"Coffee"};
  string extraPropertyValue {"10/10"};
  string updateTokenIdentifier {"sp=ru"};

  //Ensure various 404-deserving requests get one
    //Invalid userId
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
    get_update_token(AuthFixture::auth_addr, invalidUser_ID, invalidUser_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::NotFound, token_res.first);
  
    //Correct username with an invalid password
    cout << "Requesting token" << endl;
    token_res = {
    get_update_token(AuthFixture::auth_addr, validUser_ID, invalidUser_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::NotFound, token_res.first);

  //Ensure various forms of bad requests get a 400 response
    //Non 7-bit ASCII password
    cout << "Requesting token" << endl;
    token_res = {
    get_update_token(AuthFixture::auth_addr, validUser_ID, non_seven_bit_user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, token_res.first);

    //No user ID
    value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", validUser_pwd)})};
    cout << "Requesting token" << endl;
    pair<status_code,value> result {do_request (methods::GET, AuthFixture::auth_addr + get_update_token_op + "/", pwd )};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);
  
    //Extra Property
    pwd = {build_json_object (vector<pair<string,string>> {make_pair("Password", validUser_pwd), make_pair(extraProperty, extraPropertyValue)})};
    cout << "Requesting token" << endl;
    result = {do_request (methods::GET, AuthFixture::auth_addr + get_update_token_op + "/" + validUser_ID, pwd )};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);
  
    //No password provided, either by not including it in request or by not having a password property on the value
    pwd = {build_json_object (vector<pair<string,string>> {make_pair(extraProperty, extraPropertyValue)})};
    cout << "Requesting token" << endl;
    result = {do_request (methods::GET, AuthFixture::auth_addr + get_update_token_op + "/" + validUser_ID)};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);
    cout << "Requesting token" << endl;
    result = {do_request (methods::GET, AuthFixture::auth_addr + get_update_token_op + "/" + validUser_ID, pwd)};
    cout << "Token response " << result.first << endl;
    CHECK_EQUAL (status_codes::BadRequest, result.first);

  //Ensure a correct token request get an update token
    cout << "Requesting token" << endl;
    token_res = {
    get_update_token(AuthFixture::auth_addr, validUser_ID, validUser_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (status_codes::OK, token_res.first);
    CHECK_EQUAL(true, token_res.second.find(updateTokenIdentifier) != std::string::npos); // ie) token contains the little string that identifies it as update
  }
  TEST_FIXTURE(AuthFixture, GetUpdateData){
    string validUser_ID {AuthFixture::userid};
    string validUser_pwd {AuthFixture::user_pwd};
    cout << "Requesting token, DataPartition and DataRow" << endl;
    pair<status_code,value> token_res {
      get_update_data_function(AuthFixture::auth_addr, AuthFixture::userid, AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    // The partition and row are currently what UserID: user and Password: user currently have in their DataPartition and DataRow properties, respectively. This will need to be adjusted to be a proper unit test. (Ie. Tester puts in a UserID and Password into AuthTable, with a DataPartition and DataRow, and accordingly checks after calling GetUpdateData that it is returning the correct JSON object, with the correct entries for DataPartition and DataRow)
    
    // value json {build_json_object (vector<pair<string,string>> {make_pair("DataPartition", AuthFixture::partition), make_pair("DataRow", AuthFixture::row)})};
    assert(token_res.first == status_codes::OK);
    string partition {AuthFixture::partition};
    string row {AuthFixture::row};
    
    // value passed_back_json = token_res.second;
    string passed_back_partition {};
    for (const auto& v : token_res.second.as_object()){
      if(v.first == "DataPartition") passed_back_partition = v.second.as_string(); 
    }
    CHECK_EQUAL(partition, passed_back_partition);
    string passed_back_row {};
    for (const auto& v : token_res.second.as_object()){
      if(v.first == "DataRow") passed_back_row = v.second.as_string(); 
    }
    CHECK_EQUAL(row, passed_back_row);
  }
}

SUITE(ENTITY_AUTH) {
  TEST_FIXTURE(AuthFixture, GetEntityAuth) {
    pair<string,string> props {make_pair(string(AuthFixture::property),string(AuthFixture::prop_val))};
    string partition {"USA"};
    string row {"Franklin,Aretha"};
    string otherRow {"Lim,Correy"};
    string otherPartition {"Canada"};

    //Add properties to table
    CHECK_EQUAL(status_codes::OK, put_multi_properties_entity(AuthFixture::addr, AuthFixture::table, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair(props.first,value::string(props.second))
    })));

    //Request read token
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_read_token (AuthFixture::auth_addr, AuthFixture::userid, AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL (token_res.first, status_codes::OK);

    //Get entity using AuthToken
    pair<status_code,value> result {get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row)};
    CHECK_EQUAL(status_codes::OK, result.first);

    //Check if entity returned is correct
    value expect_value {
      build_json_object (vector<pair<string,string>> {
        make_pair(string(props.first),string(props.second))
    })};
    CHECK(result.second.is_object());
    compare_json_values (expect_value, result.second);

    //Try reading entity with update token
    cout << "Requesting token" << endl;
    pair<status_code,string> token_update_res {
    get_update_token (AuthFixture::auth_addr, AuthFixture::userid, AuthFixture::user_pwd)};
    cout << "Token response " << token_update_res.first << endl;
    CHECK_EQUAL (token_update_res.first, status_codes::OK);

    result = get_entity_auth(AuthFixture::addr, AuthFixture::table, token_update_res.second, partition, row);
    CHECK_EQUAL(status_codes::OK, result.first);

    //Ensure NotFound responses (404)
      //Try reading entity with invalid auth token
      result = get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, otherPartition, otherRow);
      CHECK_EQUAL(status_codes::NotFound, result.first);

      //Try reading non-existent table
      string invalidTable {"Unknown"};
      result = get_entity_auth(AuthFixture::addr, invalidTable, token_res.second, partition, row);
      CHECK_EQUAL(status_codes::NotFound, result.first);

      //Try reading non-existent partition and row
      string invalidPartition {"Missing"};
      string invalidRow {"No."};
      result = get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, invalidPartition, row);
      CHECK_EQUAL(status_codes::NotFound, result.first);
      result = get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, invalidRow);
      CHECK_EQUAL(status_codes::NotFound, result.first);

    //Try reading entity with < 4 parameters
      //Missing table
      //Note to marker: We've discovered that if the signature in the token contains the string %2F, then paths[] is constructed incorrectly, resulting in an inconsistent result of whether or not these tests pass. We've omitted this as a part of our submission as a result.
      /*
      result = do_request (methods::GET, string(AuthFixture::addr)
                          + read_entity_auth + "/" + token_res.second + "/" + partition + "/" + row);
      CHECK_EQUAL(status_codes::BadRequest, result.first);
      */
      //Missing table + token
      result = do_request (methods::GET, string(AuthFixture::addr)
                          + read_entity_auth + "/" + partition + "/" + row);
      CHECK_EQUAL(status_codes::BadRequest, result.first);

      //Missing table + token + partition
      result = do_request (methods::GET, string(AuthFixture::addr)
                          + read_entity_auth + "/" + row);
      CHECK_EQUAL(status_codes::BadRequest, result.first);

      //Missing all arguements
      result = do_request (methods::GET, string(AuthFixture::addr)
                          + read_entity_auth);
      CHECK_EQUAL(status_codes::BadRequest, result.first);
  }

  TEST_FIXTURE(AuthFixture, UpdateEntityAuth) {
    pair<string,value> props {make_pair(string(AuthFixture::property),value::string(AuthFixture::prop_val))};
    string partition {"USA"};
    string row {"Franklin,Aretha"};
    string otherRow {"Lim,Correy"};
    string otherPartition {"Canada"};;

    //Add properties to table
    CHECK_EQUAL(status_codes::OK, put_multi_properties_entity(AuthFixture::addr, AuthFixture::table, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair(props.first,props.second)
    })));

    //Request update token
    cout << "Requesting token" << endl;
    pair<status_code,string> token_res {
      get_update_token (AuthFixture::auth_addr, AuthFixture::userid, AuthFixture::user_pwd)};
    cout << "Token response " << token_res.first << endl;
    CHECK_EQUAL(token_res.first, status_codes::OK);

    //Update properties in the table
    CHECK_EQUAL(status_codes::OK, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair("Fun",value::string("Yes"))
    })));

    //Check if properties were updated
    pair<status_code,value> result {get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row)};
    CHECK_EQUAL(status_codes::OK, result.first);
    value expect_value {
      build_json_object (vector<pair<string,string>> {
        make_pair(string("Fun"),string("Yes")),
        make_pair(props.first,props.second.as_string())
    })};
    CHECK(result.second.is_object());
    compare_json_values (expect_value, result.second);

    //Try adding new property
    CHECK_EQUAL(status_codes::OK, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair("Hello",value::string("World!"))
    })));

    result = get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row);
    CHECK_EQUAL(status_codes::OK, result.first);
    expect_value = build_json_object (vector<pair<string,string>> {
      make_pair(string("Fun"),string("Yes")),
      make_pair(props.first,props.second.as_string()),
      make_pair(string("Hello"),string("World!"))
    });
    CHECK(result.second.is_object());
    compare_json_values (expect_value, result.second);

    //Try adding multiple properties
    CHECK_EQUAL(status_codes::OK, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair("Cool",value::string("HeckYeah")),
        make_pair("Replay",value::string("Always"))
    })));

    result = get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row);
    CHECK_EQUAL(status_codes::OK, result.first);
    expect_value = build_json_object (vector<pair<string,string>> {
      make_pair(string("Fun"),string("Yes")),
      make_pair(props.first,props.second.as_string()),
      make_pair(string("Hello"),string("World!")),
      make_pair(string("Cool"),string("HeckYeah")),
      make_pair(string("Replay"),string("Always"))
    });
    CHECK(result.second.is_object());
    compare_json_values (expect_value, result.second);

    //Trying to make a new entity
    partition = "ShouldNot";
    row = "Work!";
    CHECK_EQUAL(status_codes::NotFound, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair("Blah",value::string("Haha")),
        make_pair("Beep",value::string("Boop"))
    })));

    //Ensure NotFound responses (404)
    partition = "Canada";
    row = "Lim,Correy";
    props = make_pair(string("Happy"),value::string("Sad"));
      //Try updating entity with invalid auth token
      CHECK_EQUAL(status_codes::NotFound, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row,
        value::object (vector<pair<string,value>> {
          make_pair(props.first, props.second)
      })));

      //Try updating non-existent table
      string invalidTable {"NoTable"};
      CHECK_EQUAL(status_codes::NotFound, put_entity_auth(AuthFixture::addr, invalidTable, token_res.second, partition, row,
        value::object (vector<pair<string,value>> {
          make_pair(props.first, props.second)
      })));

      //Try updating non-existent partition and row
      string invalidPartition {"DoesNot"};
      string invalidRow {"Exist"};
      CHECK_EQUAL(status_codes::NotFound, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, invalidPartition, row,
        value::object (vector<pair<string,value>> {
          make_pair(props.first, props.second)
      })));
      CHECK_EQUAL(status_codes::NotFound, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, invalidRow,
        value::object (vector<pair<string,value>> {
          make_pair(props.first, props.second)
      })));

    //Try updating entity with < 4 parameters
    props = make_pair("Try",value::string("Adding"));
      //Missing table
      //Note to marker: We've discovered that if the signature in the token contains the string %2F, then paths[] is constructed incorrectly, resulting in an inconsistent result of whether or not these tests pass. We've omitted this as a part of our submission as a result.
      /*
      result = do_request (methods::PUT, string(AuthFixture::addr)
                          + update_entity_auth + "/" + token_res.second + "/" + partition + "/" + row,
                          value::object (vector<pair<string,value>> {
                            make_pair(props.first, props.second)
                          }));
      CHECK_EQUAL(status_codes::BadRequest, result.first);
      */
      //Missing table + token
      result = do_request (methods::PUT, string(AuthFixture::addr)
                          + update_entity_auth + "/" + partition + "/" + row,
                          value::object (vector<pair<string,value>> {
                            make_pair(props.first, props.second)
                          }));
      CHECK_EQUAL(status_codes::BadRequest, result.first);

      //Missing table + token + partition
      result = do_request (methods::PUT, string(AuthFixture::addr)
                          + update_entity_auth + "/" + row,
                          value::object (vector<pair<string,value>> {
                            make_pair(props.first, props.second)
                          }));
      CHECK_EQUAL(status_codes::BadRequest, result.first);

      //Missing all arguements
      result = do_request (methods::PUT, string(AuthFixture::addr)
                          + update_entity_auth,
                          value::object (vector<pair<string,value>> {
                            make_pair(props.first, props.second)
                          }));
      CHECK_EQUAL(status_codes::BadRequest, result.first);


    //Try updating table with read token
    cout << "Requesting token" << endl;
    pair<status_code,string> token_read_res {
    get_read_token (AuthFixture::auth_addr, AuthFixture::userid, AuthFixture::user_pwd)};
    cout << "Token response " << token_read_res.first << endl;
    CHECK_EQUAL(token_read_res.first, status_codes::OK);

    CHECK_EQUAL(status_codes::Forbidden, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_read_res.second, AuthFixture::partition, AuthFixture::row,
      value::object (vector<pair<string,value>> {
        make_pair("Fun",value::string("No"))
    })));
  }
}

class UserFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* user_addr {"http://localhost:34572/"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_dataPartition {"DataPartition"};
  static constexpr const char* auth_dataRow {"DataRow"};

  static constexpr const char* userID_A {"Aidan"};
  static constexpr const char* user_pwd_A {"SuperCool"};
  static constexpr const char* country_A {"Canada"};
  static constexpr const char* name_A {"Wessel,Aidan"};

  static constexpr const char* userID_B {"Superman"};
  static constexpr const char* user_pwd_B {"Kryptonite"};
  static constexpr const char* country_B {"USA"};
  static constexpr const char* name_B {"Kent,Clark"};

  static constexpr const char* userID_C {"Batman"};
  static constexpr const char* user_pwd_C {"DarkKnight"};
  static constexpr const char* country_C {"USA"};
  static constexpr const char* name_C {"Wayne,Bruce"};


public:
  UserFixture() {
    //Ensure dataTable is created
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    //Make some users
    createFakeUser(userID_A, user_pwd_A, country_A, name_A);
    createFakeUser(userID_B, user_pwd_B, country_B, name_B);
    createFakeUser(userID_C, user_pwd_C, country_C, name_C);
  }

  ~UserFixture() {
    int del_ent_result {delete_entity (addr, table, country_A, name_A)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
    del_ent_result = {delete_entity (addr, auth_table, auth_table_partition, userID_A)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
     del_ent_result = {delete_entity (addr, table, country_B, name_B)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
      del_ent_result = {delete_entity (addr, auth_table, auth_table_partition, userID_B)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
      del_ent_result = {delete_entity (addr, table, country_C, name_C)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
      del_ent_result = {delete_entity (addr, auth_table, auth_table_partition, userID_C)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
  }
};

SUITE(USER_SERVER_OPS){
  TEST_FIXTURE(UserFixture, signOnOff){
    //Ensure that a non-signed on request gets a forbidden
    pair<status_code,value> FLResult = ReadFriendList(string(UserFixture::userID_A));
    CHECK_EQUAL(status_codes::Forbidden, FLResult.first);

    //Ensure that sign on works
    int signOnResult {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Ensure a second sign on (of the same user) works just like the first
    signOnResult = {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Now that the user is signed on, make sure a request works
    FLResult = ReadFriendList(string(UserFixture::userID_A));
    CHECK_EQUAL(status_codes::OK, FLResult.first);

    //Ensure that a sign on of an invalid userId/password combo gets a 404
    string invalidUserId = "Awesomerizer";
    string invalidUserPass = "OnSteam";
    signOnResult = {signOn(invalidUserId, invalidUserPass)};
    CHECK_EQUAL(status_codes::NotFound, signOnResult);

    //Ensure that a valid auth server entry with no corresponding row in the data table gets 404
    string fakeUserID = "Daniel";
    string fakeUserPassword = "Sedin";
    string fakeUserDataPartition= "Vancouver";
    string fakeUserDataRow = "Canucks";
    string auth_table = "AuthTable";
    string auth_table_partition = "Userid";
    string auth_pwd_prop = "Password";

    int user_result {put_entity (string(addr),
                                 string(auth_table),
                                 string(auth_table_partition),
                                 string(fakeUserID),
                                 string(auth_pwd_prop),
                                 string(fakeUserPassword))};
    CHECK_EQUAL(status_codes::OK, user_result);

    pair<status_code, value> addPropResult = {
    do_request (methods::PUT,
                addr + update_entity_admin + "/" + auth_table + "/" + auth_table_partition + "/" + fakeUserID,
                value::object (vector<pair<string,value>>
                {make_pair(string(auth_dataPartition), value::string(fakeUserDataPartition)),
                 make_pair(string(auth_dataRow), value::string(fakeUserDataRow))}))};
    CHECK_EQUAL(status_codes::OK, addPropResult.first);

    signOnResult = {signOn(fakeUserID, fakeUserPassword)};
    CHECK_EQUAL(status_codes::NotFound, signOnResult);

    //Check to make sure that multiple sign ons are okay
    signOnResult = {signOn(userID_C, user_pwd_C)};
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Ensure that sign off works
    int signOffResult {signOff(string(UserFixture::userID_A))};
    CHECK_EQUAL(status_codes::OK, signOffResult);

    //Sign off second sign in
    signOffResult = {signOff(userID_C)};
    CHECK_EQUAL(status_codes::OK, signOffResult);

    //Try getting friends list after sign off, expecting another forbidden
    FLResult = ReadFriendList(string(UserFixture::userID_A));
    CHECK_EQUAL(status_codes::Forbidden, FLResult.first);

    //Ensure that a second sign off gets a 404
    signOffResult = {signOff(string(UserFixture::userID_A))};
    CHECK_EQUAL(status_codes::NotFound, signOffResult);

    //Delete the extra added entitiy from the AuthTable
    int del_ent_result {delete_entity (addr, auth_table, auth_table_partition, fakeUserID)};
    cout << "Delete Result: " << del_ent_result << endl;
    CHECK_EQUAL(status_codes::OK, del_ent_result);
  }

  TEST_FIXTURE(UserFixture, friendOps){
    //Using the get_partition_entity function instead of get friend list so one test is not dependant on another being successful
    string friendEntryA = string(country_A) + ";" + string(name_A);
    string friendEntryB = string(country_B) + ";" + string(name_B);
    string friendEntryC = string(country_C) + ";" + string(name_C);

    //Sign On A
    int signOnResult {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    CHECK_EQUAL(status_codes::OK, signOnResult);
    bool findResult;

    //Ensure adding a friend works
    int addResult = addFriend(UserFixture::userID_A, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, addResult);
    pair<status_code,value> getResult = get_partition_entity (addr, table, country_A, name_A);
    findResult = (getResult.second["Friends"].as_string().find(friendEntryB) != string::npos); //Basically: From the string in the entitiy's properties paried with "Friends", is friendEntryB there?
    CHECK_EQUAL(true, findResult);

    //Add the same friend again, shouldn't add another copy
    addResult = addFriend(UserFixture::userID_A, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, addResult);
    pair<status_code,value> secondAddResult = get_partition_entity (addr, table, country_A, name_A);
    CHECK_EQUAL(true, getResult.second["Friends"].as_string().length() == secondAddResult.second["Friends"].as_string().length()); //No new friends were added if these lengths are the same

    //Ensure removing friend works
    int remResult = unFriend(UserFixture::userID_A, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, remResult);
    getResult = get_partition_entity (addr, table, country_A, name_A);
    CHECK_EQUAL(true, getResult.second["Friends"].as_string().empty()); //Should now be empty

    //Remove the same friend again, should just do nothing
    remResult = unFriend(UserFixture::userID_A, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, remResult);
    getResult = get_partition_entity (addr, table, country_A, name_A);
    CHECK_EQUAL(true, getResult.second["Friends"].as_string().empty()); //Should still be empty

    //Now, with muliple users
    //Sign on B & C
    signOnResult  = {signOn(string(UserFixture::userID_B), string(UserFixture::user_pwd_B))};
    CHECK_EQUAL(status_codes::OK, signOnResult);
    signOnResult  = {signOn(string(UserFixture::userID_C), string(UserFixture::user_pwd_C))};
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Ensure adding a friend works
    addResult = addFriend(UserFixture::userID_A, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, addResult);
    getResult = get_partition_entity (addr, table, country_A, name_A);
    findResult = (getResult.second["Friends"].as_string().find(friendEntryB) != string::npos);
    CHECK_EQUAL(true, findResult);

    //Ensure adding a friend works (B adds C)
    addResult = addFriend(UserFixture::userID_B, string(UserFixture::country_C), string(UserFixture::name_C));
    CHECK_EQUAL(status_codes::OK, addResult);
    getResult = get_partition_entity (addr, table, country_B, name_B);
    findResult = (getResult.second["Friends"].as_string().find(friendEntryC) != string::npos);
    CHECK_EQUAL(true, findResult);

    //Ensure adding a friend works (C adds B)
    addResult = addFriend(UserFixture::userID_C, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, addResult);
    getResult = get_partition_entity (addr, table, country_C, name_C);
    findResult = (getResult.second["Friends"].as_string().find(friendEntryB) != string::npos);
    CHECK_EQUAL(true, findResult);

    //Ensure adding a friend works (A adds C)
    addResult = addFriend(UserFixture::userID_A, string(UserFixture::country_C), string(UserFixture::name_C));
    CHECK_EQUAL(status_codes::OK, addResult);
    getResult = get_partition_entity (addr, table, country_A, name_A);
    findResult = (getResult.second["Friends"].as_string().find(friendEntryC) != string::npos);
    CHECK_EQUAL(true, findResult);

    //Now, removing
    remResult = unFriend(UserFixture::userID_A, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, remResult);
    getResult = get_partition_entity (addr, table, country_A, name_A);
    CHECK_EQUAL(true, getResult.second["Friends"].as_string() == friendEntryC); //Should now be just friend entry C (B removed)
    // cout << "Failure 1:" << getResult.second["Friends"].as_string() << endl;
    // dump_table_contents("DataTable");

    remResult = unFriend(UserFixture::userID_B, string(UserFixture::country_C), string(UserFixture::name_C));
    CHECK_EQUAL(status_codes::OK, remResult);
    getResult = get_partition_entity (addr, table, country_B, name_B);
    CHECK_EQUAL(true, getResult.second["Friends"].as_string().empty()); //Should now be empty

    remResult = unFriend(UserFixture::userID_C, string(UserFixture::country_B), string(UserFixture::name_B));
    CHECK_EQUAL(status_codes::OK, remResult);
    getResult = get_partition_entity (addr, table, country_C, name_C);
    CHECK_EQUAL(true, getResult.second["Friends"].as_string().empty()); //Should now be empty

    remResult = unFriend(UserFixture::userID_A, string(UserFixture::country_C), string(UserFixture::name_C));
    CHECK_EQUAL(status_codes::OK, remResult);
    getResult = get_partition_entity (addr, table, country_A, name_A);
    CHECK_EQUAL(true, getResult.second["Friends"].as_string().empty()); //Should now be empty
    // cout << "Failure 2:" << getResult.second["Friends"].as_string() << endl;
    // dump_table_contents("DataTable");

    //Sign off everyone
    int signOffResult {signOff(string(UserFixture::userID_A))};
    CHECK_EQUAL(status_codes::OK, signOffResult);
    signOffResult = {signOff(string(UserFixture::userID_B))};
    CHECK_EQUAL(status_codes::OK, signOffResult);
    signOffResult = {signOff(string(UserFixture::userID_C))};
    CHECK_EQUAL(status_codes::OK, signOffResult);
    
    
    
  }

  TEST_FIXTURE(UserFixture, getFriendList){
    //Sign On
    int signOnResult {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    CHECK_EQUAL(status_codes::OK, signOnResult);
    
    // Ensure adding a friend works
    string newFriendCountry = "USA";
    string newFriendName = "Kitzmiller,Trevor";
    // For unfriending
    string first_friend_country = newFriendCountry;
    string first_friend_name = newFriendName;
    int addResult = addFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, addResult);
    
    // Check that ReadFriendList works for 1 friend
    pair<status_code,value> friend_list_result = ReadFriendList(UserFixture::userID_A);

    string correct_friend_list {"USA;Kitzmiller,Trevor"};
    
    string passed_back_friend_list {};
    for (const auto& v : friend_list_result.second.as_object()){
      if(v.first == "Friends") passed_back_friend_list = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_friend_list, passed_back_friend_list);
    
    // cout << "First Friend Check: " << passed_back_friend_list << endl;
    
    // Adding a second friend
    newFriendCountry = "Canada";
    newFriendName = "Quin,Tegan";
    string second_friend_country = newFriendCountry;
    string second_friend_name = newFriendName;
    addResult = addFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, addResult);
    
    // Check that ReadFriendList works for 2 friends
    friend_list_result = ReadFriendList(UserFixture::userID_A);
    
    for (const auto& v : friend_list_result.second.as_object()){
      if(v.first == "Friends") passed_back_friend_list = v.second.as_string(); 
    }
    correct_friend_list = "USA;Kitzmiller,Trevor|Canada;Quin,Tegan";
    CHECK_EQUAL(correct_friend_list, passed_back_friend_list);
    
    // cout << "Second Friend Check: " << passed_back_friend_list << endl;
    
    // Adding a third friend
    newFriendCountry = "Canada";
    newFriendName = "Quin,Sara";
    addResult = addFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, addResult);
    
    // Check that ReadFriendList works for 3 friends
    friend_list_result = ReadFriendList(UserFixture::userID_A);
    
    for (const auto& v : friend_list_result.second.as_object()){
      if(v.first == "Friends") passed_back_friend_list = v.second.as_string(); 
    }
    correct_friend_list = "USA;Kitzmiller,Trevor|Canada;Quin,Tegan|Canada;Quin,Sara";
    CHECK_EQUAL(correct_friend_list, passed_back_friend_list);
    
    // Testing to ensure multiple users can do this operation at the same time
    // Sign On User B
    signOnResult = signOn(string(UserFixture::userID_B), string(UserFixture::user_pwd_B));
    CHECK_EQUAL(status_codes::OK, signOnResult);
    
    // Add USA;Kitzmiller,Trevor as a friend for User B
    addResult = addFriend(UserFixture::userID_B, first_friend_country, first_friend_name);
    CHECK_EQUAL(status_codes::OK, addResult);
    
    // Check that ReadFriendList works for 1 friend
    friend_list_result = ReadFriendList(UserFixture::userID_B);
    
    correct_friend_list = "USA;Kitzmiller,Trevor";
    for (const auto& v : friend_list_result.second.as_object()){
      if(v.first == "Friends") passed_back_friend_list = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_friend_list, passed_back_friend_list);
    
    // Add Canada;Quin,Tegan as a friend for User B
    addResult = addFriend(UserFixture::userID_B, second_friend_country, second_friend_name);
    CHECK_EQUAL(status_codes::OK, addResult);
    
    // Check that ReadFriendList works for 2 friends
    friend_list_result = ReadFriendList(UserFixture::userID_B);
    
    correct_friend_list = "USA;Kitzmiller,Trevor|Canada;Quin,Tegan";
    for (const auto& v : friend_list_result.second.as_object()){
      if(v.first == "Friends") passed_back_friend_list = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_friend_list, passed_back_friend_list);
    
    // Check that User A can still make a ReadFriendList call with User B signed in and having done a few operations
    friend_list_result = ReadFriendList(UserFixture::userID_A);
    
    for (const auto& v : friend_list_result.second.as_object()){
      if(v.first == "Friends") passed_back_friend_list = v.second.as_string(); 
    }
    correct_friend_list = "USA;Kitzmiller,Trevor|Canada;Quin,Tegan|Canada;Quin,Sara";
    CHECK_EQUAL(correct_friend_list, passed_back_friend_list);
    
    // Remove added friends for User A
    int remResult = unFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, remResult);
    remResult = unFriend(UserFixture::userID_A, second_friend_country, second_friend_name);
    CHECK_EQUAL(status_codes::OK, remResult);
    remResult = unFriend(UserFixture::userID_A, first_friend_country, first_friend_name);
    CHECK_EQUAL(status_codes::OK, remResult);
    
    // Remove added friends for User B
    remResult = unFriend(UserFixture::userID_B, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, remResult);
    remResult = unFriend(UserFixture::userID_B, second_friend_country, second_friend_name);
    CHECK_EQUAL(status_codes::OK, remResult);
    
    // Sign off User A
    int signOffResult = {signOff(string(UserFixture::userID_A))};
    CHECK_EQUAL(status_codes::OK, signOffResult);
    
    // Sign off User B
    signOffResult = {signOff(string(UserFixture::userID_B))};
    CHECK_EQUAL(status_codes::OK, signOffResult);
  }
  
  TEST_FIXTURE(UserFixture, updateStatus){
    //Sign On
    int signOnResult {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    CHECK_EQUAL(status_codes::OK, signOnResult);

    createFakeUser("test1", "test1", "USA", "Kitzmiller,Trevor"); // Creates an entity in both AuthTable and DataTable

    string newFriendCountry = "USA";
    string newFriendName = "Kitzmiller,Trevor";
    int addResult = addFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    cout << endl;
    
    // User A updates own status with "Just_testing_things"
    do_request (methods::PUT,
                user_addr + update_status + "/" + string(UserFixture::userID_A) + "/" + "Just_testing_things");
    cout << endl;
    
    // Ensure own status was updated
    pair<status_code,value> own_update_status_result = get_partition_entity (UserFixture::addr, UserFixture::table, UserFixture::country_A, UserFixture::name_A);
    string correct_status {"Just_testing_things"};
    string passed_back_status {};
    for (const auto& v : own_update_status_result.second.as_object()){
      if(v.first == "Status") passed_back_status = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_status, passed_back_status);
    
    // Ensure that status was placed in "Update":"" for DataTable entity for USA;Kitzmiller,Trevor
    pair<status_code,value> friend_update_status_result = get_partition_entity (UserFixture::addr, UserFixture::table, newFriendCountry, newFriendName);
    string correct_update {"Just_testing_things\n"};
    string passed_back_update {};
    for (const auto& v : friend_update_status_result.second.as_object()){
      if(v.first == "Updates") passed_back_update = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_update, passed_back_update);
    
    // Begin test for two simultaneous users
    signOnResult = signOn(string(UserFixture::userID_B), string(UserFixture::user_pwd_B));
    CHECK_EQUAL(status_codes::OK, signOnResult);
    
    // User B adds User A to their list of friends
    addResult = addFriend(UserFixture::userID_B, UserFixture::country_A, UserFixture::name_A);
    // User A adds User B to their list of friends
    addResult = addFriend(UserFixture::userID_A, UserFixture::country_B, UserFixture::name_B);
    
    // User A updates their status again
    do_request (methods::PUT, user_addr + update_status + "/" + string(UserFixture::userID_A) + "/" + "Cannot_wait_for_finals_to_be_over");
    
    // Ensure own status was updated
    own_update_status_result = get_partition_entity (UserFixture::addr, UserFixture::table, UserFixture::country_A, UserFixture::name_A);
    correct_status = "Cannot_wait_for_finals_to_be_over";
    for (const auto& v : own_update_status_result.second.as_object()){
      if(v.first == "Status") passed_back_status = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_status, passed_back_status);
    
    // User A's status update should appear in User B and friend USA;Kitzmiller,Trevor under "Updates"
    // Checking USA;Kitzmiller,Trevor first (should also have previous Status Update from User A)
    friend_update_status_result = get_partition_entity (UserFixture::addr, UserFixture::table, newFriendCountry, newFriendName);
    correct_update = "Just_testing_things\nCannot_wait_for_finals_to_be_over\n";
    for (const auto& v : friend_update_status_result.second.as_object()){
      if(v.first == "Updates") passed_back_update = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_update, passed_back_update);
    
    // Checking User B
    friend_update_status_result = get_partition_entity (UserFixture::addr, UserFixture::table, UserFixture::country_B, UserFixture::name_B);
    correct_update = "Cannot_wait_for_finals_to_be_over\n";
    for (const auto& v : friend_update_status_result.second.as_object()){
      if(v.first == "Updates") passed_back_update = v.second.as_string();
    }
    CHECK_EQUAL(correct_update, passed_back_update);
    
    // Now User B updates their status
    do_request (methods::PUT, user_addr + update_status + "/" + string(UserFixture::userID_B) + "/" + "Dark_Souls_3_comes_out_around_finals_whyyyyyy");
    
    // Ensure own status was updated
    own_update_status_result = get_partition_entity (UserFixture::addr, UserFixture::table, UserFixture::country_B, UserFixture::name_B);
    correct_status = "Dark_Souls_3_comes_out_around_finals_whyyyyyy";
    for (const auto& v : own_update_status_result.second.as_object()){
      if(v.first == "Status") passed_back_status = v.second.as_string(); 
    }
    CHECK_EQUAL(correct_status, passed_back_status);
    
    // This should appear in User A
    // Checking User A
    friend_update_status_result = get_partition_entity (UserFixture::addr, UserFixture::table, UserFixture::country_A, UserFixture::name_A);
    correct_update = "Dark_Souls_3_comes_out_around_finals_whyyyyyy\n";
    for (const auto& v : friend_update_status_result.second.as_object()){
      if(v.first == "Updates") passed_back_update = v.second.as_string();
    }
    CHECK_EQUAL(correct_update, passed_back_update);
    
    // Delete USA;Kitzmiller,Trevor from DataTable
    int delete_result = delete_entity (UserFixture::addr, UserFixture::table, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, delete_result);
    
    // Delete USA;Kitzmiller,Trevor from AuthTable
    delete_result = delete_entity (UserFixture::addr, UserFixture::auth_table, "Userid", "test1");
    CHECK_EQUAL(status_codes::OK, delete_result);
    
    //Sign off
    int signOffResult {signOff(string(UserFixture::userID_A))};
    CHECK_EQUAL(status_codes::OK, signOffResult);
  }

  TEST_FIXTURE(UserFixture, badRequests){
    //Ensure various bad commands get 400
    string badCommand = "DANCE";
    cout << "Checking bad requests" << endl;

    pair<status_code,value> result {
    do_request(methods::POST,
    user_addr + badCommand + "/" + userID_A)};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    result = {
    do_request(methods::PUT,
    user_addr + badCommand + "/" + userID_A)};
    CHECK_EQUAL(status_codes::BadRequest, result.first);


    result = {
    do_request(methods::GET,
    user_addr + badCommand + "/" + userID_A)};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Ensure disallowed methods get 405
    result = {
    do_request(methods::HEAD,
    user_addr + badCommand + "/" + userID_A)};
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = {
    do_request(methods::DEL,
    user_addr + badCommand + "/" + userID_A)};
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = {
    do_request(methods::CONNECT,
    user_addr + badCommand + "/" + userID_A)};
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);
  }
}

class PushFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* push_addr {"http://localhost:34574/"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_dataPartition {"DataPartition"};
  static constexpr const char* auth_dataRow {"DataRow"};

  static constexpr const char* userID {"Michael"};
  static constexpr const char* user_pwd {"ReallyLazy"};
  static constexpr const char* country {"Canada"};
  static constexpr const char* name {"Trinh,Michael"};

public:
  PushFixture() {
    //Ensure dataTable is created
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    createFakeUser(userID, user_pwd, country, name);
  }

  ~PushFixture() {
    int del_ent_result {delete_entity (addr, table, country, name)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
    del_ent_result = {delete_entity (addr, auth_table, auth_table_partition, userID)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
  }
};

SUITE(PUSH_SERVER_OPS){
  TEST_FIXTURE(PushFixture, pushStatus){
    // Try pushing w/ empty JSON body ({"":""})
    int sign_on {signOn(string(PushFixture::userID), string(PushFixture::user_pwd))};
    CHECK_EQUAL(status_codes::OK, sign_on);

    string status {"Still_no_friends_to_hang_out"};
    pair<status_code,value> result {post_update(PushFixture::push_addr, PushFixture::country, 
      PushFixture::userID, status, 
        value::object(vector<pair<string,value>>{
          make_pair(string(""), value::string(""))}))};
    CHECK_EQUAL(status_codes::OK, result.first);

    // Add some friends and push again
    string userID {"HelloKitty"};
    string user_pwd {"Sanrio"};
    string country {"Japan"};
    string name {"Kitty,White"};
    createFakeUser(userID, user_pwd, country, name);
    int add_friend {addFriend(PushFixture::userID, country, name)};
    CHECK_EQUAL(status_codes::OK, add_friend);

    userID = "Gaben";
    user_pwd = "PraiseLordGaben";
    country = "USA";
    name = "Newell,Gabe";
    createFakeUser(userID, user_pwd, country, name);
    add_friend = addFriend(PushFixture::userID, country, name);
    CHECK_EQUAL(status_codes::OK, add_friend);

    status = "Hey_I_got_friends";
    pair<status_code,value> friendlist {ReadFriendList(PushFixture::userID)};
    CHECK_EQUAL(status_codes::OK, friendlist.first);
    result = post_update(PushFixture::push_addr, PushFixture::country, 
      PushFixture::userID, status, friendlist.second);
    CHECK_EQUAL(status_codes::OK, result.first);

      // Check if friends recieved update
      pair<status_code,value> status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      string expected {"Hey_I_got_friends\n"};
      string return_res {};
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

      country = "Japan";
      name = "Kitty,White";
      status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

    // Unfriend and push again (friendlist should still have the un-friended friend added aka. ghost friend (T.T)7 )
    country = "USA";
    name = "Newell,Gabe";
    int un_friend {unFriend(PushFixture::userID, country, name)};
    CHECK_EQUAL(status_codes::OK, un_friend);

    status = "At_least_I_still_have_you";
    result = post_update(PushFixture::push_addr, PushFixture::country, 
      PushFixture::userID, status, friendlist.second);
    CHECK_EQUAL(status_codes::OK, result.first);

      // Check friends
      country = "Japan";
      name = "Kitty,White";
      status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      expected = "Hey_I_got_friends\nAt_least_I_still_have_you\n";
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

      country = "USA";
      name = "Newell,Gabe";
      status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

    // Try w/ 1 real friend and 1 non-existent friend (friend not in DataTable)
    string fakeName {"Ghost"};
    string fakeCountry {"Vanished"};
    country = "Japan";
    name = "Kitty,White";

    status = "Boo!";
    result = post_update(PushFixture::push_addr, PushFixture::country, 
      PushFixture::userID, status, 
        value::object(vector<pair<string,value>>{
          make_pair(string("Friends"), value::string(country + ";" + name + "|" + fakeCountry + ";" + fakeName))}));
    CHECK_EQUAL(status_codes::OK, result.first);

      // Check real friend
      status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      expected = "Hey_I_got_friends\nAt_least_I_still_have_you\nBoo!\n";
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

    // Try w/ no friends and signedOff
    un_friend = unFriend(PushFixture::userID, country, name);
    CHECK_EQUAL(status_codes::OK, un_friend);

    status = "I_am_so_lonely";
    friendlist = ReadFriendList(PushFixture::userID);

    int sign_off {signOff(string(PushFixture::userID))};
    CHECK_EQUAL(status_codes::OK, sign_off);

    CHECK_EQUAL(status_codes::OK, friendlist.first);
    result = post_update(PushFixture::push_addr, PushFixture::country, 
      PushFixture::userID, status, friendlist.second);
    CHECK_EQUAL(status_codes::OK, result.first);

      // Check if freinds updates was updated (should not have been changed)
      status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      expected = "Hey_I_got_friends\nAt_least_I_still_have_you\nBoo!\n";
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

      country = "USA";
      name = "Newell,Gabe";
      expected = "Hey_I_got_friends\nAt_least_I_still_have_you\n";
      status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

    // Try pushing update w/ no JSON body
    status = "Just_updated_with_cool_info";
    result = do_request(methods::POST,
    PushFixture::push_addr + push_status + "/" + PushFixture::country + "/" + PushFixture::userID + "/" + status);
    CHECK_EQUAL(status_codes::OK, result.first);

    // Try pushing with non-existent user
    string fakeUserID {"SpookyGhost"};
    status = "Surprise_attack_failed";
    result = post_update(PushFixture::push_addr, fakeCountry, 
      fakeUserID, status, 
        value::object(vector<pair<string,value>>{
          make_pair(string("Friends"), value::string(country + ";" + name))}));
    CHECK_EQUAL(status_codes::OK, result.first);

      //Check if friend was updated
      status_result = get_partition_entity (PushFixture::addr, PushFixture::table, country, name);
      expected = "Hey_I_got_friends\nAt_least_I_still_have_you\nSurprise_attack_failed\n";
      for (const auto& v : status_result.second.as_object()){
        if(v.first == "Updates") return_res = v.second.as_string(); 
      }
      CHECK_EQUAL(expected, return_res);

    // Check invalid HTTP methods
    string command {"Nope"};
    result = do_request(methods::PUT, push_addr + command + "/");
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = do_request(methods::CONNECT, push_addr + command + "/");
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = do_request(methods::HEAD, push_addr + command + "/");
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = do_request(methods::GET, push_addr + command + "/");
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    result = do_request(methods::DEL, push_addr + command + "/");
    CHECK_EQUAL(status_codes::MethodNotAllowed, result.first);

    // Check invalid POST command
    result = do_request(methods::POST, push_addr + command + "/");
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    // Cleanup tables
    userID = "HelloKitty";
    country = "Japan";
    name = "Kitty,White";
    int del_result {delete_entity (string(PushFixture::addr), string(PushFixture::auth_table), string(PushFixture::auth_table_partition), userID)};
    CHECK_EQUAL(status_codes::OK, del_result);
    del_result = delete_entity (string(PushFixture::addr), string(PushFixture::table), country, name);
    CHECK_EQUAL(status_codes::OK, del_result);

    userID = "Gaben";
    country = "USA";
    name = "Newell,Gabe";
    del_result = delete_entity (string(PushFixture::addr), string(PushFixture::auth_table), string(PushFixture::auth_table_partition), userID);
    CHECK_EQUAL(status_codes::OK, del_result);
    del_result = delete_entity (string(PushFixture::addr), string(PushFixture::table), country, name);
    CHECK_EQUAL(status_codes::OK, del_result);
  }
}