#include "DataStore.h"

#include <glog/logging.h>
#include <sqlite3.h>

#include <vector>
#include <iostream>

using namespace std;


#pragma mark - Public Query Interface
/**
 * Returns all nodes in the datastore in a vector.
 */
vector<DataStore::Node *> DataStore::getAllNodes() {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DataStore::Node *> nodes;

	// execute the query
	err = this->sqlPrepare("SELECT * FROM nodes;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// execute the query
	while((result = this->sqlStep(statement)) == SQLITE_ROW) {
		// create the node, populate it, and add it to the vector
		DataStore::Node *node = new DataStore::Node(statement, this);

		nodes.push_back(node);
	}

	// free our statement
	this->sqlFinalize(statement);

	return nodes;
}

/**
 * Finds a node with the given MAC address. Returns a pointer to a Node object
 * if it exists, or nullptr if not.
 *
 * @note The caller is responsible for deleting the returned object when it is
 * no longer needed.
 */
DataStore::Node *DataStore::findNodeWithMac(uint8_t macIn[6]) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// check whether the node exists
	if(DataStore::Node::_macExists(macIn, this) == false) {
		return nullptr;
	}

	// allocate the object for later
	DataStore::Node *node = nullptr;

	// it exists, so we must now get it from the db
	err = this->sqlPrepare("SELECT * FROM nodes WHERE mac = :mac;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the mac address
	err = this->sqlBind(statement, ":mac", macIn, 6);
	CHECK(err == SQLITE_OK) << "Couldn't bind MAC: " << sqlite3_errstr(err);

	// execute the query
	result = this->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// populate the node object
		node = new DataStore::Node(statement, this);
	}

	// free our statement
	this->sqlFinalize(statement);

	// return the populated node object
	return node;
}

/**
 * Updates a node in the database based off the data in the passed object. If
 * the node doesn't exist, it's created.
 */
void DataStore::update(DataStore::Node *node) {
	// does the node exist?
	// if(this->_nodeWithMacExists(node->macAddr)) {
	if(node->id != 0) {
		// it does, so we can just update it
		node->_update(this);
	} else {
		// it doesn't, so we need to create it
		node->_create(this);
	}
}

#pragma mark - Private Query Interface
/**
 * Creates a node in the database. This doesn't check whether one with the same
 * MAC already exists -- if it does, the query will fail.
 */
void DataStore::Node::_create(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Creating new node with MAC " << this->macToString();

	// prepare an update query
	err = db->sqlPrepare("INSERT INTO nodes (ip, mac, hostname, adopted, hwversion, swversion, lastSeen) VALUES (:ip, :mac, :hostname, :adopted, :hwversion, :swversion, :lastseen);", &statement);
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
	CHECK(result != 0) << "rowid for inserted node is zero… this shouldn't happen.";

	this->id = result;
}

/**
 * Updates an existing node in the database. Nodes are searched for based on
 * their MAC address.
 */
void DataStore::Node::_update(DataStore *db) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	VLOG(1) << "Updating existing node with MAC " << this->macToString();

	// prepare an update query
	err = db->sqlPrepare("UPDATE nodes SET ip = :ip, mac = :mac, hostname = :hostname, adopted = :adopted, hwversion = :hwversion, swversion = :swversion, lastSeen = :lastseen WHERE id = :id;", &statement);
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
 * Checks if a node with the specified MAC address exists.
 */
bool DataStore::Node::_macExists(uint8_t macIn[6], DataStore *db) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// prepare a count statement
	err = db->sqlPrepare("SELECT count(*) FROM nodes WHERE mac = :mac;", &statement);
	CHECK(err == SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the mac address
	err = db->sqlBind(statement, ":mac", macIn, 6);
	CHECK(err == SQLITE_OK) << "Couldn't bind MAC: " << sqlite3_errstr(err);

	// execute the query
	result = db->sqlStep(statement);

	if(result == SQLITE_ROW) {
		// retrieve the value of the first column (0-based)
		count = sqlite3_column_int(statement, 0);
	}

	// free our statement
	db->sqlFinalize(statement);

	// if count is nonzero, the node exists
	CHECK(count < 2) << "Duplicate node records for MAC " << Node::macToString(macIn);

	return (count != 0);
}

/**
 * Copies the fields from a statement (which is currently returning a row) into
 * an existing node object.
 */
void DataStore::Node::_fromRow(sqlite3_stmt *statement, DataStore *db) {
	int numColumns = sqlite3_column_count(statement);

	// iterate over all returned columns
	for(int i = 0; i < numColumns; i++) {
		// get the column name and see to which property it matches up
		const char *colName = sqlite3_column_name(statement, i);

		// is it the id column?
		if(strcmp(colName, "id") == 0) {
			this->id = sqlite3_column_int(statement, i);
		}
		// is it the ip column?
		else if(strcmp(colName, "ip") == 0) {
			this->ip = sqlite3_column_int(statement, i);
		}
		// is it the MAC address column?
		else if(strcmp(colName, "mac") == 0) {
			const void *rawMac = sqlite3_column_blob(statement, i);
			int length = sqlite3_column_bytes(statement, i);

			// copy only the first six bytes… that's all we have space for
			if(length > 6) {
				length = 6;
			}

			memcpy(this->macAddr, rawMac, length);
		}
		// is it the hostname column?
		else if(strcmp(colName, "hostname") == 0) {
			this->hostname = db->_stringFromColumn(statement, i);
		}
		// is it the adopted column?
		else if(strcmp(colName, "adopted") == 0) {
			this->adopted = (sqlite3_column_int(statement, i) != 0);
		}
		// is it the hardware version column?
		else if(strcmp(colName, "hwversion") == 0) {
			this->hwVersion = sqlite3_column_int(statement, i);
		}
		// is it the software version column?
		else if(strcmp(colName, "swversion") == 0) {
			this->swVersion = sqlite3_column_int(statement, i);
		}
		// is it the last seen timestamp column?
		else if(strcmp(colName, "lastSeen") == 0) {
			this->lastSeen = sqlite3_column_int(statement, i);
		}
	}
}

/**
 * Binds the fields of a node object to a prepared query. The prepared query
 * is expected to have named parameters corresponding to all of the fields
 * in the node object; the "id" field is the only field that may be missing.
 */
void DataStore::Node::_bindToStatement(sqlite3_stmt *statement, DataStore *db) {
	int err, idx;

	// bind the ip address
	err = db->sqlBind(statement, ":ip", this->ip);
	CHECK(err == SQLITE_OK) << "Couldn't bind node IP address: " << sqlite3_errstr(err);

	// bind the MAC address
	err = db->sqlBind(statement, ":mac", this->macAddr, 6);
	CHECK(err == SQLITE_OK) << "Couldn't bind node MAC: " << sqlite3_errstr(err);

	// bind the hostname
	err = db->sqlBind(statement, ":hostname", this->hostname);
	CHECK(err == SQLITE_OK) << "Couldn't bind node hostname: " << sqlite3_errstr(err);

	// bind the adopted status
	err = db->sqlBind(statement, ":adopted", (this->adopted ? 1 : 0));
	CHECK(err == SQLITE_OK) << "Couldn't bind node adopted status: " << sqlite3_errstr(err);

	// bind the HW version
	err = db->sqlBind(statement, ":hwversion", this->hwVersion);
	CHECK(err == SQLITE_OK) << "Couldn't bind node hardware version: " << sqlite3_errstr(err);

	// bind the SW version
	err = db->sqlBind(statement, ":swversion", this->swVersion);
	CHECK(err == SQLITE_OK) << "Couldn't bind node software version: " << sqlite3_errstr(err);

	// bind the last seen timestamp
	err = db->sqlBind(statement, ":lastseen", this->lastSeen);
	CHECK(err == SQLITE_OK) << "Couldn't bind node last seen timestamp: " << sqlite3_errstr(err);

	// optionally, also bind the id field
	err = db->sqlBind(statement, ":id", this->id, true);
	CHECK(err == SQLITE_OK) << "Couldn't bind node id: " << sqlite3_errstr(err);
}

#pragma mark - Helpers
/**
 * Converts a MAC address to a string.
 */
const string DataStore::Node::macToString(const uint8_t macIn[6]) {
	static const int macBufSz = 32;

	char mac[macBufSz];
	snprintf(mac, macBufSz, "%02X-%02X-%02X-%02X-%02X-%02X", macIn[0], macIn[1],
			 macIn[2], macIn[3], macIn[4], macIn[5]);

	return string(mac);
}

#pragma mark - Operators
/**
 * Compares whether two nodes are equal; they are equal if they have the same
 * id; thus, this will not work if one of the nodes hasn't been inserted into
 * the database yet.
 */
bool operator==(const DataStore::Node& lhs, const DataStore::Node& rhs) {
	return (lhs.id == rhs.id);
}

bool operator!=(const DataStore::Node& lhs, const DataStore::Node& rhs) {
	return !(lhs == rhs);
}

bool operator< (const DataStore::Node& lhs, const DataStore::Node& rhs) {
	return (lhs.id < rhs.id);
}
bool operator> (const DataStore::Node& lhs, const DataStore::Node& rhs) {
	return rhs < lhs;
}
bool operator<=(const DataStore::Node& lhs, const DataStore::Node& rhs) {
	return !(lhs > rhs);
}
bool operator>=(const DataStore::Node& lhs, const DataStore::Node& rhs) {
	return !(lhs < rhs);
}

/**
 * Outputs the some info about the node to the output stream.
 */
ostream &operator<<(ostream& strm, const DataStore::Node& obj) {
	strm << "node id " << obj.id << "{hostname = " << obj.hostname << ", mac = "
	 	 << obj.macToString() << "}";

	return strm;
}
