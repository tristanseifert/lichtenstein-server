#include "DataStore.h"

#include <glog/logging.h>

using namespace std;

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
 */
void DataStore::commit() {

}

/**
 * Opens the sqlite database for use.
 */
void DataStore::open() {
	int status = 0;

	// get the path
	const char *path = this->path.c_str();
	LOG(INFO) << "Opening sqlite3 database at " << path;

	// attempt to open the db
	status = sqlite3_open_v2(path, &this->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, nullptr);

	LOG_IF(FATAL, status != SQLITE_OK) << "Couldn't open database: " << sqlite3_errmsg(this->db);
}

/**
 * Closes the sqlite database.
 */
void DataStore::close() {
	int status = 0;

	// attempt to close the db
	status = sqlite3_close(this->db);
	LOG_IF(ERROR, status != SQLITE_OK) << "Couldn't close database, data loss may result!";
}
