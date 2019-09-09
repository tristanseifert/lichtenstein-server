/**
 * Implements a thin wrapper around a region of memory – the framebuffer – into
 * which all effects write their data.
 *
 * This framebuffer can be resized at runtime to accommodate for changes in the
 * grouping configuration, and is safe for concurrent access by multiple threads
 * so long as no two threads attempt to WRITE to the same region of the buffer.
 *
 * Internally, the framebuffer stores each pixel as a HSI tuple. The index of
 * the H component is 0, S is at 1, and I is at 2.
 *
 * TODO: Resize the framebuffer if the configuration of groups is changed.
 */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "HSIPixel.h"

#include <vector>
#include <iostream>
#include <memory>

class DataStore;

class Framebuffer {
	friend class EffectRunner;

	public:
    Framebuffer(std::shared_ptr<DataStore> store);
		~Framebuffer();

		void recalculateMinSize();

		void resize(int elements);

		std::vector<HSIPixel>::iterator getDataPointer();

		/**
		 * Returns how many elements the framebuffer can accomodate. It is
		 * very important that no elements are added past this index.
		 */
		int size() {
			return this->data.capacity();
		}

	private:
    std::shared_ptr<DataStore> store;

		std::vector<HSIPixel> data;
};

#endif
