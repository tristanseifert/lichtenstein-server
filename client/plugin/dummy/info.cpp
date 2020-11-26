#include "PluginInfo.h"
#include "PluginManager.h"
#include "DummyChannel.h"

#include <version.h>

#include <iostream>

// dummy channels
std::vector<std::shared_ptr<DummyChannel>> channels;

using namespace Lichtenstein::Client::Output;

/**
 * Initializer function
 */
static int PluginInit(PluginManager *mgr, std::vector<std::shared_ptr<IOutputChannel>> &outChannels) {
    // how many channels do we want?
    auto numChannels = mgr->cfgGetUnsigned("plugin.dummy.channels", 0);
    if(numChannels == 0) {
        return 0;
    }

    // read the format and size
    size_t size = mgr->cfgGetUnsigned("plugin.dummy.pixels", 0);
    if(size == 0) {
        return -1;
    }
    size_t format = mgr->cfgGetUnsigned("plugin.dummy.format", 0);
    if(format != 0 && format != 1) {
        return -2;
    }

    // create the dummy channels
    for(size_t i = 0; i < numChannels; i++) {
        auto channel = std::make_shared<DummyChannel>(i, size, format);
        channels.push_back(channel);
        outChannels.push_back(channel);
    }

    return 0;
}

/**
 * Closes all channels and cleans up resources.
 */
static int PluginShutdown() {
    // release our references to the channels
    channels.clear();

    return 0;
}

/**
 * Define the main plugin info struct
 */
extern "C" plugin_info_t __lichtenstein_output_plugin_info = {
    .magic = kOutputPluginMagic,
    .name = "Dummy output plugin",
    .shortname = "dummy",
    .version = gVERSION,

    .init = PluginInit,
    .shutdown = PluginShutdown,
};
