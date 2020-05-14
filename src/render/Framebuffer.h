/**
 * All effects render into the global framebuffer, which is a 1D structure of
 * HSI triples. Output channels take a number of pixels from a particular
 * offset into the framebuffer.
 */
#ifndef RENDER_FRAMEBUFFER_H
#define RENDER_FRAMEBUFFER_H

#include <cstddef>
#include <memory>
#include <mutex>

namespace Lichtenstein::Server::Render {
    class HSIPixel;
    class RGBPixel;
    class RGBWPixel;

    class Framebuffer {
        public:
            Framebuffer();
            virtual ~Framebuffer();

        public:
            size_t size() const {
                return this->numPixels;
            }

            void copyOut(size_t start, size_t num, HSIPixel *out);
            void copyOut(size_t start, size_t num, RGBPixel *out);
            void copyOut(size_t start, size_t num, RGBWPixel *out);

            void copyIn(size_t start, size_t num, const HSIPixel *in);
            HSIPixel *getPtr(size_t start, size_t num);

        private:
            void assertInBounds(size_t start, size_t num);

        private:
            std::mutex writeLock;

            std::unique_ptr<HSIPixel[]> pixels;
            unsigned int numPixels;
    };
}

#endif
