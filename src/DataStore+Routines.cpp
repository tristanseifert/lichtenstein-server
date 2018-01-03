#include "DataStore.h"

#include <glog/logging.h>
#include <sqlite3.h>

#include <vector>

using namespace std;


/**
 * Returns all routines in the datastore in a vector.
 */
vector<DataStore::Routine *> DataStore::getAllRoutines() {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DataStore::Routine *> routines;

	// execute the query
	err = this->sqlPrepare("SELECT * FROM routines;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// execute the query
	while((result = this->sqlStep(statement)) == SQLITE_ROW) {
		// create the routine, populate it, and add it to the vector
		DataStore::Routine *routine = new DataStore::Routine();
		this->_routineFromRow(statement, routine);

		routines.push_back(routine);
	}

	// free our statement
	this->sqlFinalize(statement);

	return routines;
}

/**
 * Finds a routine with the given id. If no such group exists, nullptr is returned.
 */
DataStore::Routine *DataStore::findRoutineWithId(int id) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// check whether the node exists
	if(this->_groupWithIdExists(id) == false) {
		return nullptr;
	}

	// allocate the object for later
	DataStore::Routine *routine = nullptr;

	// it exists, so we must now get it from the db
	err = this->sqlPrepare("SELECT * FROM routines WHERE id = ?1;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = sqlite3_bind_int(statement, 1, id);
	CHECK(err == SQLITE_OK) << "Couldn't bind group id: " << sqlite3_errstr(err);

	// execute the query
	result = this->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// populate the node object
		routine = new DataStore::Routine();
		this->_routineFromRow(statement, routine);
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
void DataStore::updateRoutine(DataStore::Routine *routine) {
	// does the routine exist?
	if(routine->id != 0) {
		// it does, so we can just update it
		this->_updateRoutine(routine);
	} else {
		// it doesn't, so we need to create it
		this->_createRoutine(routine);
	}
}

/**
 * Creates a new routine in the database, then assigns the id value of the group
 * that was passed in.
 */
void DataStore::_createRoutine(DataStore::Routine *routine) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Creating new routine named " << routine->name;

	// prepare an update query
	err = this->sqlPrepare("INSERT INTO routines (name, lua) VALUES (:name, :lua);", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindRoutineToStatement(statement, routine);

	// then execute it
	result = this->sqlStep(statement);
	CHECK(result == SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	this->sqlFinalize(statement);

	// update the rowid
	result = sqlite3_last_insert_rowid(this->db);
	CHECK(result == 0) << "rowid for inserted routine is zero… this shouldn't happen.";

	routine->id = result;
}

/**
 * Updates an existing routine in the database. This replaces all fields except
 * for id.
 */
void DataStore::_updateRoutine(DataStore::Routine *routine) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Updating existing routine with id " << routine->id;

	// prepare an update query
	err = this->sqlPrepare("UPDATE routines SET name = :name, lua = :lua WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindRoutineToStatement(statement, routine);

	// then execute it
	result = this->sqlStep(statement);
	CHECK(result == SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	this->sqlFinalize(statement);
}

/**
 * Determines whether a group with the given id exists.
 */
bool DataStore::_routineWithIdExists(int id) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// prepare a count statement
	err = this->sqlPrepare("SELECT count(*) FROM routines WHERE id = ?1;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = sqlite3_bind_int(statement, 1, id);
	CHECK(err == SQLITE_OK) << "Couldn't bind group id: " << sqlite3_errstr(err);

	// execute the query
	result = this->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// retrieve the value of the first column (0-based)
		count = sqlite3_column_int(statement, 0);
	}

	// free our statement
	this->sqlFinalize(statement);

	// if count is nonzero, the routine exists
	CHECK(count < 2) << "Found " << count << " routines with the same id, this should never happen";
	return (count != 0);
}

/**
 * Copies the fields from a statement (which is currently returning a row) into
 * an existing routine object.
 */
void DataStore::_routineFromRow(sqlite3_stmt *statement, DataStore::Routine *routine) {
	int numColumns = sqlite3_column_count(statement);

	// iterate over all returned columns
	for(int i = 0; i < numColumns; i++) {
		// get the column name and see to which property it matches up
		const char *colName = sqlite3_column_name(statement, i);

		// is it the id column?
		if(strcmp(colName, "id") == 0) {
			routine->id = sqlite3_column_int(statement, i);
		}
		// is it the name column?
		else if(strcmp(colName, "name") == 0) {
			routine->name = this->_stringFromColumn(statement, i);
		}
		// is it the Lua code column?
		else if(strcmp(colName, "lua") == 0) {
			routine->lua = this->_stringFromColumn(statement, i);
		}
	}
}

/**
 * Binds the fields of a group object to a prepared query. The prepared query
 * is expected to have named parameters corresponding to all of the fields
 * in the group object; the "id" field is the only field that may be missing.
 */
void DataStore::_bindRoutineToStatement(sqlite3_stmt *statement, DataStore::Routine *routine) {
	int err, idx;

	// bind the name
	idx = sqlite3_bind_parameter_index(statement, ":name");
	CHECK(idx != 0) << "Couldn't resolve parameter name";

	err = sqlite3_bind_text(statement, idx, routine->name.c_str(), -1, SQLITE_TRANSIENT);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine name: " << sqlite3_errstr(err);

	// bind the lua code
	idx = sqlite3_bind_parameter_index(statement, ":lua");
	CHECK(idx != 0) << "Couldn't resolve parameter lua";

	err = sqlite3_bind_text(statement, idx, routine->lua.c_str(), -1, SQLITE_TRANSIENT);
	CHECK(err == SQLITE_OK) << "Couldn't bind routine Lua code: " << sqlite3_errstr(err);


	// optionally, also bind the id field
	err = this->sqlBind(statement, ":id", routine->id, true);
}

#pragma mark - Operators
/**
 * Compares whether two routines are equal; they are equal if they have the same
 * id; thus, this will not work if one of the routines hasn't been inserted into
 * the database yet.
 */
bool operator==(const DataStore::Routine& lhs, const DataStore::Routine& rhs) {
	return (lhs.id == rhs.id);
}

bool operator!=(const DataStore::Routine& lhs, const DataStore::Routine& rhs) {
	return !(lhs == rhs);
}

bool operator< (const DataStore::Routine& lhs, const DataStore::Routine& rhs) {
	return (lhs.id < rhs.id);
}
bool operator> (const DataStore::Routine& lhs, const DataStore::Routine& rhs) {
	return rhs < lhs;
}
bool operator<=(const DataStore::Routine& lhs, const DataStore::Routine& rhs) {
	return !(lhs > rhs);
}
bool operator>=(const DataStore::Routine& lhs, const DataStore::Routine& rhs) {
	return !(lhs < rhs);
}
