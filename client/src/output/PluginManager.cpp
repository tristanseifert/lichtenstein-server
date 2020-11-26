#include "PluginManager.h"
#include "IOutputChannel.h"
#include "PluginInfo.h"

#include <Format.h>
#include <Logging.h>
#include <ConfigManager.h>

#include <errno.h>
#include <dlfcn.h>

#include <filesystem>

using namespace Lichtenstein::Client::Output;

/// singleton plugin manager instance
std::shared_ptr<PluginManager> PluginManager::shared = nullptr;

/**
 * Allocates the plugin manager; this will load plugins from disk and initialize them.
 */
void PluginManager::start() {
    XASSERT(shared == nullptr, "Repeated calls to PluginManager::start() not allowed");

    shared = std::make_shared<PluginManager>();
}

/**
 * Notifies plugins to clean up.
 */
void PluginManager::stop() {
    XASSERT(shared != nullptr, "Shared client must be set up");

    shared->terminate();
    shared = nullptr;
}



/**
 * Loads plugins from disk.
 */
PluginManager::PluginManager() {
    this->hasTerminated = false;

    int err;
    const plugin_info_t *info;

    // directory to read plugins from
    auto pathStr = ConfigManager::get("plugin.path", "");
    if(pathStr.empty()) {
        Logging::warn("The plugin.path config variable is not set; no output plugins available");
        return;
    }

    // iterate over it
    for(auto& p : std::filesystem::directory_iterator(pathStr)) {
        std::vector<std::shared_ptr<IOutputChannel>> newCh;

        const auto path = p.path().string();
        const auto extension = p.path().extension().string();
        if(extension != kPluginExtension) {
            continue;
        }

        // try to open the library
        (void) dlerror();
        void *handle = dlopen(path.c_str(), RTLD_NOW);
        if(!handle) {
            Logging::warn("Failed to load library {} ({}) {}", path, errno, dlerror());
            continue;
        }

        Logging::trace("Loaded output plugin {}", path);

        void *rawInfo = dlsym(handle, "__lichtenstein_output_plugin_info");
        if(!rawInfo) {
            Logging::warn("No info symbol in plugin {}", path);
            goto beach;
        }

        // validate its info struct
        info = reinterpret_cast<plugin_info_t *>(rawInfo);
        if(info->magic != kOutputPluginMagic) {
            Logging::warn("Invalid plugin magic for {}: {:x}", path, info->magic);
            goto beach;
        }

        // initialize the plugin
        err = info->init(this, newCh);
        if(err != 0) {
            Logging::error("Failed to initialize plugin {}: {}", path, err);
            goto beach;
        }

        Logging::trace("Plugin {} ({} '{}') registered {} channels", path, info->name,
                info->shortname, newCh.size());

        for(auto ch : newCh) {
            this->channels.push_back(ch);
        }

        // success
        this->pluginInfo.push_back(info);
        this->pluginHandles.push_back(handle);

        // do not fall through to next
        continue;

        // handle failure case
beach:;
        dlclose(handle);
        continue;
    }
}

/**
 * Cleans up all resources; primarily this ensures the termination handler was invoked.
 */
PluginManager::~PluginManager() {
    if(!this->hasTerminated) {
        Logging::error("API misuse! PluginManager destructed without calling terminate()!");
        this->terminate();
    }
}


/**
 * Notifies all loaded plugins to shut down.
 */
void PluginManager::terminate() {
    int err;

    // invoke all plugin shutdown handlers
    this->channels.clear();

    for(const auto & info : this->pluginInfo) {
        Logging::trace("Shutting down '{}'", info->shortname);
        err = info->shutdown();
        if(err) {
            Logging::error("Failed to shut down '{}': {}", info->name, err);
        }
    }

    // close the handles
    for(auto handle : this->pluginHandles) {
        err = dlclose(handle);

        if(err != 0) {
            Logging::error("Failed to close library handle {}: {}", handle, err);
        }
    }

    // set termination flag
    this->hasTerminated = true;
}



const bool PluginManager::cfgGetBool(const std::string &path, const bool fallback) {
    return ConfigManager::getBool(path, fallback);
}

const long PluginManager::cfgGetNumber(const std::string &path, const long fallback) {
    return ConfigManager::getNumber(path, fallback);
}

const unsigned long PluginManager::cfgGetUnsigned(const std::string &path, const unsigned long fallback) {
    return ConfigManager::getUnsigned(path, fallback);
}

const double PluginManager::cfgGetDouble(const std::string &path, const double fallback) {
    return ConfigManager::getDouble(path, fallback);
}

const std::string PluginManager::cfgGet(const std::string &path, const std::string &fallback) {
    return ConfigManager::get(path, fallback);
}

const struct timeval PluginManager::cfgGetTimeval(const std::string &path, const double fallback) {
    return ConfigManager::getTimeval(path, fallback);
}

