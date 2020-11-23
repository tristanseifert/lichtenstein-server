#include "GroupTarget.h"
#include "HSIPixel.h"
#include "IRenderable.h"
#include "Framebuffer.h"

#include <Logging.h>
#include "db/DataStorePrimitives.h"

using namespace Lichtenstein::Server::Render;



/**
 * Initializes a group target that will output to the specified group's
 * framebuffer area.
 */
GroupTarget::GroupTarget(const DbGroup &group) {
    this->fbOffset = group.startOff;
    this->length = (group.endOff - group.startOff);
    this->mirrored = group.mirrored;
    this->groupId = group.id;
}

/**
 * Initializes a group target that outputs to a particular section of the
 * framebuffer.
 */
GroupTarget::GroupTarget(size_t fbOffset, size_t numPixels, bool mirrored) : 
    fbOffset(fbOffset), length(numPixels), mirrored(mirrored) {
    // nothing
}

/**
 * Copies pixel data out of the renderable. The start offset is fixed at 0.
 */
void GroupTarget::inscreteFrame(FbPtr fb, std::shared_ptr<IRenderable> in) {
    XASSERT(in, "Input renderable is required");

    auto fbPtr = fb->getPtr(this->fbOffset, this->length);
    in->copyOut(0, this->length, fbPtr, this->mirrored);

    fb->markRegionDone(this->fbOffset, this->length);
}

/**
 * Gets the group ID we were created with.
 */
void GroupTarget::getGroupIds(std::vector<int> &out) const {
    XASSERT(this->groupId > 0, "No group id in group target");

    out.push_back(this->groupId);
}
