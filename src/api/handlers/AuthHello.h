//
// Created by Tristan Seifert on 2019-09-02.
//

#ifndef LICHTENSTEIN_SERVER_API_HANDLERS_AUTHHELLO_H
#define LICHTENSTEIN_SERVER_API_HANDLERS_AUTHHELLO_H

#include "../IRequestHandler.h"

#include <memory>

namespace api {
  class ClientHandler;
}


namespace api::handlers {
  class AuthHello : public IRequestHandler {
    public:
      explicit AuthHello(ClientHandler *client) : IRequestHandler(client) {}

    public:
      void handle(const lichtenstein::protocol::Message &received) override;

    private:
      static std::unique_ptr<IRequestHandler> construct(ClientHandler *client);

    private:
      static bool registered;
  };
}


#endif //LICHTENSTEIN_SERVER_API_HANDLERS_AUTHHELLO_H
