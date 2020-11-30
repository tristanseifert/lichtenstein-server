#include "OutputChannel.h"
#include "Plugin.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

using namespace Lichtenstein::Plugin::RPi;

/**
 * Initializes the channel.
 */
OutputChannel::OutputChannel(Plugin *_p, uint32_t i, size_t _numPixels, size_t _format) : index(i), 
    numPixels(_numPixels), format(_format), plugin(_p) {
}

/**
 * On shutdown, ensure we close the device.
 */
OutputChannel::~OutputChannel() {

}


/**
 * Copies received pixel data into the output buffer.
 */
int OutputChannel::updatePixelData(const size_t offset, const void *_data, const size_t dataLen) {
    // resize the buffer appropriately
    this->buffer.resize(dataLen);

    // then copy the data
    auto data = reinterpret_cast<const std::byte *>(_data);
    std::copy(data, data+dataLen, this->buffer.begin());

    // possibly byteswap to get 0xWWRRGGBB

    // copy into LED buffer
    auto pixels = reinterpret_cast<const uint32_t *>(this->buffer.data());
    std::copy(pixels, pixels + this->numPixels, this->driverBuffer);

    // success :D
    return 0;
}

/**
 * Writes data to ledchain device
 */
int OutputChannel::outputPixelData() {
    this->plugin->willOutputChannel(this->index);

    return 0;
}
