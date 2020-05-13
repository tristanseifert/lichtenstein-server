/**
 * Defines a packed 8-bit RGB pixel value.
 */
#ifndef RENDER_RGBWPIXEL_H
#define RENDER_RGBWPIXEL_H

#include <cstdint>
#include <cmath>

#include <nlohmann/json_fwd.hpp>

#include "HSIPixel.h"

namespace Lichtenstein::Server::Render {
    struct RGBWPixel {
        public:
            uint8_t r, g, b, w;

        public:
            inline RGBWPixel() {
                this->r = this->g = this->b = this->w = 0;
            }
            inline RGBWPixel(uint8_t r, uint8_t g, uint8_t b, uint8_t w) : 
                r(r), g(g), b(b), w(w) {  }
            inline RGBWPixel(const RGBWPixel &p) {
                this->r = p.r;
                this->g = p.g;
                this->b = p.b;
                this->w = p.w;
            }
            /**
             * Allow for implicit conversion from HSI type in framebuffer. See
             * http://blog.saikoled.com/post/44677718712 for details on the
             * algorithm used
             */
            inline RGBWPixel(const HSIPixel &p) {
                double cos_h, cos_1047_h;

                double H = p.h;
                double S = p.s;
                double I = p.i;

                // convert
                H = fmod(H, 360); // cycle H around to 0-360 degrees
                H = 3.14159 * H / 180.f; // Convert to radians.
                S = S > 0 ? (S < 1 ? S : 1) : 0; // clamp S and I to interval [0,1]
                I = I > 0 ? (I < 1 ? I : 1) : 0;

                if(H < 2.09439) {
                    cos_h = cos(H);
                    cos_1047_h = cos(1.047196667 - H);
                    this->r = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
                    this->g = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
                    this->b = 0;
                    this->w = 255 * (1 - S) * I;
                } else if(H < 4.188787) {
                    H = H - 2.09439;
                    cos_h = cos(H);
                    cos_1047_h = cos(1.047196667 - H);
                    this->g = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
                    this->b = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
                    this->r = 0;
                    this->w = 255 * (1 - S) * I;
                } else {
                    H = H - 4.188787;
                    cos_h = cos(H);
                    cos_1047_h = cos(1.047196667 - H);
                    this->b = S * 255 * I / 3 * (1 + cos_h / cos_1047_h);
                    this->r = S * 255 * I / 3 * (1 + (1 - cos_h / cos_1047_h));
                    this->g = 0;
                    this->w = 255 * (1 - S) * I;
                }
            }
            
            /// assignment operator
            inline RGBWPixel& operator=(const RGBWPixel& p) noexcept {
                // only copy if not self-assigning
                if(this != &p) {
                    this->r = p.r;
                    this->g = p.g;
                    this->b = p.b;
                    this->w = p.w;
                }

                return *this;
            }
            /// equality operator
            inline bool operator==(const RGBWPixel &rhs) const noexcept {
                return (this->r == rhs.r) && (this->g == rhs.g) && 
                    (this->b == rhs.b) && (this->w == rhs.w);
            }
    } __attribute__((packed));

    void from_json(const nlohmann::json &, RGBWPixel &);
    void to_json(nlohmann::json &, const RGBWPixel &);
}

#endif
