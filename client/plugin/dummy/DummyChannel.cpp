#include "DummyChannel.h"

/**
 * Received pixel data for the channel. We do nothing here.
 */
int DummyChannel::updatePixelData(const void *data, const size_t dataLen) {
    return 0;
}
