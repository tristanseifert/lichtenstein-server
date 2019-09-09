//
// Created by Tristan Seifert on 2019-08-20.
//

#ifndef LICHTENSTEIN_SERVER_API_API_H
#define LICHTENSTEIN_SERVER_API_API_H

#include <atomic>
#include <thread>
#include <vector>

#include <uuid.h>

class DataStore;

namespace liblichtenstein::io {
  class TLSServer;
}
namespace liblichtenstein::mdns {
  class Service;
}

namespace api {
  class ClientHandler;

  /**
   * Implements the TCP-based server API.
   */
  class API {
    public:
      API(std::shared_ptr<DataStore> store);

      ~API();

    private:
      void createSocket();

      void createTLSServer();

      void listenThread();

    private:
      std::shared_ptr<DataStore> store;

    private:
      // server UUID
      uuids::uuid serverUuid;

      // socket on which the API is listening
      int socket = -1;
      // TLS server to handle the API
      std::unique_ptr<liblichtenstein::io::TLSServer> tls;

      // mDNS service used to advertise
      std::unique_ptr<liblichtenstein::mdns::Service> service;

      // should the background worker thread shut down?
      std::atomic_bool shutdown = false;
      // background worker thread used to wait and accept connections
      std::unique_ptr<std::thread> thread;

      // a list of clients we've accepted and their threads
      std::vector<std::shared_ptr<ClientHandler>> clients;
  };
}


#endif //LICHTENSTEIN_SERVER_API_API_H
