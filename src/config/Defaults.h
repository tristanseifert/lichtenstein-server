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
      using ValueType = std::tuple<std::string, long, double, bool, std::optional<std::string>>;

    public:
      Defaults() = delete;

    public:
      static std::optional<std::string> getString(const std::string &key);
      static std::optional<long> getLong(const std::string &key);
      static std::optional<double> getDouble(const std::string &key);
      static std::optional<bool> getBool(const std::string &key);

    public:
      static bool registerString(const std::string &key, const std::string &value, const std::optional<std::string> description = std::nullopt);
      static bool registerLong(const std::string &key, const long value, const std::optional<std::string> description = std::nullopt);
      static bool registerDouble(const std::string &key, const double value, const std::optional<std::string> description = std::nullopt);
      static bool registerBool(const std::string &key, const bool value, const std::optional<std::string> description = std::nullopt);

    public:
      static std::string printData();
      static std::string printDescriptions();

    private:
      static void allocData();

    private:
      static std::map<std::string, ValueType> *data;
  };
}

#endif //LICHTENSTEIN_SERVER_CONFIG_DEFAULTS_H
