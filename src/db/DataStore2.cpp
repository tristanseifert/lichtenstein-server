//
// Created by Tristan Seifert on 2019-09-08.
//
#include "DataStore2.h"

#include "../config/Config.h"
#include "../config/Defaults.h"

#include "../version.h"

#include <sstream>

#include <glog/logging.h>

#include <sqlite_orm/sqlite_orm.h>

using config::ConfigReader;

// sqlite stuff
using namespace sqlite_orm;

// register defaults
static bool defaultsRegistered = // NOLINT(cert-err58-cpp)
        config::Defaults::registerString("db.path", "", "Path to data store") &&
        config::Defaults::registerString("db.journal", "WAL",
                                         "SQLite journalling mode to use") &&
        config::Defaults::registerLong("db.checkpointInterval", 0,
                                       "How often to checkpoint DB (0 to disable)");

namespace db {
  /**
   * Allocates the database's backing store.
   */
  DataStore2::DataStore2() : db(initStorage(Config::getString("db.path"))) {
    // set the journalling mode (i wish there were a cleaner way)
    auto journalMode = Config::getString("db.journal");

    if(journalMode == "WAL") {
      this->db.pragma.journal_mode(sqlite_orm::journal_mode::WAL);
    } else if(journalMode == "MEMORY") {
      this->db.pragma.journal_mode(sqlite_orm::journal_mode::MEMORY);
    } else if(journalMode == "DELETE") {
      this->db.pragma.journal_mode(sqlite_orm::journal_mode::DELETE);
    } else if(journalMode == "TRUNCATE") {
      this->db.pragma.journal_mode(sqlite_orm::journal_mode::TRUNCATE);
    } else if(journalMode == "PERSIST") {
      this->db.pragma.journal_mode(sqlite_orm::journal_mode::PERSIST);
    } else {
      std::stringstream error;
      error << "Invalid database journalling mode '" << journalMode << "'";

      throw std::runtime_error(error.str().c_str());
    }


    // update schema
    this->db.sync_schema(true);

    // update the server versions
    auto build = this->getMeta("server_build");
    auto version = this->getMeta("server_version");

    if(build && version) {
      LOG(INFO) << "Database last opened with version " << version.value()
                << " (build " << build.value() << ")";
    }

    this->setMeta("server_build", std::string(gVERSION_HASH) + "/" +
                                  std::string(gVERSION_BRANCH));
    this->setMeta("server_version", std::string(gVERSION));
  }

  /**
   * Closes the datablaze
   */
  DataStore2::~DataStore2() {

  }


  /**
   * Sets the given key in the metadata column.
   *
   * @param key Key to create or update
   * @param value Value to set the key to
   */
  void DataStore2::setMeta(const std::string key, std::string value) {
    using types::Info;

    this->db.transaction([&]() mutable {
      // do we have it in the db?
      auto metas = this->db.get_all<Info>(where(c(&Info::getKey) == key));

      if(!metas.empty()) {
        auto existing = metas.at(0);

        // update the value
        VLOG(2) << "Setting existing meta key '" << key << "' : '" << value
                << "'";

        existing.setValue(value);
        this->db.update(existing);

        // success!
        return true;
      } else {
        // we must create a new one
        VLOG(2) << "Creating new meta key '" << key << "' : '" << value << "'";

        // create a new meta key and store it
        auto newKey = Info(key, value);
        this->db.insert(newKey);

        // success
        return true;
      }

      // if we get here, something went wrong
      return false;
    });
  }

  /**
   * Attempts to find the value for the given key.
   *
   * @param key Key to read
   * @return Value, if available
   */
  std::optional<std::string> DataStore2::getMeta(const std::string key) {
    using types::Info;

    auto metas = this->db.get_all<Info>(where(c(&Info::getKey) == key));

    if(!metas.empty()) {
      // return the value of the first one
      return metas.at(0).getValue();
    }
      // could not find any matching metas :(
    else {
      return std::nullopt;
    }
  }


}