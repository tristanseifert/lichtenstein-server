#include "ProtocolHandler.h"
#include "AdoptionHandler.h"

#include "../config/Config.h"
#include "../config/Defaults.h"

#include "../api/API.h"
#include "../rt/API.h"
#include "../rt/PixelDataHandler.h"

#include <glog/logging.h>

#include <openssl/ssl.h>
#include <google/protobuf/stubs/common.h>

// define shorthands for some classes we use a lot
using ServerAPI = api::API;
using RealtimeAPI = rt::API;

using PixelHandler = rt::PixelDataHandler;

using protocol::AdoptionHandler;

// define defaults
static bool defaultsRegistered =
  config::Defaults::registerString("server.uuid", "", "Server UUID value");


namespace protocol {
  /**
   * Sets up the lichtenstein protocol server.
   */
  ProtocolHandler::ProtocolHandler(std::shared_ptr<DataStore> db) : store(db) {
    // verify that the protobuf library version is correct
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // initialize LibreSSL
    SSL_library_init();
    SSL_load_error_strings();

    // read server uuid
    auto uuidStr = Config::getString("server.uuid");
    auto uuid = uuids::uuid::from_string(uuidStr);

    CHECK(uuid.has_value()) << "Failed to parse UUID: '" << uuidStr << "'";

    this->uuid = uuid.value();

    LOG(INFO) << "Server UUID is " << to_string(this->uuid);

    // set up the API
    this->serverApi = std::make_unique<ServerAPI>(this->store);

    // also, create the realtime API
    this->rtApi = std::make_unique<RealtimeAPI>(this->store);

    // set up adoption handler
    this->orphanage = std::make_unique<AdoptionHandler>(this->store, this);

    // try some shit
    try {
      this->orphanage->attemptAdoption("127.0.0.1", 56789);
    } catch(std::exception &e) {
      LOG(ERROR) << "Failed to adopt node: " << e.what();
    }
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