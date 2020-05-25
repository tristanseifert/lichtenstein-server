/**
 * Defines an abstract interface for an authentication handler. This is used by
 * the authentication state machine to support pluggable auth mechanisms.
 */
#include "db/DataStorePrimitives.h"

namespace Lichtenstein::Server::Proto::Auth {
    class IAuthHandler {
        using Node = DB::Types::Node;
        
        public:
            IAuthHandler() = delete;
            IAuthHandler(const Node &node) : node(node) {}

        protected:
            Node node;
    };
}
