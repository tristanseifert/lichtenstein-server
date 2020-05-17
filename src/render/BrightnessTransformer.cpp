#include "BrightnessTransformer.h"
#include "Framebuffer.h"
#include "HSIPixel.h"

#include "../Logging.h"

using namespace Lichtenstein::Server::Render;

/**
 * Multiplies each pixel in the range with the factor.
 */
void BrightnessTransformer::transform(std::shared_ptr<Framebuffer> fb, 
        const FbRange & range) {
    auto ptr = fb->getPtr(range.getOffset(), range.getLength());

    for(size_t i = 0; i < range.getLength(); i++) {
        ptr->i *= this->factor;
        ptr++;
    }
}
