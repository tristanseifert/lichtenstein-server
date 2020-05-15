/**
 * Serves as a target for a single output group. It simply copies the number
 * of pixels corresponding to the group out of the renderable and into the
 * framebuffer.
 */
#ifndef RENDER_GROUPTARGET_H
#define RENDER_GROUPTARGET_H

#include "IRenderTarget.h"
#include "IGroupContainer.h"

namespace Lichtenstein::Server::Render {
    class Framebuffer;

    class GroupTarget: public IRenderTarget, public IGroupContainer {
        using FbPtr = std::shared_ptr<Framebuffer>;
        using DbGroup = Lichtenstein::Server::DB::Types::Group;

        public:
            GroupTarget() = delete;
            GroupTarget(const DbGroup &group);
            GroupTarget(size_t fbOffset, size_t numPixels, bool mirrored);

            void inscreteFrame(FbPtr fb, std::shared_ptr<IRenderable> in);

        public:
            /**
             * Compares the given group ID against the one this group was
             * created with.
             */
            bool contains(int id) const {
                return (this->groupId == id);
            }

            /**
             * Gets the size of this group's container. Since we handle only a
             * single group, it will always be 1.
             */
            size_t numGroups() const {
                return 1;
            }

            /**
             * Returns the number of pixels required as input.
             */
            size_t numPixels() const {
                return this->length;
            }

        protected:
            /**
             * Gets out the group id we were created with.
             */
            void getGroupIds(std::vector<int> &out) const;

        public:
            inline bool operator==(const GroupTarget &rhs) const noexcept {
                if(rhs.groupId != -1 && this->groupId != -1) {
                    return (rhs.groupId == this->groupId);
                } else {
                    return (rhs.fbOffset == this->fbOffset) && 
                        (rhs.length == this->length) && 
                        (rhs.mirrored == this->mirrored);
                }
            }

        private:
            // ID of group if loaded from datastore 
            int groupId = -1;

            // destination region of framebuffer
            size_t fbOffset;
            size_t length;

            // whether the pixels are flipped
            bool mirrored;
    };
};

#endif
