//
// Created by Tristan Seifert on 2019-08-23.
//
#include "ChannelDataAck.h"
#include "../HandlerFactory.h"
#include "../ClientHandler.h"

#include <glog/logging.h>

#include <proto/shared/Message.pb.h>
#include <proto/rt/ChannelData.pb.h>
#include <proto/rt/ChannelDataAck.pb.h>


namespace rt::handlers {
  using MessageType = lichtenstein::protocol::rt::ChannelDataAck;

  /// register with the factory
  bool ChannelDataAck::registered = HandlerFactory::registerClass( // NOLINT(cert-err58-cpp)
          "type.googleapis.com/lichtenstein.protocol.rt.ChannelDataAck",
          ChannelDataAck::construct);

  /**
   * Creates a new instance of the "channel data ack" message handler
   *
   * @param api API this request was received on
   * @param client Client from which the request was received
   * @return Instance of handler
   */
  std::unique_ptr<IRequestHandler>
  ChannelDataAck::construct(ClientHandler *client) {
    return std::make_unique<ChannelDataAck>(client);
  }

  /**
   * Handles an acknowledgement for a channel's data.
   *
   * @param received Received message
   */
  void ChannelDataAck::handle(const lichtenstein::protocol::Message &received) {
    int token;

    // unpack message
    MessageType ack;
    received.payload().UnpackTo(&ack);

    VLOG(1) << "Received acknowledge: " << ack.DebugString();

    // forward it to the client handler
    this->client->handlePixelDataAck(ack.transaction());
  }
}