/**
 * A movie-theatre style ticker effect, where half every other pixel is always
 * on, and the offset of pixels alternates every frame.
 *
 * Several properties are available:
 *
 * - speed: How many frames to wait between alternating.
 * - hue: Hue of the pixels
 * - saturation: Saturation of the pixels
 */
uint timer = 0;
uint frame = 0;

void effectStep() {
	uint speed = uint(properties['speed']);

	double hue = double(properties['hue']);
	double saturation = double(properties['saturation']);

	timer++;

	if(timer >= speed) {
		frame++;
		timer = 0;
	}

	for(uint x = 0; x < buffer.length(); x++) {
		buffer[x].h = hue;
		buffer[x].s = saturation;

		if(((x & 1) == 1) && ((frame & 1) == 1)) {
			buffer[x].i = 1;
		} else if(((x & 1) == 0) && ((frame & 1) == 1)) {
			buffer[x].i = 0;
		} else if(((x & 1) == 1) && ((frame & 1) == 0)) {
			buffer[x].i = 0;
		} else if(((x & 1) == 0) && ((frame & 1) == 0)) {
			buffer[x].i = 1;
		}
	}
}
