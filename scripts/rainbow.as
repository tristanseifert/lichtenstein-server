/**
 * Renders a moving rainbow. Two properties are available:
 *
 * - size: Determines how many times the rainbow "repeats". Default 1.
 * - speed: How many degrees the hue changes between frames. Default 1.
 */
void effectStep() {
	double width = double(buffer.length());
	double effectSize = double(properties['size']);

	double hAddend = 360 / (width / effectSize);

	double speed = double(properties['speed']);

	double offset = double(frameCounter) * speed;

	for(uint i = 0; i < buffer.length(); i++) {
		buffer[i].h = hAddend * (double(i) + offset);
		buffer[i].s = 1;
		buffer[i].i = 1;
	}
}
