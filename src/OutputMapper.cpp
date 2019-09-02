#include "OutputMapper.h"

#include "DataStore.h"
#include "Framebuffer.h"
#include "Routine.h"

#include <glog/logging.h>

#include <map>
#include <set>

/**
 * Initializes the output mapper.
 */
OutputMapper::OutputMapper(std::shared_ptr<DataStore> db,
                           std::shared_ptr<Framebuffer> fb,
                           std::shared_ptr<INIReader> ini) : store(db), fb(fb),
                                                             config(ini) {

}

/**
 * Cleans up anything created by the output mapper.
 */
OutputMapper::~OutputMapper() {

}

/**
 * Prints the output map.
 */
void OutputMapper::printMap(void) {
	std::stringstream str;

	for(auto elem : this->outputMap) {
		str << elem.first << ": " << elem.second << std::endl;
	}

	LOG(INFO) << "Output map: " << str.str();
}

/**
 * Adds a mapping between the specified output group and routine state.
 *
 * If the group has already been mapped to, that mapping is removed. If the
 * group being added is an ubergroup, any mappings to existing groups will also
 * be removed.
 */
void OutputMapper::addMapping(OutputMapper::OutputGroup *g, Routine *r) {
	// check that iput is not null
	if(g == nullptr || r == nullptr) {
		LOG(ERROR) << "addMapping called with null group or routine!";
		return;
	}

	VLOG(1) << "Adding mapping for " << g;

	// take the lock for this scope
	std::lock_guard<std::recursive_mutex>(this->outputMapLock);

	// check if it's an ubergroup
	OutputMapper::OutputUberGroup *ug = dynamic_cast<OutputMapper::OutputUberGroup *>(g);

	if(ug != nullptr) {
		VLOG(1) << "Is ubergroup, removing mappings for it";

		// make sure it's not empty (wtf)
		if(ug->groups.size() == 0) {
			throw OutputMapper::invalid_ubergroup();
		}

		this->_removeMappingsInUbergroup(ug);
	} else {
		this->removeMappingForGroup(g);
	}

	// remove any empty ubergroups
	this->removeEmptyUbergroups();

	// we've removed any stale mappings so insert it
	this->outputMap[g] = r;

	this->printMap();
}

/**
 * Removes an output mapping for the given group.
 */
void OutputMapper::removeMappingForGroup(OutputGroup *g) {
	// check that iput is not null
	if(g == nullptr) {
		LOG(ERROR) << "removeMappingForGroup called with null group!";
		return;
	}

	// take the lock for this scope
	std::lock_guard<std::recursive_mutex>(this->outputMapLock);

	VLOG(1) << "Removing mapping for " << g;
	this->printMap();

	bool deleted = false;

	// search for it in the groups themselves
	for(auto it = this->outputMap.cbegin(); it != this->outputMap.cend();) {
		// make sure the group is not null
		if(it->first != nullptr) {
			VLOG(1) << "Group: " << it->first;

			// if the groups are equal
			if(*std::get<0>(*it) == *g) {
				// delete it
				it = this->outputMap.erase(it);
				deleted = true;
			} else {
				// otherwise, check the next one.
				++it;
			}
		}
		// group is null: this should never happen.
		else {
			++it;
		}
	}

	LOG(INFO) << "Deleted: " << deleted;

	// search for it in the groups itself
	if(!deleted) {
		// we couldn't find it, so we have to check the ubergroups
		for(auto [group, routine] : this->outputMap) {
			auto ubergroup = dynamic_cast<OutputMapper::OutputUberGroup *>(group);

			if(ubergroup != nullptr) {
				// does this ubergroup contain this group?
				if(ubergroup->containsMember(g)) {
					// yee! so delete it
					ubergroup->removeMember(g);
					return;
				}
			}
		}

		// if we drop down here, it didn't exist :(
		LOG(INFO) << "Attempted to remove mapping for " << g
				  << ", but that group doesn't have any existing mappings";
	}

	// remove any empty ubergroups
	this->removeEmptyUbergroups();

	this->printMap();
}

/**
 * Removes any empty ubergroups.
 */
void OutputMapper::removeEmptyUbergroups(void) {
	// take the lock for this scope
	std::lock_guard<std::recursive_mutex>(this->outputMapLock);

	// check if there's any empty ubergroups
	for (auto it = this->outputMap.cbegin(); it != this->outputMap.cend();) {
		// attempt to cast it
		auto mapUg = dynamic_cast<OutputMapper::OutputUberGroup *>(it->first);

		if(mapUg) {
			// is its groups set empty?
			if(mapUg->numMembers() == 0) {
				// if so, remove it
				it = this->outputMap.erase(it);
			} else {
				++it;
			}
		} else {
			++it;
		}
	}
}

/**
 * Removes output mappings for any and all groups in the given ubergroup.
 */
void OutputMapper::_removeMappingsInUbergroup(OutputMapper::OutputUberGroup *ug) {
	// check that iput is not null
	if(ug == nullptr) {
		LOG(ERROR) << "_removeMappingsInUbergroup called with null ug!";
		return;
	}

	VLOG(1) << "Removing any conflicting mappings in ubergroup " << ug;

	// take the lock for this scope
	std::lock_guard<std::recursive_mutex>(this->outputMapLock);

	// check if there's any identical ubergroups
	for (auto it = this->outputMap.cbegin(); it != this->outputMap.cend();) {
		// attempt to cast it
		auto mapUg = dynamic_cast<OutputMapper::OutputUberGroup *>(it->first);

		if(mapUg) {
			// is it equal?
			if(*ug == *mapUg) {
				// if so, remove it
				it = this->outputMap.erase(it);
			} else {
				++it;
			}
		} else {
			++it;
		}
	}

	// TODO: the shits below are broken, but here's what we need to do:
	// 1. check through all other ubergroups. if any groups in the passed in
	//		ubergroup are in that group, remove those mappings from that group.
	// 2. check whether there's any standalone groups in the output map that are
	//		also in this ubergroup, and if so, remove them.

	// check if there's any ubergroups and whether this is a member in it
	for(auto [group, routine] : this->outputMap) {
		auto ubergroup = dynamic_cast<OutputMapper::OutputUberGroup *>(group);

		if(ubergroup != nullptr) {
			// iterate over all groups in the ubergroup we've been passed
			for(auto group : ug->groups) {
				// is this in the ubergroup we found?
				if(ubergroup->containsMember(group)) {
					// remove it
					ubergroup->removeMember(group);

					/*// if the ubergroup is empty, remove its mapping
					if(ubergroup->numMembers() == 0) {
						// XXX: infinite loop?
						this->removeMappingForGroup(ubergroup);
					}*/
				}
			}
		}
	}

	// iterate over all groups in the ubergroup
	for(auto group : ug->groups) {
		// search for it in the groups themselves
		for (auto it = this->outputMap.cbegin(); it != this->outputMap.cend();) {
			// if the groups are equal
			if(*it->first == *group) {
				// delete it
				it = this->outputMap.erase(it);
				// delete (*it).first; // TODO: memory leak?
			} else {
				++it;
			}
		}
	}
}


/**
 * Gets a reference to all real groups (aka groups that reference a single group
 * rather than an ubergroup)
 */
void OutputMapper::getAllGroups(std::vector<OutputGroup *> &groups) {
  // search for it in the groups themselves
	for(auto [group, routine] : this->outputMap) {
    // make sure the group is not null
    if(group != nullptr) {
      // is it an ubergroup?
      auto ubergroup = dynamic_cast<OutputMapper::OutputUberGroup *>(group);

      if(ubergroup) {
        // extract all groups in the ubergroup
        for(auto member : ubergroup->groups) {
          groups.push_back(member);
        }
      } else {
        groups.push_back(group);
      }
    }
    // group is null: this should never happen.
    else { }
  }
}

#pragma mark - Group Implementation
/**
 * Destroys the allocated buffer.
 */
OutputMapper::OutputGroup::~OutputGroup() {
	if(this->buffer) {
		free(this->buffer);
		this->buffer = nullptr;
	}

	// delete the group
	if(this->group) {
		// delete this->group; TODO: memory leak?
	}
}

/**
 * Creates an output group that corresponds to a particular group in the
 * datastore.
 */
OutputMapper::OutputGroup::OutputGroup(DbGroup *g) {
	this->group = g;

	// allocate the buffer
	if(g != nullptr) {
		this->_resizeBuffer();
	}
}

/**
 * Allocates the correct amount of memory in the framebuffer.
 */
void OutputMapper::OutputGroup::_resizeBuffer() {
	// de-allocate any old buffers
	if(this->buffer) {
		free(this->buffer);
		this->buffer = nullptr;
	}

	// allocate the buffer
	this->bufferSz = this->numPixels();

	if(this->bufferSz > 0) {
		this->buffer = static_cast<HSIPixel *>(calloc(this->bufferSz,
												  sizeof(HSIPixel)));
		// marks the buffer as having changed
		this->bufferChanged = true;
	} else {
		LOG(ERROR) << "Allocated buffer size 0";
		this->buffer = nullptr;
	}

	// VLOG(2) << "New buffer for " << *this << ": 0x" << this->buffer
			// << ", size " << this->bufferSz;
}

/**
 * Returns the number of pixels in the group.
 */
int OutputMapper::OutputGroup::numPixels() {
	return this->group->numPixels();
}

/**
 * Binds the buffer to the given routine. This is only done if either the buffer
 * or the routine itself changed since the last invocation.
 */
void OutputMapper::OutputGroup::bindBufferToRoutine(Routine *r) {
	if(r != this->bufferBoundRoutine || this->bufferChanged) {
		// attach the buffer
		r->attachBuffer(this->buffer, this->bufferSz);

		this->bufferBoundRoutine = r;
		this->bufferChanged = false;
	}
}

/**
 * Copies the pixel data for this group into the framebuffer at the correct
 * offsets.
 */
void
OutputMapper::OutputGroup::copyIntoFramebuffer(std::shared_ptr<Framebuffer> fb,
                                               HSIPixel *buffer) {
	// if buffer is nullptr, use the buffer we've been allocated previously
	if(buffer == nullptr) {
		buffer = this->buffer;
	}

	// get pointers
	auto fbPointer = fb->getDataPointer();

	int fbStart = this->group->start;
	int fbEnd = this->group->end;

	// VLOG_EVERY_N(1, 60) << "Buffer address: 0x" << this->buffer;

	// VLOG(1) << "Copying " << *this << " to " << fbStart << " to " << fbEnd;

	for(int i = fbStart, j = 0; i <= fbEnd; i++, j++) {
		// const HSIPixel p(double(i), 1, 1);
		// fbPointer[i] = p;
		fbPointer[i] = buffer[j];

    // scale for brightness
    fbPointer[i].i *= this->brightness;
	}

	// VLOG(1) << fbPointer[1] << "; " << this->buffer[1];
}

/**
 * Compares two groups. They are considered equivalent if the underlying group
 * from the datastore has the same ID.
 */
bool operator==(const OutputMapper::OutputGroup& lhs, const OutputMapper::OutputGroup& rhs) {
	// if either is an ubergroup, they're inequal
	if(typeid(lhs) == typeid(const OutputMapper::OutputUberGroup)) return false;
	else if(typeid(rhs) == typeid(const OutputMapper::OutputUberGroup)) return false;

	return (*lhs.group == *rhs.group);
}
bool operator!=(const OutputMapper::OutputGroup& lhs, const OutputMapper::OutputGroup& rhs) {
	return !(lhs == rhs);
}

bool operator< (const OutputMapper::OutputGroup& lhs, const OutputMapper::OutputGroup& rhs) {
	return (lhs.group < rhs.group);
}
bool operator> (const OutputMapper::OutputGroup& lhs, const OutputMapper::OutputGroup& rhs) {
	return rhs < lhs;
}
bool operator<=(const OutputMapper::OutputGroup& lhs, const OutputMapper::OutputGroup& rhs) {
	return !(lhs > rhs);
}
bool operator>=(const OutputMapper::OutputGroup& lhs, const OutputMapper::OutputGroup& rhs) {
	return !(lhs < rhs);
}

std::ostream &operator<<(std::ostream& strm, const OutputMapper::OutputGroup& obj) {
	// can we cast it to an ubergroup?
	try {
		strm << dynamic_cast<const OutputMapper::OutputUberGroup&>(obj);
	} catch(std::bad_cast e) {
		strm << "output group{group = " << obj.group << "}";
	}

	return strm;
}

#pragma mark - UberGroup Implementation
/**
 * Creates an Ubergroup with no members.
 */
OutputMapper::OutputUberGroup::OutputUberGroup() : OutputMapper::OutputGroup(nullptr) {

}

/**
 * Creates an Ubergroup with the specified members.
 */
OutputMapper::OutputUberGroup::OutputUberGroup(std::vector<OutputGroup *> &members) :
				 OutputMapper::OutputGroup(nullptr) {
	for(auto group : members) {
		this->groups.insert(group);
	}

	// resize the framebuffer
	this->_resizeBuffer();
}

/**
 * Cleans up any additional data structures that we allocated aside from the
 * group framebuffer.
 */
OutputMapper::OutputUberGroup::~OutputUberGroup() {
	// delete all groups
	for(auto group : this->groups) {
		// delete group; // TODO: memory leak
	}
}

/**
 * Inserts the specified output group to the ubergroup.
 */
void OutputMapper::OutputUberGroup::addMember(OutputGroup *group) {
	// make sure that it's not already in the set
	if(this->containsMember(group) == false) {
		this->groups.insert(group);
	}

	// resize the framebuffer
	this->_resizeBuffer();
}

/**
 * Removes the specified group from the ubergroup.
 */
void OutputMapper::OutputUberGroup::removeMember(OutputGroup *group) {
	// take the lock to modify the groups set
	// std::lock_guard<std::recursive_mutex>(this->groupsLock);

	// check whether the group even contains this group
	if(this->containsMember(group)) {
		for (auto it = this->groups.cbegin(); it != this->groups.cend();) {
			// if the groups are equal
			if(**it == *group) {
				// delete it
				it = this->groups.erase(it);
				// delete *it;
			} else {
				++it;
			}
		}
	}

	// resize the framebuffer
	this->_resizeBuffer();
}

/**
 * Checks whether the ubergroup contains the given member.
 */
bool OutputMapper::OutputUberGroup::containsMember(OutputGroup *inGroup) {
	// take the lock to modify the groups set
	// std::lock_guard<std::recursive_mutex>(this->groupsLock);

	// iterate over all elements and compare them
	for(auto group : this->groups) {
		if(*group == *inGroup) {
			return true;
		}
	}

	// if we fall through to the bottom, it's not in the group
	return false;
}

/**
 * Copies the pixel data for each of the groups that make up this ubergroup into
 * the appropriate places of the framebuffer.
 *
 * @note Stuff like brightness changes aren't handled here since we just call
 * into the groups' copy functions, which eventually will call into the base.
 */
void OutputMapper::OutputUberGroup::copyIntoFramebuffer(
        std::shared_ptr<Framebuffer> fb, HSIPixel *buffer) {
	// take the lock to modify the groups set
	// std::lock_guard<std::recursive_mutex>(this->groupsLock);

	for(auto group : this->groups) {
		group->copyIntoFramebuffer(fb, this->buffer);
	}
}

/**
 * Returns the number of pixels in the group.
 */
int OutputMapper::OutputUberGroup::numPixels() {
	// take the lock to modify the groups set
	// std::lock_guard<std::recursive_mutex>(this->groupsLock);

	int elements = 0;

	// sum up all of the groups' pixels
	for(auto group : this->groups) {
		elements += group->numPixels();
	}

	return elements;
}

/**
 * Compares two groups. They are considered equivalent if all output groups are
 * identical.
 */
bool operator==(const OutputMapper::OutputUberGroup& lhs, const OutputMapper::OutputUberGroup& rhs) {
   return (lhs.groups == rhs.groups);
}
bool operator!=(const OutputMapper::OutputUberGroup& lhs, const OutputMapper::OutputUberGroup& rhs) {
   return !(lhs == rhs);
}

std::ostream &operator<<(std::ostream& strm, const OutputMapper::OutputUberGroup& obj) {
	strm << "output ubergroup{groups = [";

	for(auto group : obj.groups) {
		strm << group << ", ";
	}

	strm << "]}";

	return strm;
}
