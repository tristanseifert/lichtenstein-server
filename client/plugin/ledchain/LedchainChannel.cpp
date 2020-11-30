#include "LedchainChannel.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

using namespace Lichtenstein::Plugin::Ledchain;

/**
 * Initializes the channel.
 */
LedchainChannel::LedchainChannel(uint32_t i, size_t _numPixels, size_t _format) : index(i), 
    numPixels(_numPixels), format(_format) {
    // try to open the device
    this->openDevice();
}

/**
 * On shutdown, ensure we close the device.
 */
LedchainChannel::~LedchainChannel() {
    this->closeDevice();
}

/**
 * Opens the ledchain device.
 */
void LedchainChannel::openDevice() {
    int err;
    const std::string path = this->deviceFileName();

    err = open(path.c_str(), O_RDWR);
    if(err == -1) {
        std::cerr << "Failed to open '" << path << "': " << errno << std::endl;
        throw std::system_error(errno, std::generic_category(),  "failed to open ledchain");
    }

    this->fd = err;
}
/**
 * Closes the ledchain device.
 */
void LedchainChannel::closeDevice() {
    // TODO: write 0 pixels

    // close
    close(this->fd);
    this->fd = -1;
}


/**
 * Copies received pixel data into the output buffer.
 */
int LedchainChannel::updatePixelData(const size_t offset, const void *_data, const size_t dataLen) {
    // resize the buffer appropriately
    this->buffer.resize(dataLen);

    // then copy the data
    auto data = reinterpret_cast<const std::byte *>(_data);
    std::copy(data, data+dataLen, this->buffer.begin());

    // success :D
    return 0;
}

/**
 * Writes data to ledchain device
 */
int LedchainChannel::outputPixelData() {
    int err;

    // write
    err = write(this->fd, this->buffer.data(), this->buffer.size());

    if(err <= 0) {
        std::cerr << "Failed to write pixel data: " << errno << std::endl;
        return err;
    }
    if(err != this->buffer.size()) {
        std::cerr << "Partial write " << err << "; expected " << this->buffer.size() << std::endl;
    }

    return 0;
}
