#include "IOutputChannel.h"

/**
 * Implements a dummy channel; this simply absorbs channel data with no further
 * action. Really only useful for testing.
 */
class DummyChannel: public Lichtenstein::Client::Output::IOutputChannel {
    public:
        DummyChannel(size_t i, size_t _numPixels, size_t _format) : index(i), 
            numPixels(_numPixels), format(_format) {}

    public:
        /// Returns the number of pixels the channel is configured for.
        virtual size_t getNumPixels() {
            return this->numPixels;
        }

        /// Pixel format of the channel (0 = RGB, 1 = RGBW)
        virtual size_t getPixelFormat() {
            return this->format;
        }

        /// Channel index
        virtual size_t getChannelIndex() {
            return this->index;
        }

        virtual int updatePixelData(const void *data, const size_t dataLen);

    private:
        size_t index = 0;
        size_t numPixels = 0;
        size_t format = 0;

};
