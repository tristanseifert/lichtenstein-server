/**
 * Implements a thin wrapper around a region of memory – the framebuffer – into
 * which all effects write their data.
 *
 * This framebuffer can be resized at runtime to accomodate for changes in the
 * grouping configuration, and is safe for concurrent access by multiple threads
 * so long as no two threads attempt to WRITE to the same region of the buffer.
 *
 * Internally, the framebuffer stores each pixel as a HSI tuple. The index of
 * the H component is 0, S is at 1, and I is at 2.
 */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <vector>
#include <iostream>

#include "INIReader.h"

#include "DataStore.h"

struct HSIPixel {
	public:
		double h = 0;
		double s = 0;
		double i = 0;

		inline HSIPixel() {}
		inline HSIPixel(double h, double s, double i) : h(h), s(s), i(i) { }
		inline HSIPixel(const HSIPixel& p) {
			this->h = p.h;
			this->s = p.s;
			this->i = p.i;
		}
		inline HSIPixel& operator=(const HSIPixel& other) noexcept {
			// check for self assignment
			if(this != &other) {
				this->h = other.h;
				this->s = other.s;
				this->i = other.i;
			}

			return *this;
		}
		inline bool operator==(const HSIPixel &rhs) const noexcept {
			return (this->h == rhs.h) && (this->s == rhs.s) && (this->i == rhs.i);
		}
};
std::ostream &operator<<(std::ostream& strm, const HSIPixel& obj);

class Framebuffer {
	public:
		Framebuffer(DataStore *store, INIReader *reader);
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
		DataStore *store;
		INIReader *config;

		std::vector<HSIPixel> data;
};

#endif
