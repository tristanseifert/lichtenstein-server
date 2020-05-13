#include "Framebuffer.h"
#include "HSIPixel.h"
#include "RGBPixel.h"
#include "RGBWPixel.h"

#include <stdexcept>
#include <algorithm>
#include <iterator>

#include "../Logging.h"
#include "../ConfigManager.h"

using namespace Lichtenstein::Server::Render;

/**
 * Allocate the framebuffer memory.
 */
Framebuffer::Framebuffer() {
    this->numPixels = ConfigManager::getUnsigned("render.fb.size", 5000);
    this->pixels = std::make_unique<HSIPixel[]>(this->numPixels);

    Logging::debug("Framebuffer is {} pixels at 0x{:x}", this->numPixels, 
            (void*)&this->pixels);
}
/**
 * Cleans up framebuffer memory.
 */
Framebuffer::~Framebuffer() {
    this->pixels = nullptr;
}



/**
 * Copies pixels out from the framebuffer. The assumption is that `out` can
 * hold at least `num` pixels.
 */
void Framebuffer::copyOut(size_t start, size_t num, HSIPixel *out) {
    this->assertInBounds(start, num);
    std::copy(this->pixels.get(), (this->pixels.get())+num, out);

}
/**
 * Copies pixels out from the framebuffer, converting them to 8-bit RGB in the
 * process.
 */
void Framebuffer::copyOut(size_t start, size_t num, RGBPixel *out) {
    this->assertInBounds(start, num);
    std::copy(this->pixels.get(), (this->pixels.get())+num, out);
}
/**
 * Copies pixels out from the framebuffer, converting them to 8-bit RGBW in the
 * process.
 */
void Framebuffer::copyOut(size_t start, size_t num, RGBWPixel *out) {
    this->assertInBounds(start, num);
    std::copy(this->pixels.get(), (this->pixels.get())+num, out);
}



/**
 * Ensures that the range falls completely inside the range of pixels in the
 * framebuffer.
 */
void Framebuffer::assertInBounds(size_t start, size_t num) {
    // start musn't be more than the size
    if(start >= this->numPixels) {
        throw std::out_of_range("Starting index is outside framebuffer");
    }
    // ensure the length wouldn't take us over
    else if((start+num) >= this->numPixels) {
        throw std::out_of_range("Can't read past end of framebuffer");
    }
}

