//
// Created by Tristan Seifert on 2019-08-20.
//
#include "ClientHandler.h"
#include "HandlerFactory.h"
#include "IRequestHandler.h"
#include "PixelDataHandler.h"

#include "db/Node.h"
#include "db/Channel.h"

#include <glog/logging.h>

#include <liblichtenstein/io/GenericServerClient.h>
#include <liblichtenstein/io/OpenSSLError.h>
#include <liblichtenstein/io/SSLSessionClosedError.h>
#include <liblichtenstein/protocol/ProtocolError.h>
#include <proto/rt/ChannelData.pb.h>
#include <proto/shared/Message.pb.h>

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
    this->close();

    // wait for thread to join and delete it
    if(this->thread) {
      if(this->thread->joinable()) {
        this->thread->join();
      }

      this->thread = nullptr;
    }
  }

  /**
   * Handles closing the client handler connection.
   */
  void ClientHandler::close() {
    if(this->shutdown) return;

    VLOG(1) << "Closing rt client connection " << this;

    // this just sets the shutdown variable
    GenericClientHandler::close();

    // de-register all pixel data callbacks
    {
      // XXX: could this deadlock? i don't think so
      std::lock_guard lock(this->pixelDataCallbacksLock);

      for(int callback : this->pixelDataCallbacks) {
        PixelDataHandler::removeHandler(callback);
      }

      this->pixelDataCallbacks.clear();
    }

    // close the client connection
    this->client->close();
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
          if(this->shutdown) return;

          this->processMessage(message);
        });
      }
        // an error in the TLS library happened
      catch(SSLError &e) {
        // ignore TLS errors if we're shutting down
        if(!this->shutdown) {
          LOG(ERROR) << "TLS error in RT client: " << e.what();
        }
      }
        // if we get this exception, session was closed
      catch(SSLSessionClosedError &e) {
        VLOG(1) << "Connection was closed in RT client: " << e.what();
        break;
      }
        // an error decoding message
      catch(ProtocolError &e) {
        LOG(ERROR) << "Protocol error in RT client: " << e.what();
        this->sendException(e);
      }
        // system error
      catch(std::system_error &e) {
        LOG(ERROR) << "System error in RT client: " << e.what();
        break;
      }
        // some other runtime error happened
      catch(std::runtime_error &e) {
        LOG(ERROR) << "Runtime error RT client: " << e.what();
        this->sendException(e);

        break;
      }
    }

    // clean up client
    VLOG(1) << "Shutting down rt API client for client " << this->client;
    this->close();
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


  /**
   * Called whenever a channel to which we are subscribed has received new pixel
   * data.
   *
   * @param channel Channel with data
   * @param data Data (raw bytes)
   * @param format format of the data
   */
  void ClientHandler::receivedData(const DbChannel &channel,
                                   const std::vector<std::byte> &data,
                                   PixelDataHandler::PixelFormat format) {
    using ChannelDataMsgType = lichtenstein::protocol::rt::ChannelData;
    using PixelFmtType = ChannelDataMsgType::PixelFormat;

    // return immediately if client connection is closed
    if(!this->client || this->shutdown) {
      return;
    }

    // construct the appropriate message and send it
    ChannelDataMsgType dataMsg;

    switch(format) {
      case PixelDataHandler::RGB:
        dataMsg.set_format(PixelFmtType::ChannelData_PixelFormat_RGB);
        break;

      case PixelDataHandler::RGBW:
        dataMsg.set_format(PixelFmtType::ChannelData_PixelFormat_RGBW);
        break;

      default:
        LOG(FATAL) << "Invalid pixel format: " << format;
        break;
    }

    // generate a random transaction number for acknowledgement and store it
    // TODO: actually do something with this
    dataMsg.set_transaction(this->random());

    // actually send response
    this->sendResponse(dataMsg);
  }

  /**
   * Registers a new pixel data callback, this is typically called from the
   * request handler for subscribing to a new channel.
   *
   * @param token
   */
  void ClientHandler::registerPixelDataCallback(int token) {
    // try to take the lock and add to vector
    std::lock_guard lock(this->pixelDataCallbacksLock);

    this->pixelDataCallbacks.push_back(token);
  }

  /**
   * When a node responds with a ChannelDataAck message, this function is
   * invoked with the transaction value as a parameter.
   *
   * @param txn Transaction number from ack packet
   */
  void ClientHandler::handlePixelDataAck(int txn) {

  }
}