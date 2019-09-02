//
// Created by Tristan Seifert on 2019-09-02.
//
#include "AdoptionHandler.h"
#include "ProtocolHandler.h"

#include "../helpers/IPHelpers.h"

#include <liblichtenstein/io/TLSClient.h>
#include <liblichtenstein/io/OpenSSLError.h>
#include <liblichtenstein/protocol/ProtocolError.h>

#include <proto/shared/Message.pb.h>
#include <proto/shared/Error.pb.h>
#include <proto/client/GetInfo.pb.h>
#include <proto/client/GetInfoResponse.pb.h>
#include <proto/client/AdoptRequest.pb.h>
#include <proto/client/AdoptAck.pb.h>

#include <INIReader.h>
#include <glog/logging.h>

#include <openssl/rand.h>

using helpers::IPHelpers;

using liblichtenstein::api::MessageIO;
using liblichtenstein::api::ProtocolError;
using liblichtenstein::io::TLSClient;
using SSLError = liblichtenstein::io::OpenSSLError;

using lichtenstein::protocol::Message;
using lichtenstein::protocol::Error;
using lichtenstein::protocol::client::GetInfo;
using lichtenstein::protocol::client::GetInfoResponse;
using lichtenstein::protocol::client::AdoptRequest;
using lichtenstein::protocol::client::AdoptAck;


namespace protocol {
  /**
   * Creates a new adoption handler.
   *
   * @param store Data store holding node information
   * @param reader Configuration
   * @param proto Protocol handler
   */
  AdoptionHandler::AdoptionHandler(std::shared_ptr<DataStore> store,
                                   std::shared_ptr<INIReader> reader,
                                   ProtocolHandler *proto) : store(store),
                                                             config(reader),
                                                             handler(proto) {
    // just read the API hosts from config
    this->getApiHosts();
  }

  /**
   * Cleans up any resources we've allocated, and terminates any outstanding
   * adoptions.
   */
  AdoptionHandler::~AdoptionHandler() {

  }


  /**
   * Attempts to adopt a node, given the hostname and port pointing to its
   * control endpoint.
   *
   * @param host Hostname of the node
   * @param port Port of the node's control endpoint
   */
  void AdoptionHandler::attemptAdoption(const std::string &host,
                                        const unsigned int port) {
    int err;

    // attempt to establish a connection
    auto connection = std::make_shared<TLSClient>(host, port);
    // TODO: configure certificate verification

    auto io = std::make_shared<MessageIO>(connection);

    // verify the node's adoption state and get its UUID
    uuids::uuid nodeUuid;
    this->verifyAdoptionState(io, &nodeUuid);

    VLOG(1) << "Node at " << host << ":" << port << " UUID is "
            << to_string(nodeUuid);

    // generate a random secret to use for this node
    std::vector<std::byte> secret(kSecretLength, std::byte(0));
//    secret.reserve(kSecretLength);
    std::fill(secret.begin(), secret.begin() + (kSecretLength - 1),
              std::byte(0));

    err = RAND_bytes(reinterpret_cast<unsigned char *>(secret.data()),
                     kSecretLength);

    if(err != 1) {
      throw SSLError("RAND_bytes() failed when generating node secret");
    }

    VLOG(1) << "Generated " << secret.size() << " bytes of secret";

    // send the adoption request to the node
    this->sendAdoptReq(io, nodeUuid, secret);
    this->waitAdoptAck(io, nodeUuid);

    // TODO: store the node in data store

    // clean up
    connection->close();
  }

  /**
   * Sends a "Get Info" request to the node to get its adoption status. If it
   * is already adopted, an exception is thrown.
   *
   * @param io Message handler associated with the node
   * @param outUuid Will contain the node's UUID after successful execution
   */
  void AdoptionHandler::verifyAdoptionState(IORef io, uuids::uuid *outUuid) {
    // formulate the message
    GetInfo request;

    request.set_wantsnodeinfo(true);
    request.set_wantsadoptioninfo(true);
    request.set_wantsperformanceinfo(false);

    // send it
    io->sendMessage(request);

    // wait to receive a message
    io->readMessage([this, outUuid](Message &message) {
      std::string type = message.payload().type_url();

      // is it an error?
      if(type == "type.googleapis.com/lichtenstein.protocol.Error") {
        this->handleError(message);
      }
        // did we receive a response?
      else if(type ==
              "type.googleapis.com/lichtenstein.protocol.client.GetInfoResponse") {
        GetInfoResponse info;

        if(!message.payload().UnpackTo(&info)) {
          throw ProtocolError("Failed to unpack GetInfoResponse");
        }

        // verify the adoption state
        if(info.adoption().isadopted()) {
          throw std::runtime_error("Node is already adopted");
        }

        // copy the UUID and store it for later
        auto uuidData = info.node().uuid().data();
        std::array<uuids::uuid::value_type, 16> uuidArray{};
        std::copy(uuidData, uuidData + 16, uuidArray.begin());

        *outUuid = uuids::uuid(uuidArray);
      }
        // unknown message type
      else {
        std::stringstream error;
        error << "Unexpected message type " << type;
        throw ProtocolError(error.str().c_str());
      }
    });
  }

  /**
   * Sends the AdoptRequest message to the node.
   *
   * @param io Message handler associated with the node
   * @param nodeUuid UUID of the node
   * @param secret Secret to send to the node
   */
  void AdoptionHandler::sendAdoptReq(IORef io, const uuids::uuid &nodeUuid,
                                     const std::vector<std::byte> &secret) {
    // prepare the message
    AdoptRequest request;

    request.set_apiaddress(this->apiHost);
    request.set_apiport(this->apiPort);

    request.set_rtaddress(this->rtHost);
    request.set_rtport(this->rtPort);

    // copy the server UUID
    auto serverUuid = this->handler->getServerUuid();
    auto uuidBytes = serverUuid.as_bytes();

    request.set_serveruuid(uuidBytes.data(), uuidBytes.size());

    // copy the secret as well
    request.set_secret(secret.data(), secret.size());

    // then, send the message
    VLOG(1) << "Adopt request: " << request.DebugString();
    io->sendMessage(request);
  }

  /**
   * Waits to receive the response to an adopt request from the node.
   *
   * @param io Message handler associated with the node
   * @param nodeUuid UUID of the node
   */
  void AdoptionHandler::waitAdoptAck(IORef io, const uuids::uuid &nodeUuid) {

    // wait to receive a message
    io->readMessage([this](Message &message) {
      std::string type = message.payload().type_url();

      // is it an error?
      if(type == "type.googleapis.com/lichtenstein.protocol.Error") {
        this->handleError(message);
      }
        // did we receive a response?
      else if(type ==
              "type.googleapis.com/lichtenstein.protocol.client.AdoptAck") {
        AdoptAck ack;

        if(!message.payload().UnpackTo(&ack)) {
          throw ProtocolError("Failed to unpack AdoptAck");
        }

        // make sure adoption was successful
        if(!ack.isadopted()) {
          throw std::runtime_error(
                  "Adoption seems to have failed for some reason");
        }
      }
        // unknown message type
      else {
        std::stringstream error;
        error << "Unexpected message type " << type;
        throw ProtocolError(error.str().c_str());
      }
    });
  }


  /**
   * Determines the hostname/IP address for
   */
  void AdoptionHandler::getApiHosts() {
    // get the API host
    auto apiRemote = this->config->Get("api", "remoteAddress", "");

    if(apiRemote != "") {
      this->apiHost = apiRemote;
    } else {
      // is the listen IP unambiguous?
      auto listenIp = this->config->Get("api", "listen", "0.0.0.0");

      if(IPHelpers::isWildcardAddress(listenIp)) {
        // no, so error out
        throw std::runtime_error(
                "api.listen is wildcard address and api.remoteAddress is not specified");
      } else {
        this->apiHost = listenIp;
      }
    }

    this->apiPort = this->config->GetInteger("api", "port", 45678);

    // get the realtime service host
    auto rtRemote = this->config->Get("realtime", "remoteAddress", "");

    if(rtRemote != "") {
      this->rtHost = rtRemote;
    } else {
      // is the listen IP unambiguous?
      auto listenIp = this->config->Get("realtime", "listen", "0.0.0.0");

      if(IPHelpers::isWildcardAddress(listenIp)) {
        // no, so error out
        throw std::runtime_error(
                "api.listen is wildcard address and api.remoteAddress is not specified");
      } else {
        this->rtHost = listenIp;
      }
    }

    this->rtPort = this->config->GetInteger("realtime", "port", 7420);
  }


  /**
   * Handles an Error message, by converting it into an exception.
   *
   * @param message Received message that contains an Error as payload
   */
  void AdoptionHandler::handleError(const Message &message) {
    Error error;
    if(!message.payload().UnpackTo(&error)) {
      throw ProtocolError("Failed to unpack Error");
    }

    throw ProtocolError(error.DebugString().c_str());
  }
}