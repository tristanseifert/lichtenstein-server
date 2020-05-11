/**
 * Provides access to the server configuration.
 *
 * Once the config is loaded during startup, any code in the server may call
 * the shared instance and request a config value by its keypath. Values can be
 * retrieved in most primitive types.
 */
#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <string>
#include <mutex>
#include <memory>
#include <stdexcept>

#include <libconfig.h++>

namespace Lichtenstein::Server {
    class ConfigManager {
        public:
            static void readConfig(const std::string &path);

        // you should not call this!
            ConfigManager(const std::string &path);
        
        public:
            static const bool getBool(const std::string &path, const bool fallback = false) {
                try {
                    return sharedInstance()->getPrimitive(path);
                } catch (KeyException &) {
                    return fallback;
                }
            }
            static const long getNumber(const std::string &path, const long fallback = -1) {
                try {
                    return sharedInstance()->getPrimitive(path);
                } catch (KeyException &) {
                    return fallback;
                }
            }
            static const unsigned long getUnsigned(const std::string &path, const unsigned long fallback = 0) {
                try {
                    return sharedInstance()->getPrimitive(path);
                } catch (KeyException &) {
                    return fallback;
                }
            }
            static const double getDouble(const std::string &path, const double fallback = 0) {
                try {
                    return sharedInstance()->getPrimitive(path);
                } catch (KeyException &) {
                    return fallback;
                }
            }
            static const std::string get(const std::string &path, const std::string &fallback = "") {
                try {
                    return sharedInstance()->getPrimitive(path);
                } catch (KeyException &) {
                    return fallback;
                }
            }

        private:
            static std::shared_ptr<ConfigManager> sharedInstance();
            
            libconfig::Setting &getPrimitive(const std::string &path);

        private:
            std::mutex cfgLock;
            std::unique_ptr<libconfig::Config> cfg = nullptr;

        // Error types
        public:
            // Failed to read/write config
            class IOException : public std::runtime_error {
                friend class ConfigManager;
                private:
                    IOException(const std::string &what) : std::runtime_error(what) {}
            };
            
            // Could not find or convert key
            class KeyException : public std::runtime_error {
                friend class ConfigManager;
                private:
                    KeyException(const std::string &what) : std::runtime_error(what) {}
            };

            // Failed to parse config
            class ParseException : public std::runtime_error {
                friend class ConfigManager;
                private:
                    ParseException(const std::string &what, int line) : 
                        std::runtime_error(what), line(line) {}
                    int line = -1;

                public:
                    int getLine() const {
                        return this->line;
                    }
            };

    };
}

#endif
