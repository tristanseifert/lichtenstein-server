/**
 * Interface allowing classes to transform framebuffer data before it is sent
 * to the output devices.
 */
#ifndef RENDER_IPIXELTRANSFORMER_H
#define RENDER_IPIXELTRANSFORMER_H

#include <memory>
#include <mutex>

#include "FbRange.h"

namespace Lichtenstein::Server::Render {
    class Framebuffer;

    class IPixelTransformer {
        public:
            /**
             * Apply transformation on the given framebuffer. The range to
             * operate on is specified.
             */
            virtual void transform(std::shared_ptr<Framebuffer>, 
                    const FbRange &) = 0;

            /**
             * Gets the transform lock.
             */
            virtual void lock() {
                this->transformLock.lock();
            }

            /**
             * Releases the transform lock.
             */
            virtual void unlock() {
                this->transformLock.unlock();
            }

        private:
            std::mutex transformLock;
    };
}

#endif
