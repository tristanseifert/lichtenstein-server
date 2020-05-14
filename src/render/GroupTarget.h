/**
 * Serves as a target for a single output group. It simply copies the number
 * of pixels corresponding to the group out of the renderable and into the
 * framebuffer.
 */
#ifndef RENDER_GROUPTARGET_H
#define RENDER_GROUPTARGET_H

#include "IRenderTarget.h"

namespace Lichtenstein::Server::DB::Types {
    class Group;
}

namespace Lichtenstein::Server::Render {
    class Framebuffer;

    class GroupTarget: public IRenderTarget {
        using FbPtr = std::shared_ptr<Framebuffer>;
        using DbGroup = Lichtenstein::Server::DB::Types::Group;

        public:
            GroupTarget() = delete;
            GroupTarget(FbPtr fb, const DbGroup &group);
            GroupTarget(FbPtr fb, size_t fbOffset, size_t numPixels);

            void inscreteFrame(std::shared_ptr<IRenderable> in);

        public:
            inline bool operator==(const GroupTarget &rhs) const noexcept {
                if(rhs.groupId != -1 && this->groupId != -1) {
                    return (rhs.groupId == this->groupId);
                } else {
                    return (rhs.fbOffset == this->fbOffset) && 
                        (rhs.numPixels == this->numPixels);
                }
            }

        private:
            // ID of group if loaded from datastore 
            int groupId = -1;

            // destination region of framebuffer
            size_t fbOffset;
            size_t numPixels;

            // framebuffer into which we write data
            FbPtr fb;
    };
};

#endif
