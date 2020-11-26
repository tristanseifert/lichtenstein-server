/**
 * A fire effect, where flames are sparked starting at the bottom, and slowly
 * rise up.
 *
 * The effect can be customized with the following properties:
 *
 * - delay: How many frames to skip between updates.
 * - cooling: How 'quickly' pixels cool down. 50 is a good starting point.
 * - sparking: How likely a spark is created on each iteration. 142 is a good
 *                         default. Higher values mean more sparks.
 * - ignitionRange: How many of the bottom pixels are considered candidates
 *                                      for a new spark. Roughly 7% of the pixels is a good number.
 */
double min(double x, double y) {
    if(x > y) {
        return y;
    } else {
        return x;
    }
}

/**
 * Converts the temperature of a pixel to a color value.
 */
HSIPixel getHeatColor(uint8 temperature) {
    HSIPixel pixel;

    const float initialHue = 0;

    // Between 0x00 and 0x40, scale intensity from 0 to 1 with a hue of 0
    if(temperature <= 0x40) {
        pixel.h = initialHue;
        pixel.s = 1;

        pixel.i = min(0.95, (double(temperature)) / 64.f);
    }
    // Between 0x40 and 0x80, scale hue from 0 to 40
    if(temperature > 0x40 && temperature <= 0x80) {
        pixel.h = initialHue + ((double(temperature - 0x40)) / 1.6f);

        pixel.s = 1;
        pixel.i = 0.95;
    }
    // Between 0x80 and 0xFF, scale intensity from 1 to 0
    if(temperature > 0x80) {
        pixel.h = initialHue + 40;
        pixel.s = min(double(0), 1 - ((double(temperature - 0x80)) / 80.f));
        pixel.i = 0.95;
    }

    return pixel;
}

array<uint8> heat;
uint heatSz = 0;

void render() {
    uint delay = uint(properties['delay']);

    uint8 cooling = uint8(properties['cooling']);
    uint8 sparking = uint8(properties['sparking']);
    uint ignitionRange = uint8(properties['ignitionRange']);

    debug_print("frame counter: " + formatInt(frame));
    debug_print("frame delay: " + formatInt(delay));

    uint width = buffer.length();

    // handle delay
    if((frame % delay) != 0) {
        return;
    }

    // re-allocate heat buffer if needed
    if(heatSz != buffer.length()) {
        heat.resize(buffer.length());
        heatSz = buffer.length();

        for(uint i = 0; i < heat.length(); i++) {
            heat[i] = 0;
        }

        debug_print("allocated new heat buffer");
    }

    uint cooldown = 0;

    // step 1: cool down every cell a little
    for(uint i = 0; i < width; i++) {
        cooldown = random_range(0, ((cooling * 10) / width) + 2);

        if(cooldown > heat[i]) {
            heat[i] = 0;
        } else {
            heat[i] = (heat[i] - cooldown);
        }
    }

    // step 2: heat from each cell drifts up and diffuses a little
    for(uint k = (width - 1); k >= 2; k--) {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' near the bottom
    if(uint(random_range(0, 255)) < sparking) {
        int y = random_range(0, ignitionRange);
        heat[y] = heat[y] + random_range(160, 255);
    }

    // convert each pixel to a color
    for(uint j = 0; j < width; j++) {
        buffer[j] = getHeatColor(heat[j]);
    }
}
