/**
 * All effects render into the global framebuffer, which is a 1D structure of
 * HSI triples. Output channels take a number of pixels from a particular
 * offset into the framebuffer.
 */
#ifndef RENDER_FRAMEBUFFER_H
#define RENDER_FRAMEBUFFER_H

#include <cstddef>
#include <memory>

namespace Lichtenstein::Server::Render {
    class HSIPixel;
    class RGBPixel;
    class RGBWPixel;

    class Framebuffer {
        public:
            Framebuffer();
            virtual ~Framebuffer();

        public:
            void copyOut(size_t start, size_t num, HSIPixel *out);
            void copyOut(size_t start, size_t num, RGBPixel *out);
            void copyOut(size_t start, size_t num, RGBWPixel *out);

        private:
            void assertInBounds(size_t start, size_t num);

        private:
            std::unique_ptr<HSIPixel[]> pixels;
            unsigned int numPixels;
    };
}

#endif
