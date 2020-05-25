/**
 * Provides the /routine API path. This allows for fetching, updating, creating
 * and deleting of routines.
 */
#ifndef API_CONTROLLERS_ROUTINES_H
#define API_CONTROLLERS_ROUTINES_H

#include <memory>

#include "../IController.h"

namespace Lichtenstein::Server::API::Controllers {
    class Routines: public IController {
        public:
            Routines(Server *srv);
            virtual ~Routines() {};

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
