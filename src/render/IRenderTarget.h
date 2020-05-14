/**
 * Render targets contain the knowledge to take a renderable and output it
 * _somewhere_, which usually means knowing how to map one large renderable
 * buffer into various disjoint pieces of the framebuffer.
 */
#ifndef RENDER_IRENDERTARGET_H
#define RENDER_IRENDERTARGET_H

#include <memory>

namespace Lichtenstein::Server::Render {
    class IRenderable;

    class IRenderTarget {
        public:
            /**
             * Ingests a frame of pixel data from the render target, which has
             * just completed rendering a frame.
             *
             * This function does not need to be reentrant, but it should be
             * thread safe in that there are no guarantees which thread it will
             * be called on.
             */
            virtual void inscreteFrame(std::shared_ptr<IRenderable> in) = 0;
    };
}

#endif
