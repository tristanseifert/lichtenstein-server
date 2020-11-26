/**
 * Renders a field of twinkling stars. Several properties are available:
 *
 * - stars: Number of stars as a percentage: [0, 1]
 * - decayMin: Minimum speed of decay.
 * - decayMax: Maximum speed of decay.
 * - brightnessMin: Minimum value of initial brightness
 * - brightnessMax: Maximum value for initial brightness
 * - speed: How many frames to wait between each iteration.
 * - starHue: Hue of the stars
 * - starSaturation: Saturation of the stars
 *
 * Stars are placed at random locations in the output buffer, and each decay
 * at a random speed.
 */

// buffer for each star's decay value
array<int> heat;
uint heatSz = 0;

// return the larger of two numbers
int max(int x, int y) {
    if(x < y) {
        return y;
    } else {
        return x;
    }
}

void render() {
    // handle delay
    uint delay = max(uint(properties['speed']), 1);

    if((frame % delay) != 0) {
        return;
    }

    // calculate the number of stars
    uint width = numPixels;
    uint numStars = uint(double(width) * double(properties['stars']));

    // re-allocate heat buffer if needed
    if(heatSz != buffer.length()) {
        heat.resize(buffer.length());
        heatSz = buffer.length();

        // fill buffer with zeros
        for(uint i = 0; i < heat.length(); i++) {
            heat[i] = 0;
        }
    }


    // Determine how many nonzero heat values we have
    uint litStars = 0;

    for(uint i = 0; i < heatSz; i++) {
        if(heat[i] != 0) {
            litStars++;

            int decayMin = int(properties['decayMin']);
            int decayMax = int(properties['decayMax']);

            heat[i] = max(0, (heat[i] - random_range(decayMin, decayMax)));
        }
    }


    // If there's stars to be lit, light them.
    while(litStars <= numStars) {
        int i = random_range(0, (heatSz - 1));

        if(heat[i] == 0) {
            int brightnessMin = int(properties['brightnessMin']);
            int brightnessMax = int(properties['brightnessMax']);

            heat[i] = random_range(brightnessMin, brightnessMax);
            litStars++;
        }
    }

    // Generate an output from this
    for(uint x = 0; x < width; x++) {
        buffer[x].h = double(properties['starHue']);
        buffer[x].s = double(properties['starSaturation']);

        double brightnessMax = double(properties['brightnessMax']);
        buffer[x].i = double(heat[x]) / brightnessMax;
    }
}
