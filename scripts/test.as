/**
 * Test script: this fills the buffer with pixels of increasing intensity.
 */

void effectStep() {
	debug_print("hello world from test.as!");

	debug_print("buffer size: " + formatInt(bufferSz));
	debug_print("frame counter: " + formatInt(frameCounter));

	debug_print("test pixel: " + formatFloat(testPixel.h, '', 0, 2) + ", " + formatFloat(testPixel.s, '', 0, 2) + ", " + formatFloat(testPixel.i, '', 0, 2));

	debug_print("length of buffer array: " + formatInt(buffer.length()));

	double intensityStep = 1.0 / double(buffer.length() - 1);

	for(uint i = 0; i < buffer.length(); i++) {
		buffer[i].h = 0;
		buffer[i].s = 1;
		buffer[i].i = intensityStep * double(i);
	}
}
