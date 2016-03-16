/*
  Sample unit tests for BasicServer
 */

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

using web::json::value;

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
  pair<status_code,value> result {do_request (methods::POST, addr + "CreateTable/" + table)};
  return result.first;
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
		addr + "DeleteTable/" + table)};
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
		addr + "UpdateEntity/" + table + "/" + partition + "/" + row,
		value::object (vector<pair<string,value>>
			       {make_pair(prop, value::string(pstring))}))};
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
		addr + "DeleteEntity/" + table + "/" + partition + "/" + row)};
  return result.first;
}
/********************* 
**CODE ADDED - BEGIN**
**********************/
pair<status_code,value> get_partition_entity (const string& addr, const string& table, const string& partition, const string& row){
	pair<status_code,value> result {do_request(methods::GET, addr + table + "/" + partition + "/" + row) };
	return result;
}

pair<status_code,value> get_Entities_from_property (const string& addr, const string& table, const string& prop, const string& pstring){
	pair<status_code,value> result { do_request(methods::GET, addr + table, value::object(vector<pair<string,value>> {make_pair(prop, value::string(pstring))}))};
	return result;
}

pair<status_code,value> get_spec_properties_entity (const string& addr, const string& table, const value& properties){
  pair<status_code,value> result { do_request(methods::GET, addr + table, properties)};
  return result;
}

int put_multi_properties_entity (const string& addr, const string& table, const string& partition, const string& row, const value& properties){
  pair<status_code,value> result { 
    do_request(methods::PUT, 
      addr + "UpdateEntity/" + table + "/" + partition + "/" + row, properties)};
  return result.first;
}

int update_property (const string& addr, const string& table, const value& properties){
  pair<status_code,value> result { 
    do_request(methods::PUT, 
      addr + "UpdateProperty/" + table, properties)};
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
    addr + "UpdateEntity/" + table + "/" + partition + "/" + row)};
  return result.first;
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
  class GetFixture {
  public:
    static constexpr const char* addr {"http://127.0.0.1:34568/"};
    static constexpr const char* table {"TestTable"};
    static constexpr const char* partition {"Franklin,Aretha"};
    static constexpr const char* row {"USA"};
    static constexpr const char* property {"Song"};
    static constexpr const char* prop_val {"RESPECT"};
		static constexpr const char* row_def {"*"};

  public:
    GetFixture() {
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
    ~GetFixture() {
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
  TEST_FIXTURE(GetFixture, GetSingle) {
    pair<status_code,value> result {
      do_request (methods::GET,
		  string(GetFixture::addr)
		  + GetFixture::table + "/"
		  + GetFixture::partition + "/"
		  + GetFixture::row)};
      
      CHECK_EQUAL(string("{\"")
		  + GetFixture::property
		  + "\":\""
		  + GetFixture::prop_val
		  + "\"}",
		  result.second.serialize());
      CHECK_EQUAL(status_codes::OK, result.first);
    }
  /*
    A test of GET all table entries
   */
  TEST_FIXTURE(GetFixture, GetAll) {
    string partition {"Katherines,The"};
    string row {"Canada"};
    string property {"Home"};
    string prop_val {"Vancouver"};
    int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> result {
      do_request (methods::GET,
		  string(GetFixture::addr)
		  + string(GetFixture::table))};
    CHECK(result.second.is_array());
    CHECK_EQUAL(2, result.second.as_array().size());
		
    /*
      Checking the body is not well-supported by UnitTest++, as we have to test
      independent of the order of returned values.
     */
    //CHECK_EQUAL(body.serialize(), string("{\"")+string(GetFixture::property)+ "\":\""+string(GetFixture::prop_val)+"\"}");
    CHECK_EQUAL(status_codes::OK, result.first);

    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
  }
	/********************* 
	**CODE ADDED - BEGIN**
	**********************/
	/*
	A test of GET entities of specified partition
	*/
	
	TEST_FIXTURE(GetFixture, GetPartition){
		string partition = "Video_Game";
		string row {"The_Witcher_3"};
    string property {"Rating"};
    string prop_val {"10_Out_Of_10"};

    //Test to make sure if the partition does not exist, a 404 NotFound code is recieved
    pair<status_code,value> test_result {get_partition_entity(string(GetFixture::addr), string(GetFixture::table), partition, "*")};
    CHECK_EQUAL(status_codes::NotFound, test_result.first);

    //Ensure bad requests get a 400 response (no partition name)
    test_result = do_request (methods::GET, string(GetFixture::addr) + string(GetFixture::table) + "/" + row);
    CHECK_EQUAL(status_codes::BadRequest, test_result.first);

		//Add an element, check GET works
		int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);
		
		test_result = get_partition_entity(string(GetFixture::addr), string(GetFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(1, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add a second element, check the GET returns both elements in the partition
		row = "Fire_Emblem";
    prop_val = "8_Out_Of_10";

    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_partition_entity(string(GetFixture::addr), string(GetFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add a third element that is NOT a member of the same partition, ensure that it is not returned with the other two
    partition = "Aidan";
    row = "Canada";
    property = "Home";
    prop_val = "Surrey";
    put_result = put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);


    partition = "Video_Game";
    test_result = get_partition_entity(string(GetFixture::addr), string(GetFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add a fourth and final element to ensure that adding a non-partition element does not mess up gets of the next (partitioned) elements
    //Also tests if it can return an entity with no properties
    row = "Call_Of_Duty";
    prop_val = "5_Out_Of_10";

    put_result = put_entity_no_properties(GetFixture::addr, GetFixture::table, partition, row);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_partition_entity(string(GetFixture::addr), string(GetFixture::table), partition, "*");
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(3, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);


    //Clear Table
    row = "The_Witcher_3";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    row = "Fire_Emblem";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    partition = "Aidan";
    row = "Canada";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    partition = "Video_Game";
    row = "Call_Of_Duty";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
  }

  TEST_FIXTURE(GetFixture, AddPropertyToAll){
    string partition {"Humans"};
    string row {"PatientZero"};
    string property {"ZombieVirus"};
    string prop_val {"Infected"};

    //Add an entity with a property, one with a property that is different than the first one,
    //one without properties, and one with no properties in a different partition
    CHECK_EQUAL(status_codes::OK, put_entity(GetFixture::addr, GetFixture::table, partition, row, property, prop_val));
    row = "Michael";
    property = "HasHair";
    prop_val = "Yup";
    CHECK_EQUAL(status_codes::OK, put_entity(GetFixture::addr, GetFixture::table, partition, row, property, prop_val));
    row = "Aidan";
    CHECK_EQUAL(status_codes::OK, put_entity_no_properties(GetFixture::addr, GetFixture::table, partition, row));
    partition = "Squirrels";
    row = "Chuck";
    CHECK_EQUAL(status_codes::OK, put_entity_no_properties(GetFixture::addr, GetFixture::table, partition, row));

    //Check that only one entity has the same property as the first one (it's the first entity that should)
    property = "ZombieVirus";
    prop_val = "Infected";
    pair<status_code,value> first_test{get_Entities_from_property(GetFixture::addr, GetFixture::table, property, prop_val)};
    CHECK_EQUAL(1, first_test.second.as_array().size());

    //Update all entities to have the same one as the first
    pair<status_code,value> result = {
    do_request (methods::PUT,
    string(GetFixture::addr) + "AddProperty/" + string(GetFixture::table), value::object (vector<pair<string,value>>
             {make_pair(property, value::string(prop_val))}))};

    //Check that all entities now have the added property (It's 5 because Franklin Aretha got infected too, poor guy)
    pair<status_code,value> second_test = {get_Entities_from_property(GetFixture::addr, GetFixture::table, property, prop_val)};
    CHECK_EQUAL(5, second_test.second.as_array().size());

    //Check that an invalid AddProperty gets a 400 code   
    //Invalid because no table specified
    result = {
    do_request (methods::PUT,
    string(GetFixture::addr) + "AddProperty/")};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Invalid because no JSON body
    result = {
    do_request (methods::PUT,
    string(GetFixture::addr) + "AddProperty/" + string(GetFixture::table))};
    CHECK_EQUAL(status_codes::BadRequest, result.first);

    //Ensure if the table does not exist a 404 code is recieved
    result = {
    do_request (methods::PUT,
    string(GetFixture::addr) + "AddProperty/" + "WrongTable",
    value::object (vector<pair<string,value>>{make_pair(property, value::string(prop_val))}))};
    CHECK_EQUAL(status_codes::NotFound, result.first);

    //Clean up Table -- Extra deletes are because sometimes these entities refuse to be deleted (only this test, for some reason)
    //Especially patient zero. Why? Can't figure it out.
    partition = "Humans";
    row = "PatientZero";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    row = "Michael";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    row = "Aidan";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    partition = "Squirrels";
    row = "Chuck";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    partition = "Humans";
    row = "PatientZero";
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
    delete_entity (GetFixture::addr, GetFixture::table, partition, row);
  }

  /*
  Test get all entities with specific properties
  */
  TEST_FIXTURE(GetFixture, GetEntityWithSpecProperties){
    string partition {"Cat"};
    string row {"Domestic"};

    int put_result { put_multi_properties_entity(GetFixture::addr, GetFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("10/10")),
        make_pair("Huggable", value::string("8/10")),
        make_pair("Furball", value::string("11/10"))
    }))};
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    pair<status_code,value> test_result { get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
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

    put_result = put_multi_properties_entity(GetFixture::addr, GetFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("7/10"))
    }));
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(1, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Add another entity with both specific property in different order
    row = "Domestic";

    put_result = put_multi_properties_entity(GetFixture::addr, GetFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Huggable", value::string("7/10")),
        make_pair("Likeable", value::string("7.5/10")),
        make_pair("Cute", value::string("8/10"))
    }));
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
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

    put_result = put_multi_properties_entity(GetFixture::addr, GetFixture::table, partition, row, 
      value::object(vector<pair<string,value>> {
        make_pair("Tough", value::string("9/10"))
    }));
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
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

    put_result = put_entity_no_properties(GetFixture::addr, GetFixture::table, partition, row);
    cerr << "put result " << put_result << endl;
    assert (put_result == status_codes::OK);

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(2, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Test result with no JSON body
    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, value::object(vector<pair<string,value>> {}));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(6, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Test after deleting an entity with the specfic properties
    partition = "Cat";
    row = "Domestic";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(1, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);


    //Test result where no specfic properties is found
    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table,
      value::object(vector<pair<string,value>> {
        make_pair("Scary", value::string("*")),
        make_pair("Deadly", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(0, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);

    //Test result where table does not exist
    test_result = get_spec_properties_entity(GetFixture::addr, "Unknown",
      value::object(vector<pair<string,value>> {
        make_pair("Cute", value::string("*")),
        make_pair("Huggable", value::string("*"))
    }));
    CHECK_EQUAL(status_codes::NotFound, test_result.first);

    //Test result where no table name
    test_result = get_spec_properties_entity(GetFixture::addr, "", value::object(vector<pair<string,value>> {}));
    CHECK_EQUAL(status_codes::BadRequest, test_result.first);

    //Cleanup tables
    partition = "Pig";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    partition = "Bunny";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    row = "Wild";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    partition = "Dog";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));

  }

  /*
  Test update property value
  */
  TEST_FIXTURE(GetFixture, UpdateProperties){
    string partition {"Japanese"};
    string row {"Nintendo"};
    string property {"Fun"};
    string prop_val {"Yes"};

    CHECK_EQUAL(status_codes::OK, put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val));
    row = "PlayStation";
    CHECK_EQUAL(status_codes::OK, put_multi_properties_entity (GetFixture::addr, GetFixture::table, partition, row,
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("Yes")),
        make_pair("Cool", value::string("Yes"))
    })));
    partition = "Outdoors";
    row = "Running";
    CHECK_EQUAL(status_codes::OK, put_entity_no_properties (GetFixture::addr, GetFixture::table, partition, row));
    partition = "American";
    row = "Xbox";
    CHECK_EQUAL(status_codes::OK, put_multi_properties_entity (GetFixture::addr, GetFixture::table, partition, row,
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("No")),
        make_pair("Cool", value::string("No")),
        make_pair("Boring", value::string("No"))
    })));
    partition = "Indoors";
    row = "Volleyball";
    property = "Boring";
    prop_val = "No";
    CHECK_EQUAL(status_codes::OK, put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val));

    pair<status_code,value> test_result { get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
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

    //Update the property
    CHECK_EQUAL(status_codes::OK, update_property (GetFixture::addr, GetFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("Yes"))
    })));

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Fun", value::string("*"))
    }));
    CHECK(test_result.second.is_array());
    CHECK_EQUAL(3, test_result.second.as_array().size());
    CHECK_EQUAL(status_codes::OK, test_result.first);
    count = 0;
    for(auto &p : test_result.second.as_array()){
      if(p.at("Fun") == value::string("Yes")) count++;
    }
    CHECK_EQUAL(3, count);

    //Test result after updating multiple values
    CHECK_EQUAL(status_codes::OK, update_property (GetFixture::addr, GetFixture::table, 
      value::object(vector<pair<string,value>> {
        make_pair("Boring", value::string("Yes")),
        make_pair("Cool", value::string("No"))
    })));

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, 
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

    test_result = get_spec_properties_entity(GetFixture::addr, GetFixture::table, value::object(vector<pair<string,value>> {}));
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
    CHECK_EQUAL(status_codes::BadRequest, update_property (GetFixture::addr, GetFixture::table, value::object(vector<pair<string,value>> {})));

    //Test result without table name
    CHECK_EQUAL(status_codes::BadRequest, update_property (GetFixture::addr, "", value::object(vector<pair<string,value>> {})));

    //Test result where table does not exist
    CHECK_EQUAL(status_codes::NotFound, update_property (GetFixture::addr, "Unknown", 
      value::object(vector<pair<string,value>> {
        make_pair("Message", value::string("Hi"))
      })));

    //Cleanup tables
    partition = "Japanese";
    row = "Nintendo";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    row = "PlayStation";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    partition = "Outdoors";
    row = "Running";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    partition = "American";
    row = "Xbox";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
    partition = "Indoors";
    row = "Volleyball";
    CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));

  }
}

	/********************
	**CODE ADDED - STOP**
	********************/

/*
  Locate and run all tests
 */
int main(int argc, const char* argv[]) {
  return UnitTest::RunAllTests();
}
