//
// Created by Tristan Seifert on 2019-08-20.
//
#include "ClientHandler.h"
#include "HandlerFactory.h"
#include "IRequestHandler.h"

#include <glog/logging.h>

#include <liblichtenstein/io/GenericServerClient.h>
#include <liblichtenstein/io/OpenSSLError.h>
#include <liblichtenstein/io/SSLSessionClosedError.h>
#include <liblichtenstein/protocol/ProtocolError.h>

#include "proto/shared/Message.pb.h"

// alias some commonly used classes
using SSLError = liblichtenstein::io::OpenSSLError;
using SSLSessionClosedError = liblichtenstein::io::SSLSessionClosedError;
using ProtocolError = liblichtenstein::api::ProtocolError;

namespace rt {
  /**
   * Creates a new client handler. This creates a thread to read requests from the
   * client and act on them.
   *
   * @param api The API to which this client connected
   * @param client Client instance
   */
  ClientHandler::ClientHandler(API *api, std::shared_ptr<clientType> client)
          : GenericClientHandler(client), api(api) {
    // create worker thread
    this->thread = std::make_unique<std::thread>(&ClientHandler::handle, this);
  }

  /**
   * Cleans up the client handler.
   */
  ClientHandler::~ClientHandler() {
    this->shutdown = true;

    // wait for thread to join and delete it
    if(this->thread->joinable()) {
      this->thread->join();
    }
  }


  /**
   * Worker thread entry point; this continuously attempts to read messages
   * from the client and process them.
   */
  void ClientHandler::handle() {
    VLOG(1) << "Got new client: " << this->client;

    // service requests as long as the API is running
    while(!this->shutdown) {
      // try to read from the client
      try {
        this->readMessage([this](protoMessageType &message) {
          this->processMessage(message);
        });
      }
        // an error in the TLS library happened
      catch(SSLError &e) {
        // ignore TLS errors if we're shutting down
        if(!this->shutdown) {
          LOG(ERROR) << "TLS error reading from client: " << e.what();
        }
      }
        // if we get this exception, session was closed
      catch(SSLSessionClosedError &e) {
        VLOG(1) << "Connection was closed: " << e.what();
        break;
      }
        // an error decoding message
      catch(ProtocolError &e) {
        LOG(ERROR) << "Protocol error, closing connection: " << e.what();
        break;
      }
        // some other runtime error happened
      catch(std::runtime_error &e) {
        LOG(ERROR) << "Runtime error reading from client: " << e.what();
        break;
      }
    }

    // clean up client
    VLOG(1) << "Shutting down API client for client " << this->client;
    this->client->close();
  }

  /**
   * Processes a received message. Its type URL is looked up in the internal
   * registry and the appropriate handler function is invoked.
   *
   * @param received Message received from the client
   */
  void ClientHandler::processMessage(protoMessageType &received) {
    // get the type and try to allocate a handler
    std::string type = received.payload().type_url();
    auto handler = HandlerFactory::create(type, this);

    // invoke handler function
    if(handler) {
      handler->handle(received);
    }
      // otherwise, treat this as an error and close connection
    else {
      std::stringstream error;

      HandlerFactory::dump();

      error << "Received message of unknown type " << type;
      throw ProtocolError(error.str().c_str());
    }
  }
}