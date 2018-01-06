#include "OutputMapper.h"

#include "DataStore.h"
#include "Framebuffer.h"
#include "Routine.h"

#include <glog/logging.h>

#include <map>
#include <set>

using namespace std;

/**
 * Initializes the output mapper.
 */
OutputMapper::OutputMapper(DataStore *s, Framebuffer *f, INIReader *reader) {
	this->store = s;
	this->fb = f;

	this->config = reader;
}

/**
 * Cleans up anything created by the output mapper.
 */
OutputMapper::~OutputMapper() {

}

/**
 * Adds a mapping between the specified output group and routine state.
 *
 * If the group has already been mapped to, that mapping is removed. If the
 * group being added is an ubergroup, any mappings to existing groups will also
 * be removed.
 */
void OutputMapper::addMapping(OutputMapper::OutputGroup *g, Routine *r) {
	// check if it's an ubergroup
	OutputMapper::OutputUberGroup *ug = dynamic_cast<OutputMapper::OutputUberGroup *>(g);

	if(ug) {
		this->_removeMappingsInUbergroup(ug);
	} else {
		this->removeMappingForGroup(g);
	}

	// we've removed any stale mappings so insert it
	this->outputMap[g] = r;
}

/**
 * Removes an output mapping for the given group.
 */
void OutputMapper::removeMappingForGroup(OutputGroup *g) {
	auto element = this->outputMap.find(g);

	if(element != this->outputMap.end()) {
		this->outputMap.erase(element);
	} else {
		LOG(INFO) << "Attempted to remove mapping for output group " << g
				  << ", but that group doesn't have any existing mappings";
	}
}

/**
 * Removes output mappings for any and all groups in the given ubergroup.
 */
void OutputMapper::_removeMappingsInUbergroup(OutputMapper::OutputUberGroup *ug) {
	// iterate over all groups in the ubergroup
	for(auto group : ug->groups) {
		// find the group and remove it if it exists
		auto element = this->outputMap.find(group);

		if(element != this->outputMap.end()) {
			this->outputMap.erase(element);
		}
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
}

/**
 * Creates an output group that corresponds to a particular group in the
 * datastore.
 */
OutputMapper::OutputGroup::OutputGroup(DbGroup *g) {
	this->group = g;

	// allocate the buffer
	this->_resizeBuffer();
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
	this->buffer = static_cast<HSIPixel *>(calloc(this->bufferSz,
												  sizeof(HSIPixel)));

	// marks the buffer as having changed
	this->bufferChanged = true;
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
void OutputMapper::OutputGroup::copyIntoFramebuffer(Framebuffer *fb) {
	auto fbPointer = fb->getDataPointer();

	int fbStart = this->group->start;
	int fbEnd = this->group->end;

	for(int i = fbStart, j = 0; i <= fbEnd; i++, j++) {
		fbPointer[i] = this->buffer[j];
	}
}

/**
 * Compares two groups. They are considered equivalent if the underlying group
 * from the datastore has the same ID.
 */
bool operator==(const OutputMapper::OutputGroup& lhs, const OutputMapper::OutputGroup& rhs) {
	return (lhs.group == rhs.group);
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

#pragma mark - UberGroup Implementation
/**
 * Creates an Ubergroup with no members.
 */
OutputMapper::OutputUberGroup::OutputUberGroup() : OutputMapper::OutputGroup(nullptr) {

}

/**
 * Creates an Ubergroup with the specified members.
 */
OutputMapper::OutputUberGroup::OutputUberGroup(vector<OutputGroup *> &members) :
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
	// check whether the group even contains this group
	if(this->containsMember(group)) {
		for(auto i = this->groups.begin(); i != this->groups.end(); i++) {
			if(**i == *group) {
				this->groups.erase(*i);
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
 */
void OutputMapper::OutputUberGroup::copyIntoFramebuffer(Framebuffer *fb) {
	for(auto group : this->groups) {
		group->copyIntoFramebuffer(fb);
	}
}

/**
 * Returns the number of pixels in the group.
 */
int OutputMapper::OutputUberGroup::numPixels() const {
	int elements = 0;

	// sum up all of the groups' pixels
	for(auto group : this->groups) {
		elements += group->numPixels();
	}

	return elements;
}
