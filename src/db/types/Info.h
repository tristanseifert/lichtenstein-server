//
// Created by Tristan Seifert on 2019-09-26.
//

#ifndef LICHTENSTEIN_SERVER_DB_TYPES_INFO_H
#define LICHTENSTEIN_SERVER_DB_TYPES_INFO_H

#include <string>
#include <utility>

namespace db::types {
  /**
   * Represents a key/value piece of metadata stored in the data store.
   */
  class Info {
    public:
      Info() {}

      Info(std::string key, std::string value) : key(std::move(key)),
                                                 value(std::move(value)) {}

    public:
      int getId() const {
        return this->id;
      }

      std::string getKey() const {
        return this->key;
      }

      std::string getValue() const {
        return this->value;
      }

      void setId(int newId) {
        this->id = newId;
      }

      void setKey(std::string newKey) {
        this->key = std::move(newKey);
      }

      void setValue(std::string newValue) {
        this->value = std::move(newValue);
      }

    private:
      int id;
      std::string key;
      std::string value;
  };
}


#endif //LICHTENSTEIN_SERVER_DB_TYPES_INFO_H
