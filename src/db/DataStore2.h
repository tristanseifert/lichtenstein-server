//
// Created by Tristan Seifert on 2019-09-08.
//

#ifndef LICHTENSTEIN_SERVER_DATASTORE2_H
#define LICHTENSTEIN_SERVER_DATASTORE2_H

#include <optional>

// include the db types
#include "db/types/Info.h"

// also, include the sqlite orm stuff
#include <sqlite_orm/sqlite_orm.h>

namespace db {
  struct Employee {
    int id;
    std::string name;
    int age;
    std::string address;
    double salary;
  };

  inline auto initStorage(const std::string &path) {
    using namespace sqlite_orm;
    return make_storage(path,
                        make_table("ServerMeta",
                                   make_column("id",
                                               &types::Info::getId,
                                               &types::Info::setId,
                                               autoincrement(), primary_key()),
                                   make_column("key",
                                               &types::Info::getKey,
                                               &types::Info::setKey,
                                               unique()
                                   ),

                                   make_column("value",
                                               &types::Info::getValue,
                                               &types::Info::setValue
                                   )
                        )
    );
  }

  /**
   * This is the new version of the data store, which stores all nodes, effects,
   * and groups that the server knows about.
   */
  class DataStore2 {
    private:
      using _StorageType = decltype(initStorage(""));

    public:
      DataStore2();

      ~DataStore2();

    public:
      void setMeta(std::string key, std::string value);

      std::optional<std::string> getMeta(std::string key);

    private:
      // reference to the DB
      _StorageType db;
  };
}


#endif //LICHTENSTEIN_SERVER_DATASTORE2_H
