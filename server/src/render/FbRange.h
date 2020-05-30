/**
 * Represents a (start, length) range of framebuffer memory.
 */
#ifndef RENDER_FBRANGE_H
#define RENDER_FBRANGE_H

#include <cstddef>
#include <functional>

#include <Format.h>

namespace Lichtenstein::Server::Render {
    class FbRange {
        public:
            FbRange(size_t start, size_t length) : start(start), 
                length(length) {}

            size_t getOffset() const {
                return this->start;
            }
            size_t getLength() const {
                return this->length;
            }

            /**
             * Checks whether the two ranges intersect.
             */
            bool intersects(const FbRange &in) const {
                size_t x1 = this->start, y1 = (this->start + this->length);
                size_t x2 = in.start, y2 = (in.start + in.length);

                return (x1 >= y1 && x1 <= y2) || (x2 >= y1 && x2 <= y2) ||
                       (y1 >= x1 && y1 <= x2) || (y2 >= x1 && y2 <= x2);
            }

        public:
            bool operator==(const FbRange &rhs) const noexcept {
                return (this->start == rhs.start) &&
                    (this->length == rhs.length);
            }
            
            bool operator <(const FbRange &rhs) const {
                return (this->start < rhs.start);
            }

        private:
            size_t start;
            size_t length;

        friend struct std::hash<FbRange>;
        friend struct fmt::formatter<FbRange>;
    };

};

namespace std {
    using FbRange = Lichtenstein::Server::Render::FbRange;

    template <> struct hash<FbRange> {
        std::size_t operator()(const FbRange& k) const {
            using std::hash;

            return ((hash<size_t>()(k.start)) ^ (hash<size_t>()(k.length) << 1));
        }
    };
}

template <>
struct fmt::formatter<Lichtenstein::Server::Render::FbRange> {
  constexpr auto parse(format_parse_context& ctx) {
    auto it = ctx.begin(), end = ctx.end();
    if(it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const Lichtenstein::Server::Render::FbRange& r,
          FormatContext& ctx) {
    return format_to(ctx.out(), "FbRange({:d}, len: {:d})", r.start, r.length);
  }
};

#endif
