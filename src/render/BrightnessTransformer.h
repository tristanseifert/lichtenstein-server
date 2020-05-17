/**
 * Transforms pixel data by multiplying intensity by a given factor.
 */
#ifndef RENDER_BRIGHTNESSTRANSFORMER_H
#define RENDER_BRIGHTNESSTRANSFORMER_H

#include "IPixelTransformer.h"

namespace Lichtenstein::Server::Render {
    class BrightnessTransformer: public IPixelTransformer {
        public:
            BrightnessTransformer() = delete;
            BrightnessTransformer(const double factor) : factor(factor) {}

        public:
            void transform(std::shared_ptr<Framebuffer>, const FbRange &);

            /**
             * Returns the current brightness multiplication factor.
             */
            double getFactor() const {
                return this->factor;
            }

            /**
             * Sets the brightness multiplication factor.
             */
            void setFactor(const double newFactor) {
                this->factor = newFactor;
            }

        private:
            double factor = 1.f;
    };
}

#endif
