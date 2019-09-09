//
// Created by Tristan Seifert on 2019-08-20.
//

#ifndef LICHTENSTEIN_SERVER_RT_API_H
#define LICHTENSTEIN_SERVER_RT_API_H

#include <atomic>
#include <thread>
#include <vector>

class DataStore;

namespace liblichtenstein::io {
  class DTLSServer;
}
namespace liblichtenstein::mdns {
  class Service;
}

namespace rt {
  class ClientHandler;

  /**
   * Implements the UDP-based real-time service. Clients connect to this service
   * and subscribe to one or more channels for which they wish to receive data.
   */
  class API {
    public:
      API(std::shared_ptr<DataStore> store);

      ~API();

    private:
      void createSocket();

      void createDTLSServer();

      void listenThread();

    private:
      std::shared_ptr<DataStore> store;

    private:
      // socket on which the API is listening
      int socket = -1;
      // DTLS server to handle the API
      std::unique_ptr<liblichtenstein::io::DTLSServer> tls;

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


#endif // LICHTENSTEIN_SERVER_RT_API_H
