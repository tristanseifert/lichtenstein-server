/**
 * Provides the /channels API path. This allows for callers to retrieve info
 * about the channels of each node that this server controls. Additionally, it
 * can be used to configure the framebuffer offset of each of these channels.
 */
#ifndef API_CONTROLLERS_NODES_H
#define API_CONTROLLERS_NODES_H

#include <memory>

#include "../IController.h"

namespace Lichtenstein::Server::API::Controllers {
    class NodeChannels: public IController {
        public:
            NodeChannels(Server *srv);
            virtual ~NodeChannels() {};

        private:
            void getAll(const ReqType &, ResType &);
            void getAllForNode(const ReqType &, ResType &);
            
            void getOne(const ReqType &, ResType &);
            void update(const ReqType &, ResType &, const ReaderType &);

        private:
            static std::unique_ptr<IController> construct(Server *);
            static bool registered;
    };
}

#endif
