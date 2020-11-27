#ifndef OUTPUT_PLUGINMANAGER_H
#define OUTPUT_PLUGINMANAGER_H

#include <memory>
#include <vector>
#include <atomic>
#include <string>

// forward declare this
struct plugin_info;

namespace Lichtenstein::Client::Proto {
    class Client;
}

namespace Lichtenstein::Client::Output {
    class IOutputChannel;

    /**
     * Loads output plugins from disk and instantiates them if needed.
     */
    class PluginManager {
        friend class Lichtenstein::Client::Proto::Client;

        public:
            // don't call these, it is a Shared Instance(tm)
            PluginManager();
            virtual ~PluginManager();

        public:
            void terminate();

            void notifySyncOutput();

        public:
            static void start();
            static void stop();

            static std::shared_ptr<PluginManager> get() {
                return shared;
            }

        // functions exported for the use of plugins during initialization
        public:
            virtual const bool cfgGetBool(const std::string &path, const bool fallback = false);
            virtual const long cfgGetNumber(const std::string &path, const long fallback = -1);
            virtual const unsigned long cfgGetUnsigned(const std::string &path, const unsigned long fallback = 0);
            virtual const double cfgGetDouble(const std::string &path, const double fallback = 0);
            virtual const std::string cfgGet(const std::string &path, const std::string &fallback = "");
            virtual const struct timeval cfgGetTimeval(const std::string &path, const double fallback = 2);

        private:
#ifdef __APPLE__
            constexpr static const char *kPluginExtension = ".dylib";
#else
            constexpr static const char *kPluginExtension = ".so";
#endif
        private:
            static std::shared_ptr<PluginManager> shared;

        private:
            // if set, terminate() has been called
            std::atomic_bool hasTerminated;
            // Plugin handles
            std::vector<void *> pluginHandles;
            std::vector<const struct plugin_info *> pluginInfo;

            // Initialized channels
            std::vector<std::shared_ptr<IOutputChannel> > channels;
    };
}

#endif
