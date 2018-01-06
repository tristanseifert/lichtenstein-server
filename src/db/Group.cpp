#include "Group.h"
#include "DataStore.h"

#include <glog/logging.h>
#include <sqlite3.h>

#include <vector>
#include <iostream>

#include "json.hpp"

using namespace std;


#pragma mark - Public Query Interface
/**
 * Returns all groups in the datastore in a vector.
 */
vector<DbGroup *> DataStore::getAllGroups() {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DbGroup *> groups;

	// execute the query
	err = this->sqlPrepare("SELECT * FROM groups;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// execute the query
	while((result = this->sqlStep(statement)) == SQLITE_ROW) {
		// create the group, populate it, and add it to the vector
		DbGroup *group = new DbGroup(statement, this);

		groups.push_back(group);
	}

	// free our statement
	this->sqlFinalize(statement);

	return groups;
}

/**
 * Finds a group with the given id. If no such group exists, nullptr is returned.
 */
DbGroup *DataStore::findGroupWithId(int id) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// if id is zero or negative, return
	if(id <= 0) {
		return nullptr;
	}

	// check whether the group exists
/*	if(DbGroup::_idExists(id, this) == false) {
		return nullptr;
	}
*/

	// allocate the object for later
	DbGroup *group = nullptr;

	// it exists, so we must now get it from the db
	err = this->sqlPrepare("SELECT * FROM groups WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = this->sqlBind(statement, ":id", id);
	CHECK(err == SQLITE_OK) << "Couldn't bind group id: " << sqlite3_errstr(err);

	// execute the query
	result = this->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// populate the group object
		group = new DbGroup(statement, this);
	}

	// free our statement
	this->sqlFinalize(statement);

	// return the populated group object
	return group;
}

/**
 * Updates the specified group. If a group with this id already exists (as would
 * be expected if it was previously fetched from the database) the existing
 * group is updated. Otherwise, a new group is created.
 */
void DataStore::update(DbGroup *group) {
	// does the group exist?
	if(group->id != 0) {
		// it does, so we can just update it
		group->_update(this);
	} else {
		// it doesn't, so we need to create it
		group->_create(this);
	}
}

#pragma mark - Private Query Interface
/**
 * Creates a new group in the database, then assigns the id value of the group
 * that was passed in.
 */
void DbGroup::_create(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Creating new group named " << this->name;

	// prepare an update query
	err = db->sqlPrepare("INSERT INTO groups (name, enabled, start, end, currentRoutine) VALUES (:name, :enabled, :start, :end, :routine);", &statement);
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
	CHECK(result != 0) << "rowid for inserted group is zero… this shouldn't happen.";

	this->id = result;
}

/**
 * Updates an existing group in the database. This replaces all fields except
 * for id.
 */
void DbGroup::_update(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Updating existing group with id " << this->id;

	// prepare an update query
	err = db->sqlPrepare("UPDATE groups SET name = :name, enabled = :enabled, start = :start, end = :end, currentRoutine = :routine WHERE id = :id;", &statement);
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
 * Determines whether a group with the given id exists.
 */
bool DbGroup::_idExists(int id, DataStore *db) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// prepare a count statement
	err = db->sqlPrepare("SELECT count(*) FROM groups WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = db->sqlBind(statement, ":id", id);
	CHECK(err == SQLITE_OK) << "Couldn't bind group id: " << sqlite3_errstr(err);

	// execute the query
	result = db->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// retrieve the value of the first column (0-based)
		count = db->sqlGetColumnInt(statement, 0);
	}

	// free our statement
	db->sqlFinalize(statement);

	// if count is nonzero, the group exists
	CHECK(count < 2) << "Found " << count << " groups with the same id, this should never happen";
	return (count != 0);
}

/**
 * Copies the fields from a statement (which is currently returning a row) into
 * an existing group object.
 */
void DbGroup::_fromRow(sqlite3_stmt *statement, DataStore *db) {
	int numColumns = db->sqlGetNumColumns(statement);

	// iterate over all returned columns
	for(int i = 0; i < numColumns; i++) {
		// get the column name and see to which property it matches up
		string colName = db->sqlColumnName(statement, i);

		// is it the id column?
		if(colName == "id") {
			this->id = db->sqlGetColumnInt(statement, i);
		}
		// is it the name column?
		else if(colName == "name") {
			this->name = db->sqlGetColumnString(statement, i);
		}
		// is it the enabled column?
		else if(colName == "enabled") {
			this->enabled = (db->sqlGetColumnInt(statement, i) != 0);
		}
		// is it the framebuffer starting offset column?
		else if(colName == "start") {
			this->start = db->sqlGetColumnInt(statement, i);
		}
		// is it the framebuffer ending offset column?
		else if(colName == "end") {
			this->end = db->sqlGetColumnInt(statement, i);
		}
		// is it the current routine column?
		else if(colName == "currentRoutine") {
			this->currentRoutineId = db->sqlGetColumnInt(statement, i);

			// fetch the appropriate routine from the database
			this->currentRoutine = db->findRoutineWithId(this->currentRoutineId);
		}
	}
}

/**
 * Binds the fields of a group object to a prepared query. The prepared query
 * is expected to have named parameters corresponding to all of the fields
 * in the group object; the "id" field is the only field that may be missing.
 */
void DbGroup::_bindToStatement(sqlite3_stmt *statement, DataStore *db) {
	int err, idx;

	// bind the name
	err = db->sqlBind(statement, ":name", this->name);
	CHECK(err == SQLITE_OK) << "Couldn't bind group name: " << sqlite3_errstr(err);

	// bind the enabled flag
	err = db->sqlBind(statement, ":enabled", (this->enabled ? 1 : 0));
	CHECK(err == SQLITE_OK) << "Couldn't bind group enabled status: " << sqlite3_errstr(err);

	// bind the framebuffer starting offset
	err = db->sqlBind(statement, ":start", this->start);
	CHECK(err == SQLITE_OK) << "Couldn't bind group start: " << sqlite3_errstr(err);

	// bind the framebuffer ending offset
	err = db->sqlBind(statement, ":end", this->end);
	CHECK(err == SQLITE_OK) << "Couldn't bind group end: " << sqlite3_errstr(err);

	// get the id of the attached routine and bind its ID
	if(this->currentRoutine != nullptr) {
		this->currentRoutineId = this->currentRoutine->id;
	} else {
		this->currentRoutineId = 0;
	}

	err = db->sqlBind(statement, ":routine", this->currentRoutineId);
	CHECK(err == SQLITE_OK) << "Couldn't bind group routine: " << sqlite3_errstr(err);


	// optionally, also bind the id field
	err = db->sqlBind(statement, ":id", this->id, true);
	CHECK(err == SQLITE_OK) << "Couldn't bind group id: " << sqlite3_errstr(err);
}

#pragma mark - Operators
/**
 * Compares whether two groups are equal; they are equal if they have the same
 * id; thus, this will not work if one of the groups hasn't been inserted into
 * the database yet.
 */
bool operator==(const DbGroup& lhs, const DbGroup& rhs) {
	return (lhs.id == rhs.id);
}

bool operator!=(const DbGroup& lhs, const DbGroup& rhs) {
	return !(lhs == rhs);
}

bool operator< (const DbGroup& lhs, const DbGroup& rhs) {
	return (lhs.id < rhs.id);
}
bool operator> (const DbGroup& lhs, const DbGroup& rhs) {
	return rhs < lhs;
}
bool operator<=(const DbGroup& lhs, const DbGroup& rhs) {
	return !(lhs > rhs);
}
bool operator>=(const DbGroup& lhs, const DbGroup& rhs) {
	return !(lhs < rhs);
}

/**
 * Outputs the some info about the group to the output stream.
 */
ostream &operator<<(ostream& strm, const DbGroup& obj) {
	strm << "group id " << obj.id << "{name = " << obj.name << ", range = ["
		 << obj.start << ", " << obj.end << "]}";

	return strm;
}
