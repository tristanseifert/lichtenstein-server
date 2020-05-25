/**
 * Allows manipulation of the pipeline state via the /pipeline endpoint.
 * Currently, there are only API calls to set one or more groups' outputs to
 * either a solid fill, or a routine with params.
 */
#ifndef API_CONTROLLERS_PIPELINE_H
#define API_CONTROLLERS_PIPELINE_H

#include <memory>

#include <nlohmann/json_fwd.hpp>

#include "../IController.h"

namespace Lichtenstein::Server::Render {
    class IRenderable;
    class IRenderTarget;
}

namespace Lichtenstein::Server::API {
    class Server;
}

namespace Lichtenstein::Server::API::Controllers {
    class PipelineController: public IController {
        public:
            PipelineController(Server *srv);

        private:
            void getState(const ReqType &, ResType &);
            void setState(const ReqType &, ResType &, const ReaderType &);
            void setBrightness(const ReqType &, ResType &, const ReaderType &);

        private:
            using RenderablePtr = std::shared_ptr<Render::IRenderable>;
            using TargetPtr = std::shared_ptr<Render::IRenderTarget>;

            RenderablePtr makeRenderableFill(size_t, const nlohmann::json &);
            RenderablePtr makeRenderableRoutine(size_t, const nlohmann::json &);

            TargetPtr makeGroupTarget(const nlohmann::json &);

        private:
            static std::unique_ptr<IController> construct(Server *);
            static bool registered;
        
    };
}

#endif
