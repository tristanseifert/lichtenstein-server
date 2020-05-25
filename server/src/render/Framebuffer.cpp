#include "Framebuffer.h"
#include "HSIPixel.h"
#include "RGBPixel.h"
#include "RGBWPixel.h"

#include <stdexcept>
#include <algorithm>
#include <iterator>

#include "Logging.h"
#include "ConfigManager.h"

using namespace Lichtenstein::Server::Render;

/**
 * Allocate the framebuffer memory.
 */
Framebuffer::Framebuffer() {
    this->numPixels = ConfigManager::getUnsigned("render.fb.size", 5000);
    this->pixels = std::make_unique<HSIPixel[]>(this->numPixels);

    Logging::debug("Framebuffer is {} pixels at {}", this->numPixels,
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
    XASSERT(out, "Output pointer cannot be null");
    this->assertInBounds(start, num);
    std::copy(this->pixels.get(), (this->pixels.get())+num, out);

}
/**
 * Copies pixels out from the framebuffer, converting them to 8-bit RGB in the
 * process.
 */
void Framebuffer::copyOut(size_t start, size_t num, RGBPixel *out) {
    XASSERT(out, "Output pointer cannot be null");
    this->assertInBounds(start, num);
    std::copy(this->pixels.get(), (this->pixels.get())+num, out);
}
/**
 * Copies pixels out from the framebuffer, converting them to 8-bit RGBW in the
 * process.
 */
void Framebuffer::copyOut(size_t start, size_t num, RGBWPixel *out) {
    XASSERT(out, "Output pointer cannot be null");
    this->assertInBounds(start, num);
    std::copy(this->pixels.get(), (this->pixels.get())+num, out);
}


/**
 * Copies pixels into the framebuffer.
 */
void Framebuffer::copyIn(size_t start, size_t num, const HSIPixel *in) {
    XASSERT(in, "Input ptr cannot be null");
    this->assertInBounds(start, num);

    // perform the copy while we hold the write lock
    {
        std::lock_guard g(this->writeLock);
        std::copy(in, in+num, this->pixels.get());
    }
}

/**
 * Returns a pointer to the start of the requested region. The region is
 * checked to ensure that it falls entirely within the framebuffer.
 *
 * The responsibility lies with the caller not to write more than `num` bytes!
 * Doing so WILL create bugs and security issues.
 */
HSIPixel *Framebuffer::getPtr(size_t start, size_t num) {
    this->assertInBounds(start, num);

    return (this->pixels.get() + start);
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

/**
 * Indicates a new frame is beginning. We return our current frame counter
 * value here.
 */
Framebuffer::FrameToken Framebuffer::startFrame() {
    return this->frameCounter;
}

/**
 * Rendering into the framebuffer has completed. The passed frame token must
 * match the value of the frame counter.
 */
void Framebuffer::endFrame(const FrameToken token) {
    XASSERT(token == this->frameCounter, "Invalid frame token");
    this->frameCounter += 1;
}
