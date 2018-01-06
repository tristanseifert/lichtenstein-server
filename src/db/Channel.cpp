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
 * Returns all channels in the datastore in a vector.
 */
vector<DbChannel *> DataStore::getAllChannels() {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DbChannel *> groups;

	// execute the query
	err = this->sqlPrepare("SELECT * FROM channels;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// execute the query
	while((result = this->sqlStep(statement)) == SQLITE_ROW) {
		// create the group, populate it, and add it to the vector
		DbChannel *group = new DbChannel(statement, this);

		groups.push_back(group);
	}

	// free our statement
	this->sqlFinalize(statement);

	return groups;
}

/**
 * Returns all channels associated with the given node in the datastore in a
 * vector.
 */
vector<DbChannel *> DataStore::getChannelsForNode(DbNode *node) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DbChannel *> channels;

	// execute the query
	err = this->sqlPrepare("SELECT * FROM channels WHERE node = :nodeId;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the node id
	err = this->sqlBind(statement, ":nodeId", node->id);
	CHECK(err == SQLITE_OK) << "Couldn't bind channel node id: " << sqlite3_errstr(err);

	// execute the query
	while((result = this->sqlStep(statement)) == SQLITE_ROW) {
		// create the channel, populate it, and add it to the vector
		DbChannel *channel = new DbChannel(statement, this, node);

		channels.push_back(channel);
	}

	// free our statement
	this->sqlFinalize(statement);

	return channels;
}

/**
 * Finds a channel with the given id. If no such channel exists, nullptr is
 * returned.
 */
DbChannel *DataStore::findChannelWithId(int id) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// if id is zero or negative, return
	if(id <= 0) {
		return nullptr;
	}

	// check whether the channel exists
/*	if(DbChannel::_idExists(id, this) == false) {
		return nullptr;
	}
*/

	// allocate the object for later
	DbChannel *group = nullptr;

	// it exists, so we must now get it from the db
	err = this->sqlPrepare("SELECT * FROM channels WHERE id = :id;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the id
	err = this->sqlBind(statement, ":id", id);
	CHECK(err == SQLITE_OK) << "Couldn't bind group id: " << sqlite3_errstr(err);

	// execute the query
	result = this->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// populate the group object
		group = new DbChannel(statement, this);
	}

	// free our statement
	this->sqlFinalize(statement);

	// return the populated group object
	return group;
}

/**
 * Updates the specified channel. If a channel with this id already exists (as
 * would be expected if it was previously fetched from the database) the
 * existing channel is updated. Otherwise, a new channel is created.
 */
void DataStore::update(DbChannel *channel) {
	// does the group exist?
	if(channel->id != 0) {
		// it does, so we can just update it
		channel->_update(this);
	} else {
		// it doesn't, so we need to create it
		channel->_create(this);
	}
}

#pragma mark - Private Query Interface
/**
 * Creates a new channel in the database, then assigns the id value of the
 * channel that was passed in.
 */
void DbChannel::_create(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Creating new channel for node " << this->node;

	// prepare an update query
	err = db->sqlPrepare("INSERT INTO channels (node, nodeOffset, numPixels, fbOffset) VALUES (:node, :nodeOffset, :numPixels, :fbOffset);", &statement);
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
	CHECK(result != 0) << "rowid for inserted channel is zero… this shouldn't happen.";

	this->id = result;
}

/**
 * Updates an existing channel in the database. This replaces all fields except
 * for id.
 */
void DbChannel::_update(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Updating existing channel with id " << this->id;

	// prepare an update query
	err = db->sqlPrepare("UPDATE channels SET node = :node, nodeOffset = :nodeOffset, numPixels = :numPixels, fbOffset = :fbOffset WHERE id = :id;", &statement);
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
 * Determines whether a channel with the given id exists.
 */
bool DbChannel::_idExists(int id, DataStore *db) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// prepare a count statement
	err = db->sqlPrepare("SELECT count(*) FROM channels WHERE id = :id;", &statement);
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
	CHECK(count < 2) << "Found " << count << " channels with the same id, this should never happen";
	return (count != 0);
}

/**
 * Copies the fields from a statement (which is currently returning a row) into
 * an existing channel object.
 */
void DbChannel::_fromRow(sqlite3_stmt *statement, DataStore *db) {
	int numColumns = db->sqlGetNumColumns(statement);

	// iterate over all returned columns
	for(int i = 0; i < numColumns; i++) {
		// get the column name and see to which property it matches up
		string colName = db->sqlColumnName(statement, i);

		// is it the id column?
		if(colName == "id") {
			this->id = db->sqlGetColumnInt(statement, i);
		}
		// is it the node channel index column?
		else if(colName == "nodeOffset") {
			this->nodeOffset = db->sqlGetColumnInt(statement, i);
		}
		// is it the pixel length column?
		else if(colName == "numPixels") {
			this->numPixels = db->sqlGetColumnInt(statement, i);
		}
		// is it the framebuffer starting offset column?
		else if(colName == "fbOffset") {
			this->fbOffset = db->sqlGetColumnInt(statement, i);
		}
		// is it the node column?
		else if(colName == "node") {
			this->nodeId = db->sqlGetColumnInt(statement, i);

			// fetch the appropriate routine from the database
			if(this->node == nullptr || this->node->id != this->nodeId) {
				VLOG(1) << "node is NULL or ID doesn't match, fetching again";

				this->node = db->findNodeWithId(this->nodeId);
			}
		}
	}
}

/**
 * Binds the fields of a channel object to a prepared query. The prepared query
 * is expected to have named parameters corresponding to all of the fields
 * in the channel object; the "id" field is the only field that may be missing.
 */
void DbChannel::_bindToStatement(sqlite3_stmt *statement, DataStore *db) {
	int err, idx;

	// bind the node
	if(this->node) {
		this->nodeId = this->node->id;
	} else {
		this->nodeId = 0;
	}

	err = db->sqlBind(statement, ":node", this->nodeId);
	CHECK(err == SQLITE_OK) << "Couldn't bind channel node: " << sqlite3_errstr(err);

	// bind the channel index
	err = db->sqlBind(statement, ":nodeOffset", this->nodeOffset);
	CHECK(err == SQLITE_OK) << "Couldn't bind channel index: " << sqlite3_errstr(err);

	// bind the channel size
	err = db->sqlBind(statement, ":numPixels", this->numPixels);
	CHECK(err == SQLITE_OK) << "Couldn't bind channel size: " << sqlite3_errstr(err);

	// bind the channel framebuffer offset
	err = db->sqlBind(statement, ":fbOffset", this->fbOffset);
	CHECK(err == SQLITE_OK) << "Couldn't bind channel framebuffer offset: " << sqlite3_errstr(err);

	// optionally, also bind the id field
	err = db->sqlBind(statement, ":id", this->id, true);
	CHECK(err == SQLITE_OK) << "Couldn't bind channel id: " << sqlite3_errstr(err);
}

#pragma mark - Operators
/**
 * Compares whether two channels are equal; they are equal if they have the same
 * id; thus, this will not work if one of the channels hasn't been inserted into
 * the database yet.
 */
bool operator==(const DbChannel& lhs, const DbChannel& rhs) {
	return (lhs.id == rhs.id);
}

bool operator!=(const DbChannel& lhs, const DbChannel& rhs) {
	return !(lhs == rhs);
}

bool operator< (const DbChannel& lhs, const DbChannel& rhs) {
	return (lhs.id < rhs.id);
}
bool operator> (const DbChannel& lhs, const DbChannel& rhs) {
	return rhs < lhs;
}
bool operator<=(const DbChannel& lhs, const DbChannel& rhs) {
	return !(lhs > rhs);
}
bool operator>=(const DbChannel& lhs, const DbChannel& rhs) {
	return !(lhs < rhs);
}

/**
 * Outputs the some info about the channel to the output stream.
 */
ostream &operator<<(ostream& strm, const DbChannel& obj) {
	strm << "channel id " << obj.id << "{index = " << obj.nodeOffset << ", "
		 << "size = " << obj.numPixels << ", node = " << obj.nodeId << "}";

	return strm;
}
