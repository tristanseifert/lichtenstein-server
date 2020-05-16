/**
 * Provides the /group API path. This allows for fetching, updating, creating
 * and deleting of groups.
 */
#ifndef API_CONTROLLERS_GROUPS_H
#define API_CONTROLLERS_GROUPS_H

#include <memory>

#include "../IController.h"

namespace Lichtenstein::Server::API::Controllers {
    class Groups: public IController {
        public:
            Groups(Server *srv);
            virtual ~Groups() {};

        private:
            void getAll(const ReqType &, ResType &);
            void create(const ReqType &, ResType &, const ReaderType &);
            
            void getOne(const ReqType &, ResType &);
            void remove(const ReqType &q, ResType &);
            void update(const ReqType &, ResType &, const ReaderType &);

        private:
            static std::unique_ptr<IController> construct(Server *);
            static bool registered;
    };
}

#endif
