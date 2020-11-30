#ifndef RPI_OUTPUTCHANNEL_H
#define RPI_OUTPUTCHANNEL_H

#include "IOutputChannel.h"

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

#include "ws2811.h"

namespace Lichtenstein::Plugin::RPi {
    class Plugin;

    /**
     * Writes pixel data to ledchain device files.
     */
    class OutputChannel: public Lichtenstein::Client::Output::IOutputChannel {
        public:
            OutputChannel(Plugin *, uint32_t i, size_t numPixels, size_t format);
            ~OutputChannel();

        public:
            /// Returns the number of pixels the channel is configured for.
            virtual size_t getNumPixels() {
                return this->numPixels;
            }

            /// Pixel format of the channel (0 = RGB, 1 = RGBW)
            virtual size_t getPixelFormat() {
                return this->format;
            }

            /// Channel index (corresponds directly to device file name)
            virtual uint32_t getChannelIndex() {
                return this->index;
            }

            virtual int updatePixelData(const size_t offset, const void *data, const size_t dataLen);

            virtual int outputPixelData();

            // set the driver buffer to use
            void setDriverBuffer(uint32_t *ptr) {
                this->driverBuffer = ptr;
            }

        private:
            uint32_t index = 0;
            size_t numPixels = 0;
            // 0 = 3 bytes/pixel RGB, 1 = 4 bytes/pixel RGBW
            size_t format = 0;

            // pixel data buffer
            std::vector<std::byte> buffer;
            // ws2811 lib buffer (0xWWRRGGBB)
            uint32_t *driverBuffer = nullptr;

            // plugin to call back into on output
            Plugin *plugin = nullptr;
    };
}

#endif
