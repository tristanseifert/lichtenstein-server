//
// Created by Tristan Seifert on 2019-08-20.
//

#ifndef LICHTENSTEIN_SERVER_CLIENTHANDLER_H
#define LICHTENSTEIN_SERVER_CLIENTHANDLER_H

#include <memory>
#include <atomic>
#include <thread>

#include <liblichtenstein/protocol/GenericClientHandler.h>

namespace liblichtenstein::io {
  class GenericServerClient;
}

namespace api {
  class API;

  /**
   * An instance is created for every client that connects to the server API.
   */
  class ClientHandler : public liblichtenstein::api::GenericClientHandler {
      using clientType = liblichtenstein::io::GenericServerClient;

    public:
      ClientHandler() = delete;

      ClientHandler(API *api, std::shared_ptr<clientType> client);

      ~ClientHandler() override;

    private:
      void handle();

      void processMessage(protoMessageType &received);

    private:
      // API that this client connected to
      API *api = nullptr;

      // should worker thread shut down?
      std::atomic_bool shutdown = false;
      // worker thread
      std::unique_ptr<std::thread> thread;
  };
}

#endif //LICHTENSTEIN_SERVER_CLIENTHANDLER_H
