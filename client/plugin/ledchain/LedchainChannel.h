#include "IOutputChannel.h"

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

namespace Lichtenstein::Plugin::Ledchain {
    /**
     * Writes pixel data to ledchain device files.
     */
    class LedchainChannel: public Lichtenstein::Client::Output::IOutputChannel {
        public:
            LedchainChannel(uint32_t i, size_t _numPixels, size_t _format);
            ~LedchainChannel();

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

        private:
            // device file base
            constexpr static const char *kDeviceNameBase = "/dev/ledchain";
            // constexpr static const char *kDeviceNameBase = "./ledchain";

            // path to the device file
            const std::string deviceFileName() const {
                return std::string(kDeviceNameBase) + std::to_string(this->index);
            }

            // opens the device
            void openDevice();
            // close the ledchain device
            void closeDevice();

        private:
            int fd = -1;

            uint32_t index = 0;
            size_t numPixels = 0;
            // 0 = 3 bytes/pixel RGB, 1 = 4 bytes/pixel RGBW
            size_t format = 0;

            // pixel data buffer
            std::vector<std::byte> buffer;
    };
}
