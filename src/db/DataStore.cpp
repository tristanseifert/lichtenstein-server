#include "DataStore.h"

#include <memory>

#include "../Logging.h"
#include "../ConfigManager.h"

using namespace Lichtenstein::Server::DB;

// shared data store instance
std::shared_ptr<DataStore> DataStore::sharedInstance;


/**
 * Opens the data store. The path of the store is read from the config.
 */
void DataStore::open() {
    // get the path to the data store
    std::string path = ConfigManager::get("db.path", "");
    Logging::info("Reading data store from '{}'", path);

    sharedInstance = std::make_shared<DataStore>(path);
}

/**
 * Closes the datastore. No further access will be possible beyond this point.
 */
void DataStore::close() {
    sharedInstance = nullptr;
}

/**
 * Tries to open the data store at the given path. If there is no such file, it
 * will be created.
 */
DataStore::DataStore(const std::string &path) {
    // create the storage and ensure schema is consistent
    this->storage = std::make_unique<Storage>(initStorage(path));
    this->storage->sync_schema(true);
}

/**
 * Closes the data store for good.
 */
DataStore::~DataStore() {
    // get rid of the storage
    this->storage = nullptr;
}
