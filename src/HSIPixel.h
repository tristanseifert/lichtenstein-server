/**
 * Defines the HSIPixel data type.
 */
#ifndef HSIPIXEL_H
#define HSIPIXEL_H

#include <iostream>

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

#endif
