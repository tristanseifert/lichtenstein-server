//
// Created by Tristan Seifert on 2019-08-22.
//
#include "JoinChannel.h"
#include "../HandlerFactory.h"
#include "../ClientHandler.h"

#include <glog/logging.h>

#include <proto/shared/Message.pb.h>
#include <proto/rt/JoinChannel.pb.h>
#include <proto/rt/JoinChannelAck.pb.h>

#include "db/Node.h"
#include "db/Channel.h"


namespace rt::handlers {
  using MessageType = lichtenstein::protocol::rt::JoinChannel;
  using AckMessageType = lichtenstein::protocol::rt::JoinChannelAck;

  /// register with the factory
  bool JoinChannel::registered = HandlerFactory::registerClass( // NOLINT(cert-err58-cpp)
          "type.googleapis.com/lichtenstein.protocol.rt.JoinChannel",
          JoinChannel::construct);

  /**
   * Creates a new instance of the "Join Channel" message.
   *
   * @param api API this request was received on
   * @param client Client from which the request was received
   * @return Instance of handler
   */
  std::unique_ptr<IRequestHandler>
  JoinChannel::construct(ClientHandler *client) {
    return std::make_unique<JoinChannel>(client);
  }


  /**
   * Handles a received 'join channel' message. Currently, this does NOT handle
   * updating an existing subscription.
   *
   * @param received Received message
   */
  void JoinChannel::handle(const lichtenstein::protocol::Message &received) {
    int token;

    // unpack message
    MessageType join;
    received.payload().UnpackTo(&join);

    LOG(INFO) << "Get info: " << join.DebugString();

    // fetch channel
    DbChannel channel;

    // register the handler
    auto client = this->client;
    auto offset = join.offset();
    auto length = join.length();

    token = PixelDataHandler::registerHandler(channel, [client, offset, length](
            auto channel, auto dataIn, auto format) {
      // copy data so we don't corrupt the called in version
      std::vector<std::byte> data = dataIn;

      // get the subset of the data we're interested in
      if(offset) {
        data.erase(data.begin(), data.begin() + offset);
      }

      if(length < data.size()) {
        data.resize(length);
      }

      // send it to the client
      client->receivedData(channel, data, format);
    });

    this->client->registerPixelDataCallback(token);

    // send acknowledgement
    AckMessageType response;

    //    response.set_allocated_channel(join.channel());
    response.set_numpixels(length);

    this->client->sendResponse(response);
  }
}