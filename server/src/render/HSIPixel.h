/**
 * All effects output values in the HSI color space; this represents one such
 * pixel in the framebuffer.
 */
#ifndef RENDER_HSIPIXEL_H
#define RENDER_HSIPIXEL_H

#include <Format.h>

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

template <>
struct fmt::formatter<Lichtenstein::Server::Render::HSIPixel> {
  constexpr auto parse(format_parse_context& ctx) {
    // Parse the presentation format and store it in the formatter:
    auto it = ctx.begin(), end = ctx.end();

    // Check if reached the end of the range:
    if (it != end && *it != '}')
      throw format_error("invalid format");

    // Return an iterator past the end of the parsed range:
    return it;
  }

  template <typename FormatContext>
  auto format(const Lichtenstein::Server::Render::HSIPixel& p,
          FormatContext& ctx) {
    return format_to(
        ctx.out(),
        "({:.5g}, {:.4g}, {:.4g})",
        p.h, p.s, p.i);
  }
};

#endif
