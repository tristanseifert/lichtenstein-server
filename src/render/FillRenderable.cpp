#include "FillRenderable.h"
#include "HSIPixel.h"

#include <algorithm>

#include "../Logging.h"

using namespace Lichtenstein::Server::Render;

/**
 * We don't do anything here :)
 */
void FillRenderable::render() {
    // nothing
}

/**
 * Copies the fill value out into the output buffer. Since we haven't got any
 * rendering to do because there's no internal buffer, we handle everything
 * here.
 *
 * We effectively ignore the offset, since we fill the same value everywhere,
 * but we bounds check it for consistency anyways.
 */
void FillRenderable::copyOut(size_t offset, size_t num, HSIPixel *out, bool mirrored) {
    // validate args
    XASSERT(out, "Output pointer cannot be null");
    
    XASSERT(offset < this->numPixels, "Offset must be in bounds");
    XASSERT((offset + num) <= this->numPixels, "Length must be in bounds");

    // fill the output buffer
    std::fill(out, out+num, this->value);
}

/**
 * To support resizing, we just need to update our internal length counter.
 */
void FillRenderable::resize(size_t numPixels) {
    this->numPixels = numPixels;
}
