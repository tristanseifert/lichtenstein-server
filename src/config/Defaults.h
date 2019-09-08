//
// Created by Tristan Seifert on 2019-09-08.
//

#ifndef LICHTENSTEIN_SERVER_CONFIG_DEFAULTS_H
#define LICHTENSTEIN_SERVER_CONFIG_DEFAULTS_H

#include <map>
#include <tuple>
#include <string>
#include <optional>

namespace config {
  class Defaults {
    private:
      using ValueType = std::tuple<std::string, long, double, bool>;

    public:
      Defaults() = delete;

    public:
      static std::optional<std::string> getString(const std::string &key);
      static std::optional<long> getLong(const std::string &key);
      static std::optional<double> getDouble(const std::string &key);
      static std::optional<bool> getBool(const std::string &key);

    public:
      static bool registerString(const std::string &key, const std::string &value);
      static bool registerLong(const std::string &key, const long value);
      static bool registerDouble(const std::string &key, const double value);
      static bool registerBool(const std::string &key, const bool value);

    public:
      static void printData();

    private:
      static void allocData();

    private:
      static std::map<std::string, ValueType> *data;
  };
}

#endif //LICHTENSTEIN_SERVER_CONFIG_DEFAULTS_H
