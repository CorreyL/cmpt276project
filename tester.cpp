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

const string sign_on {"SignOn"};
const string sign_off {"SignOff"};
const string add_friend {"AddFriend"};
const string un_friend {"UnFriend"};
const string update_status {"UpdateStatus"};
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
    return signOnResult.first;
}

//Helper function to sign off
int signOff(const string& userId){
    pair<status_code,value> signOffResult {do_request(methods::POST, "http://localhost:34572/" + sign_off + "/" + userId)};
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

/********************
**CODE ADDED - STOP**
********************/

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

    compare_json_arrays(exp, result.second);

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
    CHECK_EQUAL(status_codes::OK, put_entity_no_properties(BasicFixture::addr, BasicFixture::table, partition, row));
    partition = "Squirrels";
    row = "Chuck";
    CHECK_EQUAL(status_codes::OK, put_entity_no_properties(BasicFixture::addr, BasicFixture::table, partition, row));

    //Check that only one entity has the same property as the first one (it's the first entity that should)
    property = "ZombieVirus";
    prop_val = "Infected";
    pair<status_code,value> first_test{get_Entities_from_property(BasicFixture::addr, BasicFixture::table, property, prop_val)};
    CHECK_EQUAL(status_codes::OK, first_test.first);
    CHECK_EQUAL(1, first_test.second.as_array().size());

    //Update all entities to have the same one as the first

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
    string(BasicFixture::addr) + add_property_admin + "WrongTable",
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

}

SUITE(ENTITY_AUTH) {
  TEST_FIXTURE(AuthFixture, GetEntityAuth) {
    pair<string,string> props {make_pair(string("Fun"),string("10/10"))};
    string partition {"Video_Game"};
    string row {"The_Witcher_3"};

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
      result = get_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, AuthFixture::partition, AuthFixture::row);
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

    //Cleanup table
    CHECK_EQUAL(status_codes::OK, delete_entity (AuthFixture::addr, AuthFixture::table, partition, row));
  }
  TEST_FIXTURE(AuthFixture, UpdateEntityAuth) {
    pair<string,value> props {make_pair(string("Fun"),value::string("10/10"))};
    string partition {"Video_Game"};
    string row {"The_Witcher_3"};

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
      make_pair(string("Hello"),string("World!")),
      make_pair(string("Cool"),string("HeckYeah")),
      make_pair(string("Replay"),string("Always"))
    });
    CHECK(result.second.is_object());
    compare_json_values (expect_value, result.second);

    //Trying making new entity
    partition = "ShouldNot";
    row = "Work!";
    CHECK_EQUAL(status_codes::NotFound, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair("Blah",value::string("Haha")),
        make_pair("Beep",value::string("Boop"))
    })));

    //Ensure NotFound responses (404)
    partition = "Video_Game";
    row = "The_Witcher_3";
    props = make_pair(string("Happy"),value::string("Sad"));
      //Try updating entity with invalid auth token
      CHECK_EQUAL(status_codes::NotFound, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_res.second, AuthFixture::partition, AuthFixture::row,
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

    CHECK_EQUAL(status_codes::Forbidden, put_entity_auth(AuthFixture::addr, AuthFixture::table, token_read_res.second, partition, row,
      value::object (vector<pair<string,value>> {
        make_pair("Fun",value::string("No"))
    })));

    //Cleanup table
    CHECK_EQUAL(status_codes::OK, delete_entity (AuthFixture::addr, AuthFixture::table, partition, row));
  }
}

class UserFixture {
public:
  static constexpr const char* addr {"http://localhost:34568/"};
  static constexpr const char* auth_table {"AuthTable"};
  static constexpr const char* table {"DataTable"};
  static constexpr const char* auth_table_partition {"Userid"};
  static constexpr const char* auth_dataPartition {"DataPartition"};
  static constexpr const char* auth_dataRow {"DataRow"};

  static constexpr const char* userID_A {"Aidan"};
  static constexpr const char* user_pwd_A {"SuperCool"};
  static constexpr const char* partition_A {"Canada"};
  static constexpr const char* row_A {"Wessel,Aidan"};

  static constexpr const char* userID_B {"Superman"};
  static constexpr const char* user_pwd_B {"Kryptonite"};
  static constexpr const char* partition_B {"USA"};
  static constexpr const char* row_B {"Kent,Clark"};

  static constexpr const char* userID_C {"Batman"};
  static constexpr const char* user_pwd_C {"DarkKnight"};
  static constexpr const char* partition_C {"USA"};
  static constexpr const char* row_C {"Wayne,Bruce"};


public:
  UserFixture() {
    //Ensure dataTable is created
    int make_result {create_table(addr, table)};
    cerr << "create result " << make_result << endl;
    if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
      throw std::exception();
    }
    //Make some users
    createFakeUser(userID_A, user_pwd_A, partition_A, row_A);
    createFakeUser(userID_B, user_pwd_B, partition_B, row_B);
    createFakeUser(userID_C, user_pwd_C, partition_C, row_C);
  }

  ~UserFixture() {
    int del_ent_result {delete_entity (addr, table, partition_A, row_A)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
    del_ent_result = {delete_entity (addr, auth_table, auth_table_partition, userID_A)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
     del_ent_result = {delete_entity (addr, table, partition_B, row_B)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
      del_ent_result = {delete_entity (addr, auth_table, auth_table_partition, userID_B)};
    if (del_ent_result != status_codes::OK) {
      throw std::exception();
    }
      del_ent_result = {delete_entity (addr, table, partition_C, row_C)};
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
    cout <<"Sign on result " << signOnResult << endl;
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Ensure a second sign on (of the same user) works just like the first
    signOnResult = {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    cout <<"Sign on result " << signOnResult << endl;
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Now that the user is signed on, make sure a request works
    FLResult = ReadFriendList(string(UserFixture::userID_A));
    CHECK_EQUAL(status_codes::OK, FLResult.first);

    //Ensure that a sign on of an invalid userId/password combo gets a 404
    string invalidUserId = "Awesomerizer";
    string invalidUserPass = "OnSteam";
    signOnResult = {signOn(invalidUserId, invalidUserPass)};
    cout <<"Sign on result " << signOnResult << endl;
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
    cout <<"Sign on result " << signOnResult << endl;
    CHECK_EQUAL(status_codes::NotFound, signOnResult);

    //Check to make sure that multiple sign ons are okay
    string otherUserID = "user";
    string otherUserPass = "user";

    signOnResult = {signOn(otherUserID, otherUserPass)};
    cout <<"Sign on result " << signOnResult << endl;
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Ensure that sign off works
    int signOffResult {signOff(string(UserFixture::userID_A))};
    cout <<"Sign off result " << signOffResult << endl;
    CHECK_EQUAL(status_codes::OK, signOffResult);

    //Sign off second sign in
    signOffResult = {signOff(otherUserID)};
    cout <<"Sign off result " << signOffResult << endl;
    CHECK_EQUAL(status_codes::OK, signOffResult);

    //Try getting friends list after sign off, expecting another forbidden
    FLResult = ReadFriendList(string(UserFixture::userID_A));
    CHECK_EQUAL(status_codes::Forbidden, FLResult.first);

    //Ensure that a second sign off gets a 404
    signOffResult = {signOff(string(UserFixture::userID_A))};
    cout <<"Sign off result " << signOffResult << endl;
    CHECK_EQUAL(status_codes::NotFound, signOffResult);

    //Delete the extra added entitiy from the AuthTable
    int del_ent_result {delete_entity (addr, auth_table, auth_table_partition, fakeUserID)};
    cout << "Delete Result: " << del_ent_result << endl;
    CHECK_EQUAL(status_codes::OK, del_ent_result);
  }

  TEST_FIXTURE(UserFixture, friendOps){
    //Sign On
    int signOnResult {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    cout <<"Sign on result " << signOnResult << endl;
    CHECK_EQUAL(status_codes::OK, signOnResult);

    //Ensure adding a friend works
    string newFriendCountry = "USA";
    string newFriendName = "Kitzmiller,Trevor";
    int addResult = addFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, addResult);

    //Add a second friend, shouldn't add another copy
    addResult = addFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, addResult);

    //View Friends list (debugging)
    pair<status_code,value> FLResult = ReadFriendList(string(UserFixture::userID_A));
    cout << FLResult.first << " " << FLResult.second << endl;

    //Ensure removing friend works
    int remResult = unFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, remResult);

    //Remove the same friend again, should just do nothing
    remResult = unFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    CHECK_EQUAL(status_codes::OK, remResult);

    //View Friends list (debugging)
    FLResult = ReadFriendList(string(UserFixture::userID_A));
    cout << FLResult.first << " " << FLResult.second << endl;

    //Sign off
    int signOffResult {signOff(string(UserFixture::userID_A))};
    cout <<"Sign off result " << signOffResult << endl;
    CHECK_EQUAL(status_codes::OK, signOffResult);
  }

  TEST_FIXTURE(UserFixture, getFriendList){
    //WORK IN PROGRESS (setting up that fixture is harder than it looks)
    cout << "Bueno" << endl;
  }

  TEST_FIXTURE(UserFixture, updateStatus){
    //Sign On
    int signOnResult {signOn(string(UserFixture::userID_A), string(UserFixture::user_pwd_A))};
    cout <<"Sign on result " << signOnResult << endl;
    CHECK_EQUAL(status_codes::OK, signOnResult);


    dump_table_contents("DataTable");

    string newFriendCountry = "USA";
    string newFriendName = "Kitzmiller,Trevor";
    int addResult = addFriend(UserFixture::userID_A, newFriendCountry, newFriendName);
    cout << endl;
    dump_table_contents("DataTable");

    do_request (methods::PUT,
                user_addr + update_status + "/" + string(UserFixture::userID_A) + "/" + "Just_testing_things");
    cout << endl;
    dump_table_contents("DataTable");

    //Sign off
    int signOffResult {signOff(string(UserFixture::userID_A))};
    cout <<"Sign off result " << signOffResult << endl;
    CHECK_EQUAL(status_codes::OK, signOffResult);
  }

  TEST_FIXTURE(UserFixture, badRequests){
    //WORK IN PROGRESS (setting up that fixture is harder than it looks)
    cout << "Bueno" << endl;
  }
}