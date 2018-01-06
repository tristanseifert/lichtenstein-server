#include "Routine.h"
#include "DataStore.h"

#include "json.hpp"

#include <glog/logging.h>
#include <sqlite3.h>

#include <vector>

using namespace std;
using json = nlohmann::json;


#pragma mark - Public Query Interface
/**
 * Returns all routines in the datastore in a vector.
 */
vector<DbRoutine *> DataStore::getAllRoutines() {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DbRoutine *> routines;

	// execute the query
	err = this->sqlPrepare("SELECT * FROM routines;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// execute the query
	while((result = this->sqlStep(statement)) == SQLITE_ROW) {
		// create the routine, populate it, and add it to the vector
		DbRoutine *routine = new DbRoutine(statement, this);

		routines.push_back(routine);
	}

	// free our statement
	this->sqlFinalize(statement);

	return routines;
}

/**
 * Finds a routine with the given id. If no such routine exists, nullptr is
 * returned.
 */
DbRoutine *DataStore::findRoutineWithId(int id) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// if id is zero or negative, return
	if(id <= 0) {
		return nullptr;
	}

	// check whether the routine exists
/*	if(DbRoutine::_idExists(id, this) == false) {
		return nullptr;
	}
*/

	// allocate the object for later
	DbRoutine *routine = nullptr;

	// it exists, so we must now get it from the db
	err = this->sqlPrepare("SELECT * FROM routines WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = this->sqlBind(statement, ":id", id);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine id: " << sqlite3_errstr(err);

	// execute the query
	result = this->sqlStep(statement);

	if(result == SQLITE_ROW) {
		routine = new DbRoutine(statement, this);
	}

	// free our statement
	this->sqlFinalize(statement);

	// return the populated routine object
	return routine;
}

/**
 * Updates the specified routine. If a routine with this id already exists (as
 * expected if it was previously fetched from the database) the existing routine
 * is updated. Otherwise, a new routine is created.
 */
void DataStore::update(DbRoutine *routine) {
	// convert the default properties back into a JSON object
	routine->_encodeJSON();

	// does the routine exist?
	if(routine->id != 0) {
		// it does, so we can just update it
		routine->_update(this);
	} else {
		// it doesn't, so we need to create it
		routine->_create(this);
	}
}

#pragma mark - Private Query Interface
/**
 * Creates a new routine in the database, then assigns the id value of the
 * routine that was passed in.
 */
void DbRoutine::_create(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Creating new routine named " << this->name;

	// prepare an update query
	err = db->sqlPrepare("INSERT INTO routines (name, code, defaultParams) VALUES (:name, :code, :defaultParams);", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindToStatement(statement, db);

	// then execute it
	result = db->sqlStep(statement);
	CHECK(result == SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	db->sqlFinalize(statement);

	// update the rowid
	result = db->sqlGetLastRowId();
	CHECK(result != 0) << "rowid for inserted routine is zero… this shouldn't happen.";

	this->id = result;
}

/**
 * Updates an existing routine in the database. This replaces all fields except
 * for id.
 */
void DbRoutine::_update(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Updating existing routine with id " << this->id;

	// prepare an update query
	err = db->sqlPrepare("UPDATE routines SET name = :name, code = :code, defaultParams = :defaultParams WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindToStatement(statement, db);

	// then execute it
	result = db->sqlStep(statement);
	CHECK(result == SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	db->sqlFinalize(statement);
}

/**
 * Determines whether a routine with the given id exists.
 */
bool DbRoutine::_idExists(int id, DataStore *db) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// prepare a count statement
	err = db->sqlPrepare("SELECT count(*) FROM routines WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = db->sqlBind(statement, ":id", id);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine id: " << sqlite3_errstr(err);

	// execute the query
	result = db->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// retrieve the value of the first column (0-based)
		count = sqlite3_column_int(statement, 0);
	}

	// free our statement
	db->sqlFinalize(statement);

	// if count is nonzero, the routine exists
	CHECK(count < 2) << "Found " << count << " routines with the same id, this should never happen";
	return (count != 0);
}

/**
 * Copies the fields from a statement (which is currently returning a row) into
 * an existing routine object.
 */
void DbRoutine::_fromRow(sqlite3_stmt *statement, DataStore *db) {
	int numColumns = sqlite3_column_count(statement);

	// iterate over all returned columns
	for(int i = 0; i < numColumns; i++) {
		// get the column name and see to which property it matches up
		const char *colName = sqlite3_column_name(statement, i);

		// is it the id column?
		if(strcmp(colName, "id") == 0) {
			this->id = sqlite3_column_int(statement, i);
		}
		// is it the name column?
		else if(strcmp(colName, "name") == 0) {
			this->name = db->_stringFromColumn(statement, i);
		}
		// is it the code column?
		else if(strcmp(colName, "code") == 0) {
			this->code = db->_stringFromColumn(statement, i);
		}
		// is it the default params column?
		else if(strcmp(colName, "defaultParams") == 0) {
			this->defaultParamsJSON = db->_stringFromColumn(statement, i);
			this->_decodeJSON();
		}
	}
}

/**
 * Binds the fields of a routine object to a prepared query. The prepared query
 * is expected to have named parameters corresponding to all of the fields
 * in the routine object; the "id" field is the only field that may be missing.
 */
void DbRoutine::_bindToStatement(sqlite3_stmt *statement, DataStore *db) {
	int err, idx;

	// bind the name
	err = db->sqlBind(statement, ":name", this->name);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine name: " << sqlite3_errstr(err);

	// bind the code
	err = db->sqlBind(statement, ":code", this->code);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine code: " << sqlite3_errstr(err);

	// bind the default params JSON string
	err = db->sqlBind(statement, ":defaultParams", this->defaultParamsJSON);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine default params: " << sqlite3_errstr(err);


	// optionally, also bind the id field
	err = db->sqlBind(statement, ":id", this->id, true);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine id: " << sqlite3_errstr(err);
}

#pragma mark - Property Handling
/**
 * Decodes the JSON from the `defaultParamsJSON` field into a map accessible by
 * external code.
 */
void DbRoutine::_decodeJSON() {
	// parse the JSON string
	try {
		json j = json::parse(this->defaultParamsJSON);

		// hacky approach since we can't just cast the json object to a map
		this->defaultParams.clear();

		for(json::iterator it = j.begin(); it != j.end(); ++it) {
			this->defaultParams[it.key()] = double(it.value());
		}
	} catch(json::parse_error e) {
		LOG(ERROR) << "JSON error in routine " << this->name << " defaults: "
		 		   << e.what();
	}
}

/**
 * Serializes the map of default parameters back into the `defaultParamsJSON`
 * field to be stored in the database.
 */
void DbRoutine::_encodeJSON() {
	// turn the map into a json object
	json j = json(this->defaultParams);

	// serialize it
	this->defaultParamsJSON = j.dump();
}

#pragma mark - Operators
/**
 * Compares whether two routines are equal; they are equal if they have the same
 * id; thus, this will not work if one of the routines hasn't been inserted into
 * the database yet.
 */
bool operator==(const DbRoutine& lhs, const DbRoutine& rhs) {
	return (lhs.id == rhs.id);
}

bool operator!=(const DbRoutine& lhs, const DbRoutine& rhs) {
	return !(lhs == rhs);
}

bool operator< (const DbRoutine& lhs, const DbRoutine& rhs) {
	return (lhs.id < rhs.id);
}
bool operator> (const DbRoutine& lhs, const DbRoutine& rhs) {
	return rhs < lhs;
}
bool operator<=(const DbRoutine& lhs, const DbRoutine& rhs) {
	return !(lhs > rhs);
}
bool operator>=(const DbRoutine& lhs, const DbRoutine& rhs) {
	return !(lhs < rhs);
}

/**
 * Outputs the some info about the routine to the output stream.
 */
ostream &operator<<(ostream& strm, const DbRoutine& obj) {
	strm << "routine id " << obj.id << "{name = " << obj.name << ", "
		 << obj.code.size() << " bytes of code}";

	return strm;
}
