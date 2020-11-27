#include "Framebuffer.h"
#include "HSIPixel.h"
#include "RGBPixel.h"
#include "RGBWPixel.h"

#include "../proto/Syncer.h"

#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <random>

#include <time.h>

#include <Logging.h>
#include <ConfigManager.h>

using namespace Lichtenstein::Server::Render;

/**
 * Allocate the framebuffer memory.
 */
Framebuffer::Framebuffer() {
    // seed the observer token random generator. the time is ok since these are just tokens
    this->random.seed(time(nullptr) + 'FBUF');

    // read config
    this->numPixels = ConfigManager::getUnsigned("render.fb.size", 5000);
    this->pixels = std::make_unique<HSIPixel[]>(this->numPixels);

    XASSERT(this->numPixels <= kNotifyBitsetPixels, "Framebuffer size must be less than notify bitset size");

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
    // mark all notifications as undone
    {
        std::lock_guard<std::mutex> lg(this->doneBitsetLock);
        this->doneBitset.reset();
    }

    {
        std::lock_guard<std::mutex> lg(this->observerLock);

        this->pendingObservers.clear();
        for(auto& kv : this->observers) {
            this->pendingObservers.push_back(kv.first);
        }
    }

    // just return the frame counter
    return this->frameCounter;
}

/**
 * Rendering into the framebuffer has completed. The passed frame token must
 * match the value of the frame counter.
 */
void Framebuffer::endFrame(const FrameToken token) {
    // end the frame
    XASSERT(token == this->frameCounter, "Invalid frame token");

    // wait for all handler checking operations to complete

    // invoke all handlers that did not fire
    std::lock_guard<std::mutex> lg(this->observerLock);

    for(const auto token : this->pendingObservers) {
        const auto& [range, callback] = this->observers[token];
        callback(this->frameCounter);
    }

    // then, invoke the syncer (sends the sync output packet)
    Proto::Syncer::shared()->frameCompleted();

    // set up for next frame
    this->frameCounter += 1;
}



/**
 * Registers a new observer on a range of the framebuffer.
 *
 * When the entirety of the given range is marked as available, or when the frame rendering is
 * completed, the handler will be invoked. This guarantees it's always invoked once per frame.
 *
 * @note You cannot call this method from a callback.
 */
Framebuffer::ObserverToken Framebuffer::registerObserver(size_t start, size_t length, ObserverFunction const& f) {
    std::lock_guard<std::mutex> lg(this->observerLock);
    ObserverToken token;

    std::uniform_int_distribution<ObserverToken> dist(0, std::numeric_limits<ObserverToken>::max());

generate:;
    // generate a token and ensure it's unique
    token = dist(this->random);

    if(this->observers.find(token) != this->observers.end()) {
        goto generate;
    }

    // once we've got an unique token, insert it to the list of observers
    ObserverRange range(start, length);
    ObserverInfo info(range, f);

    this->observers[token] = info;
    Logging::trace("Registered observer: {} (len {}): {}", start, length, token);

    return token;
}

/**
 * Removes an existing observer.
 *
 * @note You cannot call this method from a callback.
 *
 * @throws If there is no such observer, an exception is thrown.
 */
void Framebuffer::removeObserver(ObserverToken token) {
    std::lock_guard<std::mutex> lg(this->observerLock);

    if(this->observers.erase(token) == 0) {
        throw std::invalid_argument("No observer registered with that token");
    }

    // ensure it's removed from the pending observers list as well
    auto it = std::find(this->pendingObservers.begin(), this->pendingObservers.end(), token);
    if(it != this->pendingObservers.end()) {
        this->pendingObservers.erase(it);
    }

    Logging::trace("Removed observer {}", token);
}

/**
 * Marks a region of pixels in the framebuffer as being completed.
 */
void Framebuffer::markRegionDone(size_t start, size_t num) {
    // toggle the bits on
    {
        std::lock_guard<std::mutex> lg(this->doneBitsetLock);

        for(size_t i = 0; i < num; i++) {
            this->doneBitset.set(start + i);
        }
    }

    this->runObservers();
}

/**
 * Invokes all observers that have become activated by a recent modification of the done bitset.
 *
 * TODO: this could/should be done asynchronously in the background
 */
void Framebuffer::runObservers() {
    std::vector<ObserverToken> invoked;

    std::lock_guard<std::mutex> lg(this->observerLock);

    // check each observer
    for(const auto& token : this->pendingObservers) {
        const auto& [range, callback] = this->observers[token];

        // check its bit range
        size_t base = std::get<0>(range);
        bool shouldCall = true;

        {
            std::lock_guard<std::mutex> lg(this->doneBitsetLock);

            for(size_t i = 0; i < std::get<1>(range); i++) {
                if(!this->doneBitset[base + i]) {
                    shouldCall = false;
                    break;
                }
            }
        }

        if(!shouldCall) continue;

        // invoke the callback
        callback(this->frameCounter);
        invoked.push_back(token);
    }

    // ensure that the callbacks do not get invoked again
    for(const auto& token : invoked) {
        auto it = std::find(this->pendingObservers.begin(), this->pendingObservers.end(), token);

        if(it != this->pendingObservers.end()) {
            this->pendingObservers.erase(it);
        }
    }
}
