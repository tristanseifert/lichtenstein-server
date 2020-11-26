/**
 * A simple breathing effect, akin to the power light on Apple computers in
 * sleep mode. The effect can be customized with several properties:
 *
 * - stepSize: How fast the effect runs. A good starting point is 0.01.
 * - maxIntensity: The maximum value assigned to intensity.
 * - hue: Hue of the pixels.
 * - saturation: Saturation of the pixels.
 */
double step = 0;

void render() {
    double stepSize = double(properties['stepSize']);
    double maxIntensity = double(properties['maxIntensity']);
    double hue = double(properties['hue']);
    double saturation = double(properties['saturation']);

    for(uint x = 0; x < numPixels; x++) {
        buffer[x].h = hue;
        buffer[x].s = saturation;

        buffer[x].i = (1 - abs(sin(step))) * maxIntensity;
    }

    step = stepSize * double(frame);
}
