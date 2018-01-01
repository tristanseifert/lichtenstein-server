#include "DataStore.h"

#include "json.hpp"

#include <glog/logging.h>

using namespace std;
using json = nlohmann::json;

// include the v1 schema
const char *schema_v1 =
#include "sql/schema_v1.sql"
;

// lastest schema
const char *schema_latest = schema_v1;
const string latestSchemaVersion = "1";

// default properties to insert into the info table after schema provisioning
const char *schema_info_default =
	"INSERT INTO info (key, value) VALUES (\"server_build\", \"unknown\");"
	"INSERT INTO info (key, value) VALUES (\"server_version\", \"unknown\");"
;

/**
 * Initializes the datastore with the persistent database located at the given
 * path. The database is attempted to be loaded – if it doesn't exist, a new
 * database is created and a schema applied.
 *
 * If there is an error during the loading process, the process will terminate.
 * It is then up to the user to remediate this issue, usually by either deleting
 * the existing database, or by fixing it manually.
 */
DataStore::DataStore(std::string path) {
	this->path = path;

	this->open();
}

/**
 * Cleans up any resources associated with the datastore. This ensures all data
 * is safely commited to disk, compacts the database, and de-allocates any in-
 * memory structures.
 *
 * @note No further access to the database is possible after this point.
 */
DataStore::~DataStore() {
	this->commit();
	this->close();
}

#pragma mark - Database I/O
/**
 * Explicitly requests that the underlying storage engine commits all writes to
 * disk. This call blocks until the writes have been completed by the OS.
 *
 * This performs a "passive" checkpoint; as many frames from the write-ahead log
 * as possible are copied into the database file, WITHOUT blocking on any active
 * database readers or writers.
 */
void DataStore::commit() {
	int status = 0;

	LOG(INFO) << "Performing database checkpoint";

	status = sqlite3_wal_checkpoint_v2(this->db, nullptr, SQLITE_CHECKPOINT_PASSIVE, nullptr, nullptr);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't complete checkpoint: " << sqlite3_errstr(status);
}

/**
 * Opens the sqlite database for use.
 */
void DataStore::open() {
	int status = 0;

	// use the "serialized" threading model
	status = sqlite3_config(SQLITE_CONFIG_SERIALIZED);
	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't set serialized thread model: " << sqlite3_errstr(status);

	// get the path
	const char *path = this->path.c_str();
	LOG(INFO) << "Opening sqlite3 database at " << path;

	// attempt to open the db
	status = sqlite3_open_v2(path, &this->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);

	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't open database: " << sqlite3_errmsg(this->db);

	// apply some post-opening configurations
	this->openConfigDb();

	// check the database schema version and upgrade if needed
	this->checkDbVersion();
}

/**
 * Configures some pragmas on the database before it's used.
 */
void DataStore::openConfigDb() {
	int status = 0;
	char *errStr;

	// set journal mode mode
	status = sqlite3_exec(this->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errStr);
	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't set journal_mode: " << errStr;

	// set incremental autovacuuming mode
	status = sqlite3_exec(this->db, "PRAGMA auto_vacuum=INCREMENTAL;", nullptr, nullptr, &errStr);
	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't set auto_vacuum: " << errStr;

	// set UTF-8 encoding
	status = sqlite3_exec(this->db, "PRAGMA encoding=\"UTF-8\";", nullptr, nullptr, &errStr);
	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't set encoding: " << errStr;

	// store temporary objects in memory
	status = sqlite3_exec(this->db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, &errStr);
	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't set temp_store: " << errStr;

	// truncate the journal rather than deleting it
	status = sqlite3_exec(this->db, "PRAGMA journal_mode=TRUNCATE;", nullptr, nullptr, &errStr);
	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't set journal_mode: " << errStr;
}

/**
 * Optimizes the database: this is typically called before the database is
 * closed. This has the effect of running the following operations:
 *
 * - Vacuums the database to remove any unneeded pages.
 * - Analyzes indices and optimizes them.
 *
 * This function may be called periodically, but it can take a significant
 * amount of time to execute.
 */
void DataStore::optimize() {
	int status = 0;
	char *errStr;

	LOG(INFO) << "Database optimization requested";

	// vacuums the database
	status = sqlite3_exec(this->db, "VACUUM;", nullptr, nullptr, &errStr);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't vacuum: " << errStr;

	// run analyze pragma
	status = sqlite3_exec(this->db, "PRAGMA optimize;", nullptr, nullptr, &errStr);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't run optimize: " << errStr;
}

/**
 * Closes the sqlite database.
 *
 * The database _should_ always close, unless there are any open statements. If
 * you get a "couldn't close database" error, it is most likely a bug in the
 * server code, since statements should always be closed.
 */
void DataStore::close() {
	int status = 0;

	LOG(INFO) << "Closing sqlite database";

	// optimize the database before closing
	this->optimize();

	// attempt to close the db
	status = sqlite3_close(this->db);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't close database, data loss may result!";
	LOG_IF(INFO, status == SQLITE_OK) << "Database has been closed, no further access is possible";
}

#pragma mark - Schema Version Management
/**
 * Checks the version of the database schema, upgrading it if needed. If the
 * database is empty, this applies the most up-to-date version of the schema.
 */
void DataStore::checkDbVersion() {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// first, check if the table exists
	err = sqlite3_prepare_v2(this->db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='info';", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	result = sqlite3_step(statement);

	if(result == SQLITE_ROW) {
		// retrieve the value of the first column (0-based)
		int count = sqlite3_column_int(statement, 0);

		if(count == 0) {
			LOG(INFO) << "Couldn't find info table, assuming db needs provisioning";

			this->provisonBlankDb();
		}
	}

	// free our statement
	sqlite3_finalize(statement);


	// the database has a schema, we just have to find out what version…
	string schemaVersion = this->getInfoValue("schema_version");
	LOG(INFO) << "Schema version: " << schemaVersion;

	// is the schema version the same as that of the latest schema?
	if(schemaVersion != latestSchemaVersion) {
		this->upgradeSchema();
	}

	// log info about what server version last accessed the db
	LOG(INFO) << "Last accessed with version " << this->getInfoValue("server_version") << ", build " << this->getInfoValue("server_build");

	// update the server version that's stored in the DB
	this->updateStoredServerVersion();
}

/**
 * If the schema version is undefined (i.e. the database was just newly created)
 * we apply the schema here and insert some default values.
 */
void DataStore::provisonBlankDb() {
	int status = 0;
	char *errStr;

	// execute the entire schema string in one go. if this fails we're fucked
	status = sqlite3_exec(this->db, schema_latest, nullptr, nullptr, &errStr);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't run schema: " << errStr;

	// set the default values in the info table
	status = sqlite3_exec(this->db, schema_info_default, nullptr, nullptr, &errStr);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't set info default values: " << errStr;
}

/**
 * Performs incremental upgrades from the current schema version to the next
 * highest version until we reach the latest version.
 */
void DataStore::upgradeSchema() {
	string schemaVersion = this->getInfoValue("schema_version");
	LOG(INFO) << "Latest schema version is " << latestSchemaVersion << ", db is"
			  << " currently on version " << schemaVersion << "; upgrade required";
}

#pragma mark - Metadata

/**
 * Updates the server version stored in the db.
 */
void DataStore::updateStoredServerVersion() {
	this->setInfoValue("server_build", string(GIT_HASH) + "/" + string(GIT_BRANCH));
	this->setInfoValue("server_version", string(VERSION));
}

/**
 * Sets a DB metadata key to the specified value. This key must be defined in
 * the initial schema so that the update clause can run.
 */
void DataStore::setInfoValue(string key, string value) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// prepare an update query
	err = sqlite3_prepare_v2(this->db, "UPDATE info SET value = ? WHERE key = ?;", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the value
	err = sqlite3_bind_text(statement, 1, value.c_str(), -1, SQLITE_TRANSIENT);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind value: " << sqlite3_errstr(err);

	// bind the key
	err = sqlite3_bind_text(statement, 2, key.c_str(), -1, SQLITE_TRANSIENT);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind key: " << sqlite3_errstr(err);

	// execute query
	result = sqlite3_step(statement);
	LOG_IF(FATAL, result != SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	sqlite3_finalize(statement);
}

/**
 * Returns the value of the given database metadata key. If the key does not
 * exist, an error is raised.
 */
string DataStore::getInfoValue(string key) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;
	string returnValue;

	// prepare a query and bind key
	err = sqlite3_prepare_v2(this->db, "SELECT value FROM info WHERE key = ?;", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	err = sqlite3_bind_text(statement, 1, key.c_str(), -1, SQLITE_TRANSIENT);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind key: " << sqlite3_errstr(err);

	// execute the query
	result = sqlite3_step(statement);

	if(result == SQLITE_ROW) {
		const unsigned char *value = sqlite3_column_text(statement, 0);
		returnValue = string(reinterpret_cast<const char *>(value));
	}

	// free the statement
	sqlite3_finalize(statement);

	// return a C++ string
	return returnValue;
}

#pragma mark - Nodes
/**
 * Returns all nodes in the datastore in a vector.
 */
vector<DataStore::Node *> DataStore::getAllNodes() {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	vector<DataStore::Node *> nodes;

	// execute the query
	err = sqlite3_prepare_v2(this->db, "SELECT * FROM nodes;", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// execute the query
	while((result = sqlite3_step(statement)) == SQLITE_ROW) {
		// create the node, populate it, and add it to the vector
		DataStore::Node *node = new DataStore::Node();
		this->_nodeFromRow(statement, node);

		nodes.push_back(node);
	}

	// free our statement
	sqlite3_finalize(statement);

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
	if(this->_nodeWithMacExists(macIn) == false) {
		return nullptr;
	}

	// allocate the object for later
	DataStore::Node *node = new DataStore::Node();

	// it exists, so we must now get it from the db
	err = sqlite3_prepare_v2(this->db, "SELECT * FROM nodes WHERE mac = ?1;", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the mac address
	err = sqlite3_bind_blob(statement, 1, macIn, 6, SQLITE_STATIC);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind MAC: " << sqlite3_errstr(err);

	// execute the query
	result = sqlite3_step(statement);

	if(result == SQLITE_ROW) {
		// populate the node object
		this->_nodeFromRow(statement, node);
	}

	// free our statement
	sqlite3_finalize(statement);

	// return the populated node object
	return node;
}

/**
 * Copies the fields from a statement (which is currently returning a row) into
 * an existing node object.
 */
void DataStore::_nodeFromRow(sqlite3_stmt *statement, DataStore::Node *node) {
	int numColumns = sqlite3_column_count(statement);

	// iterate over all returned columns
	for(int i = 0; i < numColumns; i++) {
		// get the column name and see to which property it matches up
		const char *colName = sqlite3_column_name(statement, i);

		// is it the id column?
		if(strcmp(colName, "id") == 0) {
			node->id = sqlite3_column_int(statement, i);
		}
		// is it the ip column?
		else if(strcmp(colName, "ip") == 0) {
			node->ip = sqlite3_column_int(statement, i);
		}
		// is it the MAC address column?
		else if(strcmp(colName, "mac") == 0) {
			const void *rawMac = sqlite3_column_blob(statement, i);
			int length = sqlite3_column_bytes(statement, i);

			// copy only the first six bytes… that's all we have space for
			if(length > 6) {
				length = 6;
			}

			memcpy(node->macAddr, rawMac, length);
		}
		// is it the hostname column?
		else if(strcmp(colName, "hostname") == 0) {
			node->hostname = this->_stringFromColumn(statement, i);
		}
		// is it the adopted column?
		else if(strcmp(colName, "adopted") == 0) {
			node->adopted = (sqlite3_column_int(statement, i) != 0);
		}
		// is it the hardware version column?
		else if(strcmp(colName, "hwversion") == 0) {
			node->hwVersion = sqlite3_column_int(statement, i);
		}
		// is it the software version column?
		else if(strcmp(colName, "swversion") == 0) {
			node->swVersion = sqlite3_column_int(statement, i);
		}
		// is it the last seen timestamp column?
		else if(strcmp(colName, "lastSeen") == 0) {
			node->lastSeen = sqlite3_column_int(statement, i);
		}
	}
}

/**
 * Binds the fields of a node object to a prepared query. The prepared query
 * is expected to have named parameters corresponding to all of the fields
 * in the node object; the "id" field is the only field that may be missing.
 */
void DataStore::_bindNodeToStatement(sqlite3_stmt *statement, DataStore::Node *node) {
	int err, idx;

	// bind the ip address
	idx = sqlite3_bind_parameter_index(statement, ":ip");
	LOG_IF(FATAL, idx == 0) << "Couldn't resolve parameter ip";

	err = sqlite3_bind_int(statement, idx, node->ip);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node IP address: " << sqlite3_errstr(err);

	// bind the MAC address
	idx = sqlite3_bind_parameter_index(statement, ":mac");
	LOG_IF(FATAL, idx == 0) << "Couldn't resolve parameter mac";

	err = sqlite3_bind_blob(statement, idx, node->macAddr, 6, SQLITE_STATIC);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node MAC: " << sqlite3_errstr(err);

	// bind the hostname
	idx = sqlite3_bind_parameter_index(statement, ":hostname");
	LOG_IF(FATAL, idx == 0) << "Couldn't resolve parameter hostname";

	err = sqlite3_bind_text(statement, idx, node->hostname.c_str(), -1, SQLITE_TRANSIENT);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node hostname: " << sqlite3_errstr(err);

	// bind the adopted status
	idx = sqlite3_bind_parameter_index(statement, ":adopted");
	LOG_IF(FATAL, idx == 0) << "Couldn't resolve parameter adopted";

	err = sqlite3_bind_int(statement, idx, node->adopted ? 1 : 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node adopted status: " << sqlite3_errstr(err);

	// bind the HW version
	idx = sqlite3_bind_parameter_index(statement, ":hwversion");
	LOG_IF(FATAL, idx == 0) << "Couldn't resolve parameter hwversion";

	err = sqlite3_bind_int(statement, idx, node->hwVersion);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node hardware version: " << sqlite3_errstr(err);

	// bind the SW version
	idx = sqlite3_bind_parameter_index(statement, ":swversion");
	LOG_IF(FATAL, idx == 0) << "Couldn't resolve parameter swversion";

	err = sqlite3_bind_int(statement, idx, node->swVersion);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node software version: " << sqlite3_errstr(err);

	// bind the last seen timestamp
	idx = sqlite3_bind_parameter_index(statement, ":lastseen");
	LOG_IF(FATAL, idx == 0) << "Couldn't resolve parameter lastseen";

	err = sqlite3_bind_int(statement, idx, node->lastSeen);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node last seen timestamp: " << sqlite3_errstr(err);

	// optionally, also bind the id field
	idx = sqlite3_bind_parameter_index(statement, ":id");

	if(idx != 0) {
		err = sqlite3_bind_int(statement, idx, node->id);
		LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind node id: " << sqlite3_errstr(err);
	}
}

/**
 * Updates a node in the database based off the data in the passed object. If
 * the node doesn't exist, it's created.
 */
void DataStore::updateNode(DataStore::Node *node) {
	// does the node exist?
	if(this->_nodeWithMacExists(node->macAddr)) {
		// it does, so we can just update it
		this->_updateNode(node);
	} else {
		// it doesn't, so we need to create it
		this->_createNode(node);
	}
}

/**
 * Updates an existing node in the database. Nodes are searched for based on
 * their MAC address.
 */
void DataStore::_updateNode(DataStore::Node *node) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	char mac[24];
	snprintf(mac, 24, "%02X-%02X-%02X-%02X-%02X-%02X", node->macAddr[0],
			 node->macAddr[1], node->macAddr[2], node->macAddr[3],
			 node->macAddr[4], node->macAddr[5]);
	LOG(INFO) << "Updating existing node with MAC " << mac;

	// prepare an update query
	// TODO: match on id instead
	err = sqlite3_prepare_v2(this->db, "UPDATE nodes SET ip = :ip, mac = :mac, hostname = :hostname, adopted = :adopted, hwversion = :hwversion, swversion = :swversion, lastSeen = :lastseen WHERE mac = :mac;", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindNodeToStatement(statement, node);

	// then execute it
	result = sqlite3_step(statement);
	LOG_IF(FATAL, result != SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	sqlite3_finalize(statement);
}

/**
 * Creates a node in the database. This doesn't check whether one with the same
 * MAC already exists -- if it does, the query will fail.
 */
void DataStore::_createNode(DataStore::Node *node) {
	int err = 0, result;
	sqlite3_stmt *statement = nullptr;

	// logging
	char mac[24];
	snprintf(mac, 24, "%02X-%02X-%02X-%02X-%02X-%02X", node->macAddr[0],
			 node->macAddr[1], node->macAddr[2], node->macAddr[3],
			 node->macAddr[4], node->macAddr[5]);
	LOG(INFO) << "Creating new node with MAC " << mac;

	// prepare an update query
	err = sqlite3_prepare_v2(this->db, "INSERT INTO nodes (ip, mac, hostname, adopted, hwversion, swversion, lastSeen) VALUES (:ip, :mac, :hostname, :adopted, :hwversion, :swversion, :lastseen);", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the properties
	this->_bindNodeToStatement(statement, node);

	// then execute it
	result = sqlite3_step(statement);
	LOG_IF(FATAL, result != SQLITE_DONE) << "Couldn't execute query: " << sqlite3_errstr(result);

	// free the statement
	sqlite3_finalize(statement);
}

/**
 * Checks if a node with the specified MAC address exists.
 */
bool DataStore::_nodeWithMacExists(uint8_t macIn[6]) {
	int err = 0, result, count;
	sqlite3_stmt *statement = nullptr;

	// prepare a count statement
	err = sqlite3_prepare_v2(this->db, "SELECT count(*) FROM nodes WHERE mac = ?1;", -1, &statement, 0);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't prepare statement: " << sqlite3_errstr(err);

	// bind the mac address
	err = sqlite3_bind_blob(statement, 1, macIn, 6, SQLITE_STATIC);
	LOG_IF(FATAL, err != SQLITE_OK) << "Couldn't bind MAC: " << sqlite3_errstr(err);

	// execute the query
	result = sqlite3_step(statement);

	if(result == SQLITE_ROW) {
		// retrieve the value of the first column (0-based)
		count = sqlite3_column_int(statement, 0);
	}

	// free our statement
	sqlite3_finalize(statement);

	// if count is nonzero, the node exists
	char mac[24];
	snprintf(mac, 24, "%02X-%02X-%02X-%02X-%02X-%02X", macIn[0], macIn[1],
	 		 macIn[2], macIn[3], macIn[4], macIn[5]);

	CHECK(count < 2) << "Duplicate node records for MAC 0x" << std::hex << mac;

	return (count != 0);
}

#pragma mark - Conversion Routines
/**
 * Converts the string value in column `col` to an UTF-8 string, then creates a
 * standard C++ string from it.
 */
string DataStore::_stringFromColumn(sqlite3_stmt *statement, int col) {
	// get the UTF-8 string and its length from sqlite, then allocate a buffer
	const unsigned char *name = sqlite3_column_text(statement, col);
	int nameLen = sqlite3_column_bytes(statement, col);

	char *nameStr = new char[nameLen + 1];
	std::fill(nameStr, nameStr + nameLen + 1, 0);

	// copy the string, but only the number of bytes sqlite returned
	memcpy(nameStr, name, nameLen);

	// create a C++ string and then deallocate the buffer
	string retVal = string(nameStr);
	delete[] nameStr;

	return retVal;
}
