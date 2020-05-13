/**
 * Provides the /nodes API path. This allows for callers to retrieve info about
 * nodes that this server controls.
 */
#ifndef API_CONTROLLERS_NODES_H
#define API_CONTROLLERS_NODES_H

#include <memory>

#include "../IController.h"

namespace Lichtenstein::Server::API::Controllers {
    class Nodes: public IController {
        public:
            Nodes(Server *srv);
            virtual ~Nodes() {};

        private:
            void getAll(const ReqType &, ResType &);
            
            void getOne(const ReqType &, ResType &);
            void update(const ReqType &, ResType &, const ReaderType &);

        private:
            static std::unique_ptr<IController> construct(Server *);
            static bool registered;
    };
}

#endif
