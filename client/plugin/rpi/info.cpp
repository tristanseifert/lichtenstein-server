/**
 * Exported plugin info struct and startup/shutdown methods
 */
#include "Plugin.h"

#include "PluginInfo.h"
#include "version.h"

using namespace Lichtenstein::Client::Output;
using namespace Lichtenstein::Plugin::RPi;

// shared plugin instance
static std::shared_ptr<Plugin> gPlugin = nullptr;

/**
 * Initializer function; this allocates the plugin and requests loading of the config.
 */
static int PluginInit(PluginManager *mgr, std::vector<std::shared_ptr<IOutputChannel>> &outChannels) {
    int err;

    // create plugin
    gPlugin = std::make_shared<Plugin>(mgr);

    // then, request creating channels
    err = gPlugin->start(outChannels);
    return err;
}

/**
 * Closes all channels and cleans up resources.
 */
static int PluginShutdown() {
    // stop request
    int err = gPlugin->stop();

    // clear its resources
    gPlugin = nullptr;

    return err;
}

/**
 * Define the main plugin info struct
 */
extern "C" plugin_info_t __lichtenstein_output_plugin_info = {
    .magic = kOutputPluginMagic,
    .name = "Raspberry Pi PWM output",
    .shortname = "rpi",
    .version = gVERSION,

    .init = PluginInit,
    .shutdown = PluginShutdown,
};

