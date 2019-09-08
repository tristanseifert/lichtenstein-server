//
// Created by Tristan Seifert on 2019-09-08.
//

#ifndef LICHTENSTEIN_SERVER_CONFIGREADER_H
#define LICHTENSTEIN_SERVER_CONFIGREADER_H

#include <string>
#include <optional>
#include <utility>
#include <memory>
#include <stdexcept>

class INIReader;

namespace config {
  /**
   * Provides an interface for reading the app's configuration. This config is
   * split into sections of key/value data, with default values defined in the
   * defaults file.
   */
  class ConfigReader {
    public:
      // an invalid key
      class KeyError : public std::runtime_error {
        public:
          KeyError(const char *what) : std::runtime_error(what) {}
      };

    public:
      explicit ConfigReader(const std::string &path);

    public:
      std::string getString(const std::string &key);
      long getNumber(const std::string &key);
      double getDouble(const std::string &key);
      bool getBoolean(const std::string &key);

    private:
      std::pair<std::string, std::string> splitKey(const std::string &key);

    private:
      // handler to read INI config file
      std::shared_ptr<INIReader> ini;
  };
}


#endif //LICHTENSTEIN_SERVER_CONFIGREADER_H
