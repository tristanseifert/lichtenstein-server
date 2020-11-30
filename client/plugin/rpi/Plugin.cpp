#include "Plugin.h"
#include "OutputChannel.h"

#include "PluginInfo.h"
#include "PluginManager.h"

#include <version.h>

#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <cstring>

using namespace Lichtenstein::Client::Output;
using namespace Lichtenstein::Plugin::RPi;

// shared plugin
static std::shared_ptr<Plugin> gPlugin = nullptr;

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
 * Initializes the output plugin.
 */
Plugin::Plugin(PluginManager *_mgr) : manager(_mgr) {
    // general driver config
    memset(&this->driver, 0, sizeof(ws2811_t));

    this->driver.freq = 810000; // 810 kHz
    this->driver.dmanum = 10;
}
/**
 * Reads config and creates the output channels.
 */
int Plugin::start(std::vector<std::shared_ptr<Client::Output::IOutputChannel>> &outChannels) {
    int err;
    std::vector<size_t> size, format;

    // how many channels do we want?
    auto numChannels = this->manager->cfgGetUnsigned("plugin.rpi.channels", 0);
    if(numChannels == 0) {
        return 0;
    } else if(numChannels > 2) {
        std::cerr << "Invalid number of channels: " << numChannels << std::endl;
        return -3;
    }

    // read the format and size; this is a comma-separated list
    const auto sizeStr = this->manager->cfgGet("plugin.rpi.pixels", "");
    SplitCsv(sizeStr, size);

    for(const auto s : size) {
        if(s == 0) {
            std::cerr << "Invalid size " << s << "; '" << sizeStr << "'" << std::endl;
            return -1;
        }
    }

    const auto formatStr = this->manager->cfgGet("plugin.rpi.format", "");
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
    static const unsigned int kGpioNum[2] = {
        18, 13
    };

    for(size_t i = 0; i < numChannels; i++) {
        if(format[i] == 0) {
            this->driver.channel[i].strip_type = WS2812_STRIP;
        } else if(format[i] == 1) {
            this->driver.channel[i].strip_type = SK6812W_STRIP;
        }

        this->driver.channel[i].gpionum = kGpioNum[i];
        this->driver.channel[i].count = size[i];
        this->driver.channel[i].brightness = 255;
    }

    // try to initialize the driver
    err = ws2811_init(&this->driver);
    if(err != WS2811_SUCCESS) {
        std::cerr << "ws2811_init() failed: " << err << std::endl;
        return -128;
    }

    // set the output buffers for each channel
    for(size_t i = 0; i < numChannels; i++) {
        this->channels[i]->setDriverBuffer(this->driver.channel[i].leds);
    }

    return 0;

}



/**
 * Tears down the output plugin, including the ws2812 hardware.
 */
Plugin::~Plugin() {

}

/**
 * Requests shutdown of all channels and the WS2812 hardware.
 */
int Plugin::stop() {
    // destroy channels
    this->channels.clear();

    // yeet it up
    ws2811_fini(&this->driver);

    // probably success
    return 0;
}

/**
 * Once we've received an output for all channels, we need to actually output the data.
 */
void Plugin::willOutputChannel(size_t channel) {
    this->numOutput++;

    if(this->numOutput == this->channels.size()) {
        std::lock_guard<std::mutex> lg(this->driverLock);
        ws2811_render(&this->driver);

        this->numOutput = 0;
    }
}
