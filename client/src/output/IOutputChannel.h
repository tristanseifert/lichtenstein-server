#ifndef OUTPUT_IOUTPUTCHANNEL_H
#define OUTPUT_IOUTPUTCHANNEL_H

#include <cstdint>
#include <cstddef>

namespace Lichtenstein::Client::Output {
    /**
     * This is the interface exported by plugins for output channels.
     *
     * Once connected, the client logic will register observers for each of the channels, and call
     * them when new pixel data arrives.
     */
    class IOutputChannel {
        public:
            virtual ~IOutputChannel() {};

        public:
            /// Returns the number of pixels the channel is configured for.
            virtual size_t getNumPixels() = 0;

            /// Pixel format of the channel (0 = RGB, 1 = RGBW)
            virtual size_t getPixelFormat() = 0;

            /// Node-unique channel id
            virtual size_t getChannelIndex() = 0;

            /// Pixel data in the desired format has been received
            virtual int updatePixelData(const void *data, const size_t dataLen) = 0;
    };
}

#endif
