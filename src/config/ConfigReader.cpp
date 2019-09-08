//
// Created by Tristan Seifert on 2019-09-08.
//
#include "ConfigReader.h"
#include "Defaults.h"

#include <inih/INIReader.h>
#include <glog/logging.h>

#include <sstream>


namespace config {
  /**
   * Opens the INI file for reading.
   *
   * @param path Path to INI file
   */
  ConfigReader::ConfigReader(const std::string &path) {
    int err;
    std::stringstream errStr;

    // load the INI
    this->ini = std::make_shared<INIReader>(path);
    err = this->ini->ParseError();

    if(err == -1) {
      errStr << "Failed to open config file at '" << path << "'";
      throw std::runtime_error(errStr.str().c_str());
    } else if(err > 0) {
      errStr << "Parse error on line " << err << " of config file file '" << path << "'";
      throw std::runtime_error(errStr.str().c_str());
    }
  }

  /**
   * Reads a string from the config.
   *
   * @param inKey Key to read
   * @return Key value, if available
   */
  std::string ConfigReader::getString(const std::string &inKey) {
    // split the key and get its default value
    auto key = this->splitKey(inKey);

    auto defaultVal = Defaults::getString(inKey);

    if(!defaultVal.has_value()) {
      std::stringstream error;

      error << "Missing string default value for '" << inKey << "'";
      throw KeyError(error.str().c_str());
    }

    // read from config
    return this->ini->Get(key.first, key.second, defaultVal.value());
  }

  /**
   * Reads a number from the config.
   *
   * @param inKey Key to read
   * @return Key value, if available
   */
  long ConfigReader::getNumber(const std::string &inKey) {
    // split the key and get its default value
    auto key = this->splitKey(inKey);

    auto defaultVal = Defaults::getLong(inKey);

    if(!defaultVal.has_value()) {
      std::stringstream error;

      error << "Missing long default value for '" << inKey << "'";
      throw KeyError(error.str().c_str());
    }

    // read from config
    return this->ini->GetInteger(key.first, key.second, defaultVal.value());
  }

  /**
   * Reads a double from the config.
   *
   * @param inKey Key to read
   * @return Key value, if available
   */
  double ConfigReader::getDouble(const std::string &inKey) {
    // split the key and get its default value
    auto key = this->splitKey(inKey);

    auto defaultVal = Defaults::getDouble(inKey);

    if(!defaultVal.has_value()) {
      std::stringstream error;

      error << "Missing double default value for '" << inKey << "'";
      throw KeyError(error.str().c_str());
    }

    // read from config
    return this->ini->GetReal(key.first, key.second, defaultVal.value());
  }

  /**
   * Reads a boolean from the config.
   *
   * @param inKey Key to read
   * @return Key value, if available
   */
  bool ConfigReader::getBoolean(const std::string &inKey) {
    // split the key and get its default value
    auto key = this->splitKey(inKey);

    auto defaultVal = Defaults::getBool(inKey);

    if(!defaultVal.has_value()) {
      std::stringstream error;

      error << "Missing bool default value for '" << inKey << "'";
      throw KeyError(error.str().c_str());
    }

    // read from config
    return this->ini->GetBoolean(key.first, key.second, defaultVal.value());
  }



  /**
   * Splits a key (in the section.key) format into a separate section and key
   * string.
   *
   * @param key Combined section/key
   * @return Separated section/key
   */
  std::pair<std::string, std::string> ConfigReader::splitKey(const std::string &inKey) {
    auto dot = inKey.find(".");

    auto section = inKey.substr(0, dot);
    auto key = inKey.substr((dot + 1));

    return {section, key};
  }
}
