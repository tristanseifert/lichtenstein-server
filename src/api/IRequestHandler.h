//
// Created by Tristan Seifert on 2019-08-20.
//

#ifndef LICHTENSTEIN_SERVER_IREQUESTHANDLER_H
#define LICHTENSTEIN_SERVER_IREQUESTHANDLER_H

namespace lichtenstein::protocol {
  class Message;
}

namespace api {
  class ClientHandler;

  /**
   * Provides the interface implemented by all request handlers.
   */
  class IRequestHandler {
    public:
      IRequestHandler() = delete;

      explicit IRequestHandler(ClientHandler *client) : client(client) {}

      virtual ~IRequestHandler() = default;

    public:
      virtual void handle(const lichtenstein::protocol::Message &received) = 0;

    protected:
      // client handler that received this request
      ClientHandler *client = nullptr;
  };
}
#endif //LICHTENSTEIN_SERVER_IREQUESTHANDLER_H
