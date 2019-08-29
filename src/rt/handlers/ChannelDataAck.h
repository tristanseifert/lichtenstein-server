//
// Created by Tristan Seifert on 2019-08-23.
//

#ifndef LICHTENSTEIN_SERVER_RT_HANDLERS_CHANNELDATAACK_H
#define LICHTENSTEIN_SERVER_RT_HANDLERS_CHANNELDATAACK_H

#include "../IRequestHandler.h"

#include <memory>

namespace rt {
  class ClientHandler;
}

namespace rt::handlers {
  class ChannelDataAck : public IRequestHandler {
    public:
      explicit ChannelDataAck(ClientHandler *client) : IRequestHandler(
              client) {}

    public:
      void handle(const lichtenstein::protocol::Message &received) override;

    private:
      static std::unique_ptr<IRequestHandler>
      construct(ClientHandler *client);

    private:
      static bool registered;
  };
}

#endif //LICHTENSTEIN_SERVER_RT_HANDLERS_CHANNELDATAACK_H
