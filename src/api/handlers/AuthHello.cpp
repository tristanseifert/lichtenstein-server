//
// Created by Tristan Seifert on 2019-09-02.
//
#include "AuthHello.h"

#include "../HandlerFactory.h"
#include "../ClientHandler.h"

#include <liblichtenstein/protocol/HmacChallengeHandler.h>

#include <proto/shared/Message.pb.h>
#include <proto/shared/Error.pb.h>
#include <proto/shared/AuthHello.pb.h>

#include <uuid.h>

#include <exception>

using lichtenstein::protocol::Message;
using AuthHelloMsg = lichtenstein::protocol::AuthHello;

using liblichtenstein::api::HmacChallengeHandler;


namespace api::handlers {
  /// register with the factory
  bool AuthHello::registered = HandlerFactory::registerClass( // NOLINT(cert-err58-cpp)
          "type.googleapis.com/lichtenstein.protocol.AuthHello",
          AuthHello::construct);

  /**
   * Creates a new instance of the "Auth Hello" message.
   *
   * @param api API this request was received on
   * @param client Client from which the request was received
   * @return Instance of handler
   */
  std::unique_ptr<IRequestHandler> AuthHello::construct(ClientHandler *client) {
    return std::make_unique<AuthHello>(client);
  }


  /**
   * Handles a received 'join channel' message. Currently, this does NOT handle
   * updating an existing subscription.
   *
   * @param received Received message
   */
  void AuthHello::handle(const Message &received) {
    // unpack message
    AuthHelloMsg hello;

    if(!received.payload().UnpackTo(&hello)) {
      throw std::runtime_error("Failed to unpack AuthHello");
    }

    // get the node's uuid
    auto uuidData = hello.uuid().data();
    std::array<uuids::uuid::value_type, 16> uuidArray{};
    std::copy(uuidData, uuidData + 16, uuidArray.begin());

    uuids::uuid nodeUuid(uuidArray);

    // try to find a node by that UUID

    // handle the HMAC challenge here
    auto connection = this->client->getClient();

    HmacChallengeHandler challenge(connection, "A", nodeUuid);

    // this will throw if failed
    challenge.handleAuthentication(hello);
  }
}