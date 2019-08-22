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

#include <liblichtenstein/protocol/GenericClientHandler.h>

class DBChannel;

namespace liblichtenstein::io {
  class GenericServerClient;
}

namespace rt {
  class API;

  /**
   * Handles a "connection" from a node to the real-time API. This accepts any
   * requests the client makes as normally, and also sends data for any
   * channels that the client has subscribed for data updates.
   */
  class ClientHandler : public liblichtenstein::api::GenericClientHandler {
      using clientType = liblichtenstein::io::GenericServerClient;

    public:
      ClientHandler() = delete;

      ClientHandler(API *api, std::shared_ptr<clientType> client);

      ~ClientHandler() override;

      void close() override;

    private:
      void
      receivedData(const DBChannel &channel, const std::vector<std::byte> &data,
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
  };
}


#endif //LICHTENSTEIN_SERVER_RT_CLIENTHANDLER_H
