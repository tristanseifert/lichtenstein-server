//
// Created by Tristan Seifert on 2019-09-08.
//
#include "Config.h"
#include "ConfigReader.h"
#include "Defaults.h"

#include <mutex>
#include <stdexcept>

using config::ConfigReader;

ConfigReader *Config::reader = nullptr;

/**
 * Attempts to laod the config from the given path.
 *
 * @param path File path to config file
 */
void Config::load(const std::string &path) {
  static std::once_flag flag;

  std::call_once(flag, [path]{
    reader = new ConfigReader(path);
  });
}

/**
 * Reads a string from the config.
 *
 * @param key Key to read
 * @return Value in config
 */
std::string Config::getString(const std::string &key) {
  if(!reader) {
    throw std::runtime_error("Attempt to read config before config was loaded");
  }

  return reader->getString(key);
}

/**
 * Reads a number from the config.
 *
 * @param key Key to read
 * @return Value in config
 */
long Config::getNumber(const std::string &key) {
  if(!reader) {
    throw std::runtime_error("Attempt to read config before config was loaded");
  }

  return reader->getNumber(key);
}

/**
 * Reads a double from the config.
 *
 * @param key Key to read
 * @return Value in config
 */
double Config::getDouble(const std::string &key) {
  if(!reader) {
    throw std::runtime_error("Attempt to read config before config was loaded");
  }

  return reader->getDouble(key);
}

/**
 * Reads a double from the config.
 *
 * @param key Key to read
 * @return Value in config
 */
bool Config::getBool(const std::string &key) {
  if(!reader) {
    throw std::runtime_error("Attempt to read config before config was loaded");
  }

  return reader->getBoolean(key);
}
