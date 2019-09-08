//
// Created by Tristan Seifert on 2019-09-08.
//

#ifndef LICHTENSTEIN_SERVER_CONFIG_H
#define LICHTENSTEIN_SERVER_CONFIG_H

#include <string>

namespace config {
  class ConfigReader;
}

/**
 * Provides the global config class. Clients should include this file and use
 * the static methods to get values.
 */
class Config {
  public:
    Config() = delete;

  public:
    static std::string getString(const std::string &key);
    static long getNumber(const std::string &key);
    static double getDouble(const std::string &key);
    static bool getBool(const std::string &key);

  public:
    static void load(const std::string &path);

  private:
    static config::ConfigReader *reader;
};


#endif //LICHTENSTEIN_SERVER_CONFIG_H
