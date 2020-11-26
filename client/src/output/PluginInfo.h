#ifndef PLUGININFO_H
#define PLUGININFO_H

#include <cstdint>
#include <vector>
#include <memory>

#include "IOutputChannel.h"

/// magic value
constexpr static const uint32_t kOutputPluginMagic = 'BLAZ';

namespace Lichtenstein::Client::Output {
    class PluginManager;
}

/**
 * Define this structure under the name "__lichtenstein_output_plugin_info" to
 * export the plugin.
 */
typedef struct plugin_info {
    // Magic value, this is currently 'BLAZ'
    uint32_t magic;
    // Name of the plugin (long)
    const char *name;
    // Short name of the plugin (used for instantiation)
    const char *shortname;
    // Optional version string
    const char *version;

    // Initializes the plugin; push initialized channels to the vector
    int (*init)(Lichtenstein::Client::Output::PluginManager *,
                std::vector<std::shared_ptr<Lichtenstein::Client::Output::IOutputChannel>> &);
    // Tear down (before the plugin is unloaded)
    int (*shutdown)(void);
} plugin_info_t;

#endif
