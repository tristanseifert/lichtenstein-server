/**
 * All effects render into the global framebuffer, which is a 1D structure of HSI triples. Output
 * channels take a number of pixels from a particular offset into the framebuffer.
 *
 * Lastly, the framebuffer also handles notifying observers when the ranges they are interested in
 * become fully available. This is used to immediately send to nodes new pixel data as it becomes
 * available.
 */
#ifndef RENDER_FRAMEBUFFER_H
#define RENDER_FRAMEBUFFER_H

#include <cstddef>
#include <memory>
#include <mutex>
#include <bitset>
#include <vector>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <functional>

namespace Lichtenstein::Server::Render {
    class HSIPixel;
    class RGBPixel;
    class RGBWPixel;

    class Framebuffer {
        public:
            Framebuffer();
            virtual ~Framebuffer();

        public:
            using FrameToken = unsigned long long;
            using ObserverToken = unsigned long long;

            // callback for an observer; args are fb offset, length, current frame
            using ObserverFunction = std::function<void(FrameToken)>;

            size_t size() const {
                return this->numPixels;
            }

            void copyOut(size_t start, size_t num, HSIPixel *out);
            void copyOut(size_t start, size_t num, RGBPixel *out);
            void copyOut(size_t start, size_t num, RGBWPixel *out);

            void copyIn(size_t start, size_t num, const HSIPixel *in);
            HSIPixel *getPtr(size_t start, size_t num);

            ObserverToken registerObserver(size_t start, size_t length, ObserverFunction const& f);
            void removeObserver(ObserverToken token);
            void markRegionDone(size_t start, size_t length);

            FrameToken startFrame();
            void endFrame(const FrameToken);

        private:
            // size of notification bitset, in pixels
            constexpr static const size_t kNotifyBitsetPixels = 16384;

        private:
            void assertInBounds(size_t start, size_t num);

            void runObservers();

        private:
            std::mutex writeLock;

            std::unique_ptr<HSIPixel[]> pixels;
            unsigned int numPixels;

            unsigned long long frameCounter = 0;

        private:
            using ObserverRange = std::pair<size_t, size_t>; // start, length
            using ObserverInfo = std::pair<ObserverRange, ObserverFunction>;

            // pixels are marked as done as the frame progresses
            std::bitset<kNotifyBitsetPixels> doneBitset;
            std::mutex doneBitsetLock;

            // all registered observation functions
            std::unordered_map<ObserverToken, ObserverInfo> observers;
            std::mutex observerLock;

            // observers that have not yet been invoked
            std::vector<ObserverToken> pendingObservers;
    };
}

#endif
