#include "HSIPixel.h"

#include <iostream>

using namespace std;

/**
 * Output an HSIPixel struct.
 */
ostream &operator<<(ostream& strm, const HSIPixel& obj) {
	strm << "{h = " << obj.h << ", s = " << obj.s << ", i = " << obj.i << "}";

	return strm;
}
