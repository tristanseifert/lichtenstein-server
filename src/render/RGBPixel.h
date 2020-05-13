/**
 * Defines a packed 8-bit RGB pixel value.
 */
#ifndef RENDER_RGBPIXEL_H
#define RENDER_RGBPIXEL_H

#include <cstdint>
#include <cmath>

#include <nlohmann/json_fwd.hpp>

#include "HSIPixel.h"

namespace Lichtenstein::Server::Render {
    struct RGBPixel {
        public:
            uint8_t r, g, b;

        public:
            inline RGBPixel() {
                this->r = this->g = this->b = 0;
            }
            inline RGBPixel(uint8_t r, uint8_t g, uint8_t b) : r(r),g(g),b(b) {}
            inline RGBPixel(const RGBPixel &p) {
                this->r = p.r;
                this->g = p.g;
                this->b = p.b;
            }
            /**
             * Allow for implicit conversion from HSI type in framebuffer. See
             * http://blog.saikoled.com/post/43693602826 for details on the
             * algorithm used
             */
            inline RGBPixel(const HSIPixel &p) {
                double H = p.h;
                double S = p.s;
                double I = p.i;

                // convert
                H = fmod(H, 360); // cycle H around to 0-360 degrees
                H = 3.14159 * H / 180.f; // Convert to radians.
                S = S > 0 ? (S < 1 ? S : 1) : 0; // clamp S and I to interval [0,1]
                I = I > 0 ? (I < 1 ? I : 1) : 0;

                // Math! Thanks in part to Kyle Miller.
                if(H < 2.09439) {
                    this->r = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
                    this->g = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
                    this->b = 255 * I / 3 * (1 - S);
                } else if(H < 4.188787) {
                    H = H - 2.09439;
                    this->g = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
                    this->b = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
                    this->r = 255 * I / 3 * (1 - S);
                } else {
                    H = H - 4.188787;
                    this->b = 255 * I / 3 * (1 + S * cos(H) / cos(1.047196667 - H));
                    this->r = 255 * I / 3 * (1 + S * (1 - cos(H) / cos(1.047196667 - H)));
                    this->g = 255 * I / 3 * (1 - S);
                }
            }
            
            /// assignment operator
            inline RGBPixel& operator=(const RGBPixel& p) noexcept {
                // only copy if not self-assigning
                if(this != &p) {
                    this->r = p.r;
                    this->g = p.g;
                    this->b = p.b;
                }

                return *this;
            }
            /// equality operator
            inline bool operator==(const RGBPixel &rhs) const noexcept {
                return (this->r == rhs.r) && (this->g == rhs.g) && 
                    (this->b == rhs.b);
            }
    } __attribute__((packed));

    void from_json(const nlohmann::json &, RGBPixel &);
    void to_json(nlohmann::json &, const RGBPixel &);
}

#endif
