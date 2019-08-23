//
// Created by Tristan Seifert on 2019-08-20.
//

#ifndef LICHTENSTEIN_SERVER_RT_CLIENTHANDLER_H
#define LICHTENSTEIN_SERVER_RT_CLIENTHANDLER_H

#include "PixelDataHandler.h"

#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <random>

#include <liblichtenstein/protocol/GenericClientHandler.h>

class DbChannel;

namespace liblichtenstein::io {
  class GenericServerClient;
}

namespace rt::handlers {
  class JoinChannel;
}

namespace rt {
  class API;

  /**
   * Handles a "connection" from a node to the real-time API. This accepts any
   * requests the client makes as normally, and also sends data for any
   * channels that the client has subscribed for data updates.
   */
  class ClientHandler : public liblichtenstein::api::GenericClientHandler {
      friend class handlers::JoinChannel;

      using clientType = liblichtenstein::io::GenericServerClient;

    public:
      ClientHandler() = delete;

      ClientHandler(API *api, std::shared_ptr<clientType> client);

      ~ClientHandler() override;

      void close() override;

    private:
      void
      receivedData(const DbChannel &channel, const std::vector<std::byte> &data,
                   PixelDataHandler::PixelFormat format);

    private:
      void handle();

      void processMessage(protoMessageType &received);

    private:
      void registerPixelDataCallback(int token);

    private:
      // API that this client connected to
      API *api = nullptr;

      // worker thread
      std::unique_ptr<std::thread> thread;

      // mutex protecting the pixel data callbacks structure
      std::mutex pixelDataCallbacksLock;
      // IDs of any callbacks added (for pixel data)
      std::vector<int> pixelDataCallbacks;

      // used to generate random numbers
      std::mt19937 random = std::mt19937(std::random_device()());
  };
}


#endif //LICHTENSTEIN_SERVER_RT_CLIENTHANDLER_H
