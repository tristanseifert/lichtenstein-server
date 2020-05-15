/**
 * Combines several disjoint groups into one logical render target. It
 * automagically handles splitting pixel data to the correct places in the
 * framebuffer.
 */
#ifndef RENDER_MULTIGROUPTARGET_H
#define RENDER_MULTIGROUPTARGET_H

#include "IRenderTarget.h"
#include "IGroupContainer.h"

#include <vector>
#include <mutex>

namespace Lichtenstein::Server::Render {
    class Framebuffer;
    
    class MultiGroupTarget: public IRenderTarget, public IGroupContainer {
        using FbPtr = std::shared_ptr<Framebuffer>;
        using DbGroup = Lichtenstein::Server::DB::Types::Group;
        
        public:
            MultiGroupTarget() = delete;
            MultiGroupTarget(FbPtr fb) : fb(fb) {}
            MultiGroupTarget(FbPtr fb, const DbGroup &group);
            MultiGroupTarget(FbPtr fb, const std::vector<DbGroup> &groups);

            void inscreteFrame(std::shared_ptr<IRenderable> in);
            size_t numPixels() const;

        public:
            bool contains(int id) const;
            void insertGroup(int index, const DbGroup &group);
            void removeGroup(int id);

            /**
             * Returns the number of groups that this multigroup contains.
             */
            size_t numGroups() const {
                return this->groups.size();
            }
            /**
             * Allow mutation of the group membership.
             */
            bool isMutable() const {
                return true;
            }
            
        protected:
            void getGroupIds(std::vector<int> &outIds) const;
            
        public:
            inline bool operator==(const MultiGroupTarget &rhs) const noexcept {
                return (this->groups == rhs.groups);
            }

        private:
            struct OutputGroup {
                int groupId = -1;

                int fbOffset;
                int length;
            
                inline bool operator==(const OutputGroup &rhs) const noexcept {
                    return (this->groupId == rhs.groupId);
                }
            };

            std::recursive_mutex groupsLock;
            std::vector<OutputGroup> groups;

        private:
            // framebuffer into which we write data
            FbPtr fb;
    };
}

#endif
