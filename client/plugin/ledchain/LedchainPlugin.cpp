#include "LedchainPlugin.h"
#include "LedchainChannel.h"

#include "PluginInfo.h"
#include "PluginManager.h"

#include <version.h>

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <sstream>

using namespace Lichtenstein::Client::Output;
using namespace Lichtenstein::Plugin::Ledchain;

// dummy channels
std::vector<std::shared_ptr<LedchainChannel>> channels;

/**
 * Splits a string containing comma separated numbers.
 */
static void SplitCsv(const std::string &str, std::vector<size_t> &out) {
    std::stringstream ss(str);

    for(int i; ss >> i;) {
        out.push_back(i);
        if(ss.peek() == ',') {
            ss.ignore();
        }
    }
}

/**
 * Initializer function
 */
static int PluginInit(PluginManager *mgr, std::vector<std::shared_ptr<IOutputChannel>> &outChannels) {
    std::vector<size_t> size, format;

    // how many channels do we want?
    auto numChannels = mgr->cfgGetUnsigned("plugin.ledchain.channels", 0);
    if(numChannels == 0) {
        return 0;
    }

    // read the format and size; this is a comma-separated list
    const auto sizeStr = mgr->cfgGet("plugin.ledchain.pixels", "");
    SplitCsv(sizeStr, size);

    for(const auto s : size) {
        if(s == 0) {
            std::cerr << "Invalid size " << s << "; '" << sizeStr << "'" << std::endl;
            return -1;
        }
    }

    const auto formatStr = mgr->cfgGet("plugin.ledchain.format", "");
    SplitCsv(formatStr, format);

    for(const auto f : format) {
        if(f != 0 && f != 1) {
            std::cerr << "Invalid format " << f << "; '" << formatStr << "'" << std::endl;
            return -2;
        }
    }

    if(format.size() != numChannels || size.size() != numChannels) {
        std::cerr << "Number of size data (" << size.size() << ") and format data ("
                  << format.size() << ") must match number of channels (" << numChannels << ")"
                  << std::endl;
    }

    // create the channels
    for(size_t i = 0; i < numChannels; i++) {
        auto channel = std::make_shared<LedchainChannel>(i, size[i], format[i]);
        channels.push_back(channel);
        outChannels.push_back(channel);
    }

    return 0;
}
/**
 * Closes all channels and cleans up resources.
 */
static int PluginShutdown() {
    // clear that shit
    channels.clear();

    return 0;
}

/**
 * Define the main plugin info struct
 */
extern "C" plugin_info_t __lichtenstein_output_plugin_info = {
    .magic = kOutputPluginMagic,
    .name = "ledchain output plugin",
    .shortname = "ledchain",
    .version = gVERSION,

    .init = PluginInit,
    .shutdown = PluginShutdown,
};
