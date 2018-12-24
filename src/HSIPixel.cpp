#include "HSIPixel.h"

#include <iostream>
#include <cmath>

/**
 * Output an HSIPixel struct.
 */
std::ostream &operator<<(std::ostream& strm, const HSIPixel& obj) {
	strm << "{h = " << obj.h << ", s = " << obj.s << ", i = " << obj.i << "}";

	return strm;
}

/**
 * Converts a given HSI pixel to RGB.
 *
 * Data is placed in the output buffer as three bytes in RGB order.
 *
 * More info on this conversion algorithm can be found at
 * http://blog.saikoled.com/post/43693602826
 */
void HSIPixel::convertPixelToRGB(const HSIPixel &in, uint8_t *out) {
	uint8_t r, g, b;

	// get input values
	double H = in.h;
	double S = in.s;
	double I = in.i;

	// convert
	H = fmod(H, 360); // cycle H around to 0-360 degrees
	H = 3.14159 * H / 180.f; // Convert to radians.
	S = S > 0 ? (S < 1 ? S : 1) : 0; // clamp S and I to interval [0,1]
	I = I > 0 ? (I < 1 ? I : 1) : 0;

	// Math! Thanks in part to Kyle Miller.
	if(H < 2.09439) {
		r = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
		g = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
		b = 255 * I / 3 * (1 - S);
	} else if(H < 4.188787) {
		H = H - 2.09439;
		g = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
		b = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
		r = 255 * I / 3 * (1 - S);
	} else {
		H = H - 4.188787;
		b = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
		r = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
		g = 255 * I / 3 * (1 - S);
	}

	// copy it to the buffer
	out[0] = r;
	out[1] = g;
	out[2] = b;
}

/**
 * Converts a given HSI pixel to RGBW.
 *
 * Data is placed in the output buffer as four bytes in RGBW order.
 *
 * More info on this conversion algorithm can be found at
 * http://blog.saikoled.com/post/44677718712
 */
void HSIPixel::convertPixelToRGBW(const HSIPixel &in, uint8_t *out) {
	uint8_t r, g, b, w;
	double cos_h, cos_1047_h;

	// get input values
	double H = in.h;
	double S = in.s;
	double I = in.i;

	// convert
	H = fmod(H, 360); // cycle H around to 0-360 degrees
	H = 3.14159 * H / 180.f; // Convert to radians.
	S = S > 0 ? (S < 1 ? S : 1) : 0; // clamp S and I to interval [0,1]
	I = I > 0 ? (I < 1 ? I : 1) : 0;

	if(H < 2.09439) {
		cos_h = cos(H);
		cos_1047_h = cos(1.047196667 - H);
		r = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
		g = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
		b = 0;
		w = 255 * (1 - S) * I;
	} else if(H < 4.188787) {
		H = H - 2.09439;
		cos_h = cos(H);
		cos_1047_h = cos(1.047196667 - H);
		g = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
		b = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
		r = 0;
		w = 255 * (1 - S) * I;
	} else {
		H = H - 4.188787;
		cos_h = cos(H);
		cos_1047_h = cos(1.047196667 - H);
		b = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
		r = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
		g = 0;
		w = 255 * (1 - S) * I;
	}

	// copy it to the buffer
	out[0] = r;
	out[1] = g;
	out[2] = b;
	out[3] = w;
}
