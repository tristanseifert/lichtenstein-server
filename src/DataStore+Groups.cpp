#include "DataStore.h"

#include <glog/logging.h>
#include <sqlite3.h>

#include <vector>

using namespace std;


/**
 * Returns all groups in the datastore in a vector.
 */
vector<DataStore::Group *> DataStore::getAllGroups() {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DataStore::Group *> groups;

	// execute the query
	err = this->sqlPrepare("SELECT * FROM groups;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// execute the query
	while((result = this->sqlStep(statement)) == SQLITE_ROW) {
		// create the group, populate it, and add it to the vector
		DataStore::Group *group = new DataStore::Group();
		this->_groupFromRow(statement, group);

		groups.push_back(group);
	}

	// free our statement
	this->sqlFinalize(statement);

	return groups;
}

/**
 * Finds a group with the given id. If no such group exists, nullptr is returned.
 */
DataStore::Group *DataStore::findGroupWithId(int id) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// check whether the node exists
	if(this->_groupWithIdExists(id) == false) {
		return nullptr;
	}

	// allocate the object for later
	DataStore::Group *group = nullptr;

	// it exists, so we must now get it from the db
	err = this->sqlPrepare("SELECT * FROM groups WHERE id = ?1;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = sqlite3_bind_int(statement, 1, id);
	CHECK(err == SQLITE_OK) << "Couldn't bind group id: " << sqlite3_errstr(err);

	// execute the query
	result = this->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// populate the group object
		group = new DataStore::Group();
		this->_groupFromRow(statement, group);
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
void DataStore::updateGroup(DataStore::Group *group) {
	// does the group exist?
	if(group->id != 0) {
		// it does, so we can just update it
		this->_updateGroup(group);
	} else {
		// it doesn't, so we need to create it
		this->_createGroup(group);
	}
}

/**
 * Creates a new group in the database, then assigns the id value of the group
 * that was passed in.
 */
void DataStore::_createGroup(DataStore::Group *group) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Creating new group named " << group->name;

	// prepare an update query
	err = this->sqlPrepare("INSERT INTO groups (name, enabled, start, end, currentRoutine) VALUES (:name, :enabled, :start, :end, :routine);", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindGroupToStatement(statement, group);

	// then execute it
	result = this->sqlStep(statement);
	CHECK(result == SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	this->sqlFinalize(statement);

	// update the rowid
	result = sqlite3_last_insert_rowid(this->db);
	CHECK(result == 0) << "rowid for inserted group is zero… this shouldn't happen.";

	group->id = result;
}

/**
 * Updates an existing group in the database. This replaces all fields except
 * for id.
 */
void DataStore::_updateGroup(DataStore::Group *group) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Updating existing group with id " << group->id;

	// prepare an update query
	err = this->sqlPrepare("UPDATE groups SET name = :name, enabled = :enabled, start = :start, end = :end, currentRoutine = :routine WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindGroupToStatement(statement, group);

	// then execute it
	result = this->sqlStep(statement);
	CHECK(result == SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	this->sqlFinalize(statement);
}

/**
 * Determines whether a group with the given id exists.
 */
bool DataStore::_groupWithIdExists(int id) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// prepare a count statement
	err = this->sqlPrepare("SELECT count(*) FROM groups WHERE id = ?1;", &statement);
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

	// if count is nonzero, the group exists
	CHECK(count < 2) << "Found " << count << " groups with the same id, this should never happen";
	return (count != 0);
}

/**
 * Copies the fields from a statement (which is currently returning a row) into
 * an existing group object.
 */
void DataStore::_groupFromRow(sqlite3_stmt *statement, DataStore::Group *group) {
	int numColumns = sqlite3_column_count(statement);

	// iterate over all returned columns
	for(int i = 0; i < numColumns; i++) {
		// get the column name and see to which property it matches up
		const char *colName = sqlite3_column_name(statement, i);

		// is it the id column?
		if(strcmp(colName, "id") == 0) {
			group->id = sqlite3_column_int(statement, i);
		}
		// is it the name column?
		else if(strcmp(colName, "name") == 0) {
			group->name = this->_stringFromColumn(statement, i);
		}
		// is it the enabled column?
		else if(strcmp(colName, "enabled") == 0) {
			group->enabled = (sqlite3_column_int(statement, i) != 0);
		}
		// is it the framebuffer starting offset column?
		else if(strcmp(colName, "start") == 0) {
			group->start = sqlite3_column_int(statement, i);
		}
		// is it the framebuffer ending offset column?
		else if(strcmp(colName, "end") == 0) {
			group->end = sqlite3_column_int(statement, i);
		}
		// is it the current routine column?
		else if(strcmp(colName, "currentRoutine") == 0) {
			group->currentRoutine = sqlite3_column_int(statement, i);
		}
	}
}

/**
 * Binds the fields of a group object to a prepared query. The prepared query
 * is expected to have named parameters corresponding to all of the fields
 * in the group object; the "id" field is the only field that may be missing.
 */
void DataStore::_bindGroupToStatement(sqlite3_stmt *statement, DataStore::Group *group) {
	int err, idx;

	// bind the name
	idx = sqlite3_bind_parameter_index(statement, ":name");
	CHECK(idx != 0) << "Couldn't resolve parameter name";

	err = sqlite3_bind_text(statement, idx, group->name.c_str(), -1, SQLITE_TRANSIENT);
	CHECK(err == SQLITE_OK) << "Couldn't bind group name: " << sqlite3_errstr(err);

	// bind the enabled flag
	idx = sqlite3_bind_parameter_index(statement, ":enabled");
	CHECK(idx != 0) << "Couldn't resolve parameter enabled";

	err = sqlite3_bind_int(statement, idx, group->enabled ? 1 : 0);
	CHECK(err == SQLITE_OK) << "Couldn't bind group enabled status: " << sqlite3_errstr(err);

	// bind the framebuffer starting offset
	idx = sqlite3_bind_parameter_index(statement, ":start");
	CHECK(idx != 0) << "Couldn't resolve parameter start";

	err = sqlite3_bind_int(statement, idx, group->start);
	CHECK(err == SQLITE_OK) << "Couldn't bind group start: " << sqlite3_errstr(err);

	// bind the framebuffer ending offset
	idx = sqlite3_bind_parameter_index(statement, ":end");
	CHECK(idx != 0) << "Couldn't resolve parameter end";

	err = sqlite3_bind_int(statement, idx, group->end);
	CHECK(err == SQLITE_OK) << "Couldn't bind group end: " << sqlite3_errstr(err);

	// bind the current routine
	idx = sqlite3_bind_parameter_index(statement, ":routine");
	CHECK(idx != 0) << "Couldn't resolve parameter routine";

	err = sqlite3_bind_int(statement, idx, group->currentRoutine);
	CHECK(err == SQLITE_OK) << "Couldn't bind group routine: " << sqlite3_errstr(err);


	// optionally, also bind the id field
	err = this->sqlBind(statement, ":id", group->id, true);
}

#pragma mark - Operators
/**
 * Compares whether two groups are equal; they are equal if they have the same
 * id; thus, this will not work if one of the groups hasn't been inserted into
 * the database yet.
 */
bool operator==(const DataStore::Group& lhs, const DataStore::Group& rhs) {
	return (lhs.id == rhs.id);
}

bool operator!=(const DataStore::Group& lhs, const DataStore::Group& rhs) {
	return !(lhs == rhs);
}

bool operator< (const DataStore::Group& lhs, const DataStore::Group& rhs) {
	return (lhs.id < rhs.id);
}
bool operator> (const DataStore::Group& lhs, const DataStore::Group& rhs) {
	return rhs < lhs;
}
bool operator<=(const DataStore::Group& lhs, const DataStore::Group& rhs) {
	return !(lhs > rhs);
}
bool operator>=(const DataStore::Group& lhs, const DataStore::Group& rhs) {
	return !(lhs < rhs);
}
