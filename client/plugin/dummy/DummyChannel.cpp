#include "DummyChannel.h"

#include <iostream>

/**
 * Received pixel data for the channel. We do nothing here.
 */
int DummyChannel::updatePixelData(const size_t offset, const void *data, const size_t dataLen) {
    static int i = 0;

    if((i++ % 13) == 0) {
        std::cout << "(" << i << ") received data for ch " << this->index << " offset " << offset << ", len " << dataLen << std::endl;
    }

    return 0;
}

/**
 * Output pixel data
 */
int DummyChannel::outputPixelData() {
    static int i = 0;

    if((i++ % 13) == 0) {
        std::cout << "(" << i << ") output data for ch " << this->index << std::endl;
    }

    return 0;
}
