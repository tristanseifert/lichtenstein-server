/**
 * All effects output values in the HSI color space; this represents one such
 * pixel in the framebuffer.
 */
#ifndef RENDER_HSIPIXEL_H
#define RENDER_HSIPIXEL_H

#include <nlohmann/json_fwd.hpp>

namespace Lichtenstein::Server::Render {
    class HSIPixel {
        public:
            double h, s, i;

        public:
            inline HSIPixel() {
                this->h = this->s = this->i = 0.f;
            }
            inline HSIPixel(double h, double s, double i) : h(h), s(s), i(i) {}
            inline HSIPixel(const HSIPixel& p) {
                this->h = p.h;
                this->s = p.s;
                this->i = p.i;
            }

            /// assignment operator
            inline HSIPixel& operator=(const HSIPixel& p) noexcept {
                // only copy if not self-assigning
                if(this != &p) {
                    this->h = p.h;
                    this->s = p.s;
                    this->i = p.i;
                }

                return *this;
            }
            /// equality operator
            inline bool operator==(const HSIPixel &rhs) const noexcept {
                return (this->h == rhs.h) && (this->s == rhs.s) && 
                    (this->i == rhs.i);
            }
    };

    void from_json(const nlohmann::json &, HSIPixel &);
    void to_json(nlohmann::json &, const HSIPixel &);
}

#endif
