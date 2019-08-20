//
// Created by Tristan Seifert on 2019-08-20.
//
#include "API.h"
#include "ClientHandler.h"

#include "../version.h"

#include <glog/logging.h>
#include <INIReader.h>

#include <liblichtenstein/io/TLSServer.h>
#include <liblichtenstein/io/OpenSSLError.h>
#include <liblichtenstein/io/mdns/Service.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// alias some commonly used classes
using ZeroconfService = liblichtenstein::mdns::Service;

using SSLError = liblichtenstein::io::OpenSSLError;
using TLSServer = liblichtenstein::io::TLSServer;


namespace api {
  /**
   * Sets up the server API.
   *
   * @param db Data store to use for data storage
   * @param ini App configuration
   */
  API::API(DataStore *db, INIReader *ini) : store(db), config(ini) {
    // create the listening socket and TLS handler
    this->createSocket();
    this->createTLSServer();

    // create the worker thread
    this->thread = std::make_unique<std::thread>(&API::listenThread, this);
  }

  /**
   * Shuts down the API server.
   */
  API::~API() {
    int err;

    // mark to the API to terminate
    this->shutdown = true;

    // close the listening socket and join that thread
    if(this->socket != -1) {
      err = close(this->socket);
      PLOG_IF(ERROR, err != 0) << "close() on API socket failed";
    }

    if(this->thread) {
      if(this->thread->joinable()) {
        this->thread->join();
      }

      this->thread = nullptr;
    }
  }


  /**
   * Opens the listening socket.
   */
  void API::createSocket() {
    int err;
    struct sockaddr_in servaddr{};

    int on = 1;

    // get params from config
    const std::string listenAddress = this->config->Get("api", "listen",
                                                        "0.0.0.0");
    const unsigned int listenPort = this->config->GetInteger("api", "port",
                                                             45678);

    VLOG(1) << "API server starting on " << listenAddress << ":" << listenPort;

    // parse the address
    servaddr.sin_family = AF_INET;
    err = inet_pton(servaddr.sin_family, listenAddress.c_str(),
                    &servaddr.sin_addr);

    if(err != 1) {
      // try to parse it as IPv6
      servaddr.sin_family = AF_INET6;
      err = inet_pton(servaddr.sin_family, listenAddress.c_str(),
                      &servaddr.sin_addr);

      if(err != 1) {
        // give up and just listen on INADDR_ANY
        memset(&servaddr.sin_addr, 0, sizeof(servaddr.sin_addr));
        servaddr.sin_addr.s_addr = INADDR_ANY;

        LOG(WARNING) << "Couldn't parse listen address '" << listenAddress
                     << "', listening on all interfaces instead";
      }
    }

    // create listening socket
    this->socket = ::socket(servaddr.sin_family, SOCK_STREAM, 0);
    PCHECK(this->socket > 0) << "socket() failed";

    servaddr.sin_port = htons(listenPort);

    // bind to the given address
    err = bind(this->socket, (const struct sockaddr *) &servaddr,
               sizeof(servaddr));
    PCHECK(err >= 0) << "bind() failed";

    // allow address reuse
    setsockopt(this->socket, SOL_SOCKET, SO_REUSEADDR, (const void *) &on,
               (socklen_t) sizeof(on));
#if defined(SO_REUSEPORT) && !defined(__linux__)
    setsockopt(this->socket, SOL_SOCKET, SO_REUSEPORT, (const void *) &on,
               (socklen_t) sizeof(on));
#endif

    // the socket has been created! listen for connections
    const int backlog = (int) this->config->GetInteger("api", "backlog", 10);

    err = listen(this->socket, backlog);
    PCHECK(err == 0) << "listen() failed";
  }

  /**
   * Once the listening socket has been created, we can create the TLS server
   * to work to that socket.
   */
  void API::createTLSServer() {
    // create the server
    this->tls = std::make_unique<TLSServer>(this->socket);

    // load certificate
    const std::string certPath = this->config->Get("api", "certPath", "");
    const std::string certKeyPath = this->config->Get("api", "certKeyPath", "");

    this->tls->loadCert(certPath, certKeyPath);
  }


  /**
   * Listens for incoming connections.
   */
  void API::listenThread() {
    // start advertising via mDNS
    VLOG(1) << "Beginning mDNS advertisement";

    const unsigned int listenPort = this->config->GetInteger("api", "port",
                                                             45678);
    this->service = ZeroconfService::create("_licht._tcp.", listenPort);

    if(this->service) {
      this->service->startAdvertising();
      this->service->setTxtRecord("vers", std::string(gVERSION));
      this->service->setTxtRecord("typ", "server");
    }

    // API server main loop
    while(!this->shutdown) {
      try {
        // try to get a client
        auto client = this->tls->run();

        // instantiate a handler and add it to our list
        auto *handler = new ClientHandler(this, client);
        this->clients.emplace_back().reset(handler);
      } catch(SSLError &e) {
        LOG(ERROR) << "TLS error accepting client: " << e.what();
      } catch(std::system_error &e) {
        // if it's "connection aborted", ignore it
        if(e.code().value() == ECONNABORTED) {
          VLOG(1) << "Listening socket was closed";
        } else {
          LOG(ERROR) << "System error accepting client: " << e.what();
        }
      } catch(std::runtime_error &e) {
        LOG(ERROR) << "Runtime error accepting client: " << e.what();
      } catch(std::exception &e) {
        LOG(FATAL) << "Unexpected error accepting client" << e.what();
      }
    }

    VLOG(1) << "Server API is shutting down";

    // stop advertising the service
    if(this->service) {
      this->service->stopAdvertising();
    }

    // close all clients
    this->clients.clear();

    // delete the API server
    this->tls = nullptr;

    // close socket (XXX: state machine does this for us)
    // close(this->socket);
    // this->socket = -1;
  }
}