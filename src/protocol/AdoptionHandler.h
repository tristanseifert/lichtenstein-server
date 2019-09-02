//
// Created by Tristan Seifert on 2019-09-02.
//
#ifndef LICHTENSTEIN_SERVER_PROTOCOL_ADOPTIONHANDLER_H
#define LICHTENSTEIN_SERVER_PROTOCOL_ADOPTIONHANDLER_H

#include <memory>
#include <string>

#include <uuid.h>

#include <liblichtenstein/io/TLSClient.h>
#include <liblichtenstein/protocol/MessageIO.h>

class INIReader;

class DataStore;

namespace protocol {
  class ProtocolHandler;

  /**
   * Adoption of nodes is implemented through this class.
   */
  class AdoptionHandler {
      using IORef = std::shared_ptr<liblichtenstein::api::MessageIO>;

    private:
      // how many bytes of "secret" to generate when adopting nodes
      static const size_t kSecretLength = 32;

    public:
      AdoptionHandler(std::shared_ptr<DataStore> store,
                      std::shared_ptr<INIReader> reader,
                      ProtocolHandler *proto);

      ~AdoptionHandler();

    public:
      void attemptAdoption(const std::string &host, const unsigned int port);

    private:
      void verifyAdoptionState(IORef io, uuids::uuid *outUuid);

      void sendAdoptReq(IORef io, const uuids::uuid &nodeUuid,
                        const std::vector<std::byte> &secret);

      void waitAdoptAck(IORef io, const uuids::uuid &nodeUuid);

    private:
      void getApiHosts();

      void handleError(const lichtenstein::protocol::Message &message);

    private:
      std::shared_ptr<DataStore> store;
      std::shared_ptr<INIReader> config;

      // protocol handler we're owned by
      ProtocolHandler *handler;

      // hostname for API
      std::string apiHost;
      // port for API
      unsigned int apiPort = 0;

      // hostname for realtime service
      std::string rtHost;
      // port for realtime service
      unsigned int rtPort = 0;
  };
}


#endif //LICHTENSTEIN_SERVER_PROTOCOL_ADOPTIONHANDLER_H
