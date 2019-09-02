#include "ProtocolHandler.h"
#include "AdoptionHandler.h"

#include "../api/API.h"
#include "../rt/API.h"
#include "../rt/PixelDataHandler.h"

#include <INIReader.h>

#include <glog/logging.h>

#include <openssl/ssl.h>
#include <google/protobuf/stubs/common.h>

// define shorthands for some classes we use a lot
using ServerAPI = api::API;
using RealtimeAPI = rt::API;

using PixelHandler = rt::PixelDataHandler;

using protocol::AdoptionHandler;


namespace protocol {
  /**
   * Sets up the lichtenstein protocol server.
   */
  ProtocolHandler::ProtocolHandler(std::shared_ptr<DataStore> db,
                                   std::shared_ptr<INIReader> ini) : store(db),
                                                                     config(ini) {
    // verify that the protobuf library version is correct
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // initialize LibreSSL
    SSL_library_init();
    SSL_load_error_strings();

    // read server uuid
    auto uuidStr = this->config->Get("server", "uuid", "");
    auto uuid = uuids::uuid::from_string(uuidStr);

    CHECK(uuid.has_value()) << "Failed to parse UUID: '" << uuidStr << "'";

    this->uuid = uuid.value();

    LOG(INFO) << "Server UUID is " << to_string(this->uuid);

    // set up the API
    this->serverApi = std::make_unique<ServerAPI>(this->store, this->config);

    // also, create the realtime API
    this->rtApi = std::make_unique<RealtimeAPI>(this->store, this->config);

}

  /**
   * Deallocates some structures that were created.
   */
  ProtocolHandler::~ProtocolHandler() {

  }


  /**
   * Queues pixel data for the given channel.
   */
  void ProtocolHandler::sendPixelData(DbChannel *channel, void *pixelData,
                                      size_t numPixels, bool isRGBW) {
    // send it to the pixel data handler
    PixelHandler::PixelFormat format = PixelHandler::RGB;

    if(isRGBW) {
      format = PixelHandler::RGBW;
    }

    PixelHandler::receivedData(*channel, pixelData, numPixels, format);
  }


  /**
   * Called by the effect runner after data has been sent to all nodes.
   */
  void ProtocolHandler::sendOutputEnableForAllNodes(void) {

  }

  /**
   * Called as the effect worker shuts down.
   */
  void ProtocolHandler::prepareForShutDown(void) {

  }
}