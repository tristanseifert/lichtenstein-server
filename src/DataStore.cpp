#include "DataStore.h"

#include <glog/logging.h>

using namespace std;

// include the v1 schema
const char *schema_v1 =
#include "sql/schema_v1.sql"
;

// lastest schema
const char *schema_latest = schema_v1;

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
}

/**
 * Updates the server version stored in the db.
 */
void DataStore::updateStoredServerVersion() {
	this->setInfoValue("server_build", string(GIT_HASH) + "/" + string(GIT_BRANCH));
	this->setInfoValue("server_version", string(VERSION));
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
 */
void DataStore::close() {
	int status = 0;

	LOG(INFO) << "Closing sqlite database";

	// optimize the database
	this->optimize();

	// attempt to close the db
	status = sqlite3_close(this->db);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't close database, data loss may result!";
	LOG_IF(INFO, status == SQLITE_OK) << "Database has been closed, no further access is possible";
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
