#include "GroupTarget.h"
#include "HSIPixel.h"
#include "IRenderable.h"
#include "Framebuffer.h"

#include "../Logging.h"
#include "../db/DataStorePrimitives.h"

using namespace Lichtenstein::Server::Render;



/**
 * Initializes a group target that will output to the specified group's
 * framebuffer area.
 */
GroupTarget::GroupTarget(FbPtr fb, const DbGroup &group) {
    GroupTarget(fb, group.startOff, (group.endOff - group.startOff));
    
    this->groupId = group.id;
}

/**
 * Initializes a group target that outputs to a particular section of the
 * framebuffer.
 */
GroupTarget::GroupTarget(FbPtr fb, size_t fbOffset, size_t numPixels) : fb(fb),
fbOffset(fbOffset), numPixels(numPixels) {
    // nothing
}

/**
 * Copies pixel data out of the renderable. The start offset is fixed at 0.
 */
void GroupTarget::inscreteFrame(std::shared_ptr<IRenderable> in) {
    XASSERT(in, "Input renderable is required");

    auto fbPtr = this->fb->getPtr(this->fbOffset, this->numPixels);
    in->copyOut(0, this->numPixels, fbPtr);
}

