/**
 * This class handles the Lichtenstein protocol. It sets up the listening socket
 * and parses all requests it receives, including multicast node announcements.
 */
#ifndef LICHTENSTEIN_SERVER_PROTOCOL_PROTOCOLHANDLER_H
#define LICHTENSTEIN_SERVER_PROTOCOL_PROTOCOLHANDLER_H

#include <memory>

#include <uuid.h>

class DataStore;
class INIReader;

class DbNode;
class DbChannel;

namespace api {
  class API;
}
namespace rt {
  class API;
}


namespace protocol {
  class AdoptionHandler;

  class ProtocolHandler {
    public:
      ProtocolHandler(std::shared_ptr<DataStore> store,
                      std::shared_ptr<INIReader> reader);

      ~ProtocolHandler();

    public:
      void sendPixelData(DbChannel *channel, void *data, size_t numPixels,
                         bool isRGBW);

      void sendOutputEnableForAllNodes(void);

      void prepareForShutDown(void);

    public:
      uuids::uuid getServerUuid() const {
        return this->uuid;
      }


    private:
      std::shared_ptr<DataStore> store;
      std::shared_ptr<INIReader> config;

      // server UUID
      uuids::uuid uuid;

      // TCP-based control API (used by nodes and other clients)
      std::unique_ptr<api::API> serverApi;
      // UDP-based real-time data API (used for pixel data)
      std::unique_ptr<rt::API> rtApi;

      // adoption handler
      std::unique_ptr<protocol::AdoptionHandler> orphanage;
  };
}

#endif
