#include "MultiGroupTarget.h"
#include "HSIPixel.h"
#include "IRenderable.h"
#include "Framebuffer.h"

#include <stdexcept>

#include "../Logging.h"
#include "../db/DataStorePrimitives.h"

using namespace Lichtenstein::Server::Render;

/**
 * Initializes a multigroup with a single group as its sole member.
 */
MultiGroupTarget::MultiGroupTarget(const DbGroup &group) {
    this->appendGroup(group);
}

/**
 * Creates a multigroup with all of the groups from the input vector in the
 * identical order.
 */
MultiGroupTarget::MultiGroupTarget(const std::vector<DbGroup> &groups) {
    this->groups.reserve(groups.size());

    for(const auto &group : groups) {
        this->appendGroup(group);
    }
}



/**
 * Determines whether the groups list contains a group with the specified id.
 */
bool MultiGroupTarget::contains(int id) const {
    // iterate over all groups
    for(const auto &entry : this->groups) {
        if(entry.groupId == id) {
            return true;
        }
    }

    // no such group was found
    return false;
}

/**
 * Inserts a group into our internal list at the given index. An index of -1
 * will append to the end of the list.
 */
void MultiGroupTarget::insertGroup(int index, const DbGroup &group) {
    std::lock_guard lg(this->groupsLock);
    
    if(group.id == -1) {
        throw std::invalid_argument("Group must have an id");
    } else if(this->contains(group.id)) {
        throw std::invalid_argument("Duplicate groups are not allowed");
    }

    // build the entry
    OutputGroup entry = {
        .groupId = group.id,
        .fbOffset = group.startOff,
        .length = (group.endOff - group.startOff),
        .mirrored = group.mirrored
    };

    // insert it where needed
    if(index == -1) {
        this->groups.push_back(entry);
    } else {
        this->groups.at(index) = entry;
    }
}

/**
 * Removes the group with the given id from our list.
 */
void MultiGroupTarget::removeGroup(int id) {
    XASSERT(id > 0, "Group id must be positive");

    if(!this->contains(id)) {
        throw std::invalid_argument("No such group in multigroup");
    }

    // find the group and remove its entry
    for(auto it = this->groups.cbegin(); it != this->groups.cend(); ) {
        if(it->groupId == id) {
            it = this->groups.erase(it);
            break;
        } else {
            ++it;
        }
    }
}

/**
 * Gets the id's of every entry.
 */
void MultiGroupTarget::getGroupIds(std::vector<int> &outIds) const {
    for(const auto &e : this->groups) {
        outIds.push_back(e.groupId);
    } 
}



/**
 * The renderable has finished rendering, so copy the data into the framebuffer
 * per the groups offsets.
 */
void MultiGroupTarget::inscreteFrame(FbPtr fb, std::shared_ptr<IRenderable> in) {
    XASSERT(in, "Input renderable is required");
    
    std::lock_guard lg(this->groupsLock);

    for(const auto &e : this->groups) {
        auto fbPtr = fb->getPtr(e.fbOffset, e.length);
        in->copyOut(0, e.length, fbPtr, e.mirrored);
        
        Logging::debug("Fb offset {} = {}", e.fbOffset, *fbPtr);
    }
}

/**
 * Returns the total number of pixels taken up by all groups.
 */
size_t MultiGroupTarget::numPixels() const {
    size_t count = 0;

    for(const auto &e : this->groups) {
        count += e.length;
    }

    return count;
}
