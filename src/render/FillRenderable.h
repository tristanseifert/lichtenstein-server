/**
 * Fills an output buffer with a fixed pixel value.
 */
#ifndef RENDER_FILLRENDERABLE_H
#define RENDER_FILLRENDERABLE_H

#include "IRenderable.h"
#include "HSIPixel.h"

namespace Lichtenstein::Server::Render {
    class FillRenderable: public IRenderable {
        public:
            FillRenderable(size_t numPixels) = delete;
            FillRenderable(size_t numPixels, const HSIPixel &value) : 
                value(value), IRenderable(numPixels) {}

            void render();
            void copyOut(size_t offset, size_t num, HSIPixel *out) const;
            void resize(size_t numPixels);

            HSIPixel getValue() const {
                return this->value;
            }

        private:
            HSIPixel value;
    };
}

#endif
