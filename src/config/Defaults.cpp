//
// Created by Tristan Seifert on 2019-09-08.
//
#include "Defaults.h"

#include <glog/logging.h>

#include <cmath>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace config {
  /**
   * Defines the default configuration values.
   */
  std::map<std::string, Defaults::ValueType> *Defaults::data = nullptr;



  /**
   * Gets a string defaults value.
   *
   * @param key Key to look up
   * @return Defaults value if available
   */
  std::optional<std::string> Defaults::getString(const std::string &key) {
    try {
      return std::get<0>(data->at(key));
    } catch(std::out_of_range &e) {
      return std::nullopt;
    }
  }

  /**
   * Gets a long defaults value.
   *
   * @param key Key to look up
   * @return Defaults value if available
   */
  std::optional<long> Defaults::getLong(const std::string &key) {
    try {
      return std::get<1>(data->at(key));
    } catch(std::out_of_range &e) {
      return std::nullopt;
    }
  }

  /**
   * Gets a double defaults value.
   *
   * @param key Key to look up
   * @return Defaults value if available
   */
  std::optional<double> Defaults::getDouble(const std::string &key) {
    try {
      return std::get<2>(data->at(key));
    } catch(std::out_of_range &e) {
      return std::nullopt;
    }
  }

  /**
   * Gets a bool defaults value.
   *
   * @param key Key to look up
   * @return Defaults value if available
   */
  std::optional<bool> Defaults::getBool(const std::string &key) {
    try {
      return std::get<3>(data->at(key));
    } catch(std::out_of_range &e) {
      return std::nullopt;
    }
  }



  /**
   * Inserts the default value for the given key as a string.
   *
   * @param key Key to set default for
   * @param value String value to set as default
   * @return Whether the default was registered
   */
  bool
  Defaults::registerString(const std::string &key, const std::string &value, const std::optional<std::string> description) {
    Defaults::allocData();
//    LOG(INFO) << "Registered string '" << value << "' for key '" << key << "'";

    ValueType tuple = {value, 0, NAN, false, description};
    data->insert_or_assign(key, tuple);
    return true;
  }

  /**
   * Inserts the default value for the given key as a long.
   *
   * @param key Key to set default for
   * @param value String value to set as default
   * @return Whether the default was registered
   */
  bool Defaults::registerLong(const std::string &key, const long value, const std::optional<std::string> description) {
    Defaults::allocData();
//    LOG(INFO) << "Registered long " << value << " for key '" << key << "'";

    ValueType tuple = {"", value, NAN, false, description};
    data->insert_or_assign(key, tuple);
    return true;
  }

  /**
   * Inserts the default value for the given key as a double.
   *
   * @param key Key to set default for
   * @param value String value to set as default
   * @return Whether the default was registered
   */
  bool Defaults::registerDouble(const std::string &key, const double value, const std::optional<std::string> description) {
    Defaults::allocData();
//    LOG(INFO) << "Registered double " << value << " for key '" << key << "'";

    ValueType tuple = {"", 0, value, false, description};
    data->insert_or_assign(key, tuple);
    return true;
  }

  /**
   * Inserts the default value for the given key as a bool.
   *
   * @param key Key to set default for
   * @param value String value to set as default
   * @return Whether the default was registered
   */
  bool Defaults::registerBool(const std::string &key, const bool value, const std::optional<std::string> description) {
    Defaults::allocData();
//    LOG(INFO) << "Registered bool " << value << " for key '" << key << "'";

    ValueType tuple = {"", 0, NAN, value, description};
    data->insert_or_assign(key, tuple);
    return true;
  }



  /**
   * Allocates the data map.
   */
  void Defaults::allocData() {
    static std::once_flag flag;

    std::call_once(flag, []{
      // allocate that shit here
      data = new std::map<std::string, ValueType>();
    });
  }

  /**
   * Prints the defaults data.
   */
  std::string Defaults::printData() {
    std::stringstream str;

    // write header
    str << std::setw(24) << "Key";
    str << std::setw(24) << "String";
    str << std::setw(10) << "Long";
    str << std::setw(10) << "Double";
    str << std::setw(6) << "Bool";
    str << std::endl;

    // iterate over the map
    for(auto &[key, value] : *data) {
      // key
      str << std::setw(24) << key;

      // then each of the values
      str << std::setw(24) << std::get<0>(value);
      str << std::setw(10) << std::get<1>(value);
      str << std::setw(10) << std::get<2>(value);
      str << std::setw(6) << std::boolalpha << std::get<3>(value);
      str << std::endl;
    }

    return str.str();
  }

  /**
   * Returns a string containing
   */
  std::string Defaults::printDescriptions() {
    std::stringstream str;

    // write header
    str << std::right << std::setw(24) << "Key";
    str << "|";
    str << std::left << std::setw(55) << "Description";
    str << std::endl;

    // divider
    str << std::right << std::setfill('-') << std::setw(25) << '+';
    str << std::setfill('-') << std::setw(55) << ' ';
    str << std::endl << std::setfill(' ');

    // iterate over the map
    for(auto &[key, value] : *data) {
      auto desc = std::get<4>(value);

      str << std::right << std::setw(24) << key;

      // divider
      str << "|";

      // print description (if available)
      str << std::left << std::setw(55);

      if(desc.has_value()) {
        str << desc.value();
      } else {
        str << "";
      }

      str << std::endl;
    }

    return str.str();
  }
}