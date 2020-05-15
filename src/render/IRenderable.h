/**
 * Anything that renders pixel data should implement this interface. It 
 * contains the methods used by the rendering pipeline.
 */
#ifndef RENDER_IRENDERABLE_H
#define RENDER_IRENDERABLE_H

#include <cstddef>
#include <mutex>

namespace Lichtenstein::Server::Render {
    class HSIPixel;

    class IRenderable {
        public:
            IRenderable() = delete;
            IRenderable(size_t numPixels) : numPixels(numPixels) {}
            virtual ~IRenderable() {}

        public:
            /**
             * Rendering is about to begin. The pipeline calls this method on
             * every renderable (in an arbitrary order) before dispatching
             * rendering jobs.
             */
            virtual void prepare() {};

            /**
             * Produce a frame of pixel data. Typically, you would render into
             * a private buffer inside your implementation, which will be read
             * out later using the `copyOut()` call.
             *
             * Note that this function can and will be called from different
             * threads for each frame; you should not depend on any thread-
             * specific state. You do NOT need to implement reentrancy or take
             * precautions for thread safety; the pipeline guarantees that this
             * function is only called once per frame, from its own thread.
             *
             * This function should block until rendering is complete.
             */
            virtual void render() = 0;

            /**
             * Indicates that the rendering pipeline has finished all pending
             * rendering jobs.
             */
            virtual void finish() {};

            /**
             * Copies a range of pixel data out of the renderable's internal
             * buffer. This can be invoked multiple times to copy multiple
             * pieces of the buffer.
             *
             * Rendering pipeline guarantees that this function is not called
             * before render(), but it must be re-entrant and thread safe as it
             * may be called from multiple different threads. However, it will
             * never call this function with overlapping input or output ranges
             * between prepare() and finish() calls.
             *
             * Hint: using std::copy() will satisfy these requirements.
             */
            virtual void copyOut(size_t offset, size_t num, HSIPixel *out, bool mirrored = false) const = 0;

            /**
             * Resizes the renderable. To abort the resize, throw an exception.
             */
            virtual void resize(size_t numPixels) = 0;



            /**
             * Attempts to lock the renderable for use. This must be done 
             * before rendering, or modifying it. Note that the lock is
             * recursive, meaning the same thread can take the lock multiple
             * times. However, each call to `lock()` must be balanced with a
             * call to `unlock()`.
             */
            virtual void lock() {
                this->useLock.lock();
            }
            /**
             * Unlocks the renderable. This should be done immediately after
             * the owner is done with the renderable operation.
             */
            virtual void unlock() {
                this->useLock.unlock();
            }

        public:
            size_t getNumPixels() const {
                return this->numPixels;
            }

        protected:
            /// total number of pixels output for each frame
            size_t numPixels;
            /// per instance lock
            std::recursive_mutex useLock;
    };
}

#endif
