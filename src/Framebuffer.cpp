#include "Framebuffer.h"

#include "DataStore.h"

#include <glog/logging.h>

#include <vector>
#include <tuple>
#include <iostream>

using namespace std;

/**
 * Allocates the framebuffer memory.
 */
Framebuffer::Framebuffer(DataStore *store, INIReader *reader) {
	this->store = store;
	this->config = reader;
}

/**
 * Cleans up the memory associated with the framebuffer.
 */
Framebuffer::~Framebuffer() {

}

/**
 * Recalculates the minimum size the framebuffer needs to be to fit all of the
 * groups defined in the database. This is called any time that groups are
 * modified.
 *
 * This goes through all defined groups and sums up their sizes.
 */
void Framebuffer::recalculateMinSize() {
	int minSize = 0;

	// fetch all groups, then iterate over them
	vector<DbGroup *> groups = this->store->getAllGroups();

	for(auto group : groups) {
		minSize += group->numPixels();

		// delete the groups; they were allocated just for this call
		delete group;
	}

	// resize the vector
	LOG(INFO) << "Total of " << minSize << " pixels across " << groups.size()
			  << " groups";

	this->resize(minSize);
}

/**
 * Returns a pointer to the framebuffer's pixel data.
 */
vector<HSIPixel>::iterator Framebuffer::getDataPointer() {
	return this->data.begin();
}

/**
 * Resizes the framebuffer to contain AT LEAST the given number of elements. If
 * its current size is larger than what it is resized to, elements at the end
 * will be deleted. If the framebuffer is grown, empty elements are added to
 * the end.
 */
void Framebuffer::resize(int elements) {
	// first, resize the vector to the correct size
	this->data.resize(elements, {0, 0, 0});

	// now, reserve that memory
	this->data.reserve(elements);
}
