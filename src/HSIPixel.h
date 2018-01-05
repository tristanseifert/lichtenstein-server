/**
 * Defines the HSIPixel data type.
 */
#ifndef HSIPIXEL_H
#define HSIPIXEL_H

#include <iostream>

class HSIPixel {
	public:
		double h = 0;
		double s = 0;
		double i = 0;

	public:
		inline HSIPixel() {}
		inline HSIPixel(double h, double s, double i) : h(h), s(s), i(i) { }
		inline HSIPixel(const HSIPixel& p) {
			this->h = p.h;
			this->s = p.s;
			this->i = p.i;
		}

	public:
		// TODO: figure out if dereferencing causes performance issues
		inline void convertToRGB(uint8_t *out) const {
			HSIPixel::convertPixelToRGB(*this, out);
		}
		inline void convertToRGBW(uint8_t *out) const {
			HSIPixel::convertPixelToRGBW(*this, out);
		}

	public:
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

	public:
		static void convertPixelToRGB(const HSIPixel &in, uint8_t *out);
		static void convertPixelToRGBW(const HSIPixel &in, uint8_t *out);
};
std::ostream &operator<<(std::ostream& strm, const HSIPixel& obj);

#endif
