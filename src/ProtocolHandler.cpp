#include "ProtocolHandler.h"

#include <glog/logging.h>

#include <openssl/ssl.h>

// define shorthands for some classes we use a lot
using ServerAPI = api::API;
using RealtimeAPI = rt::API;



/**
 * Sets up the lichtenstein protocol server.
 */
ProtocolHandler::ProtocolHandler(DataStore *db, INIReader *ini) : store(db),
                                                                  config(ini) {
  // initialize LibreSSL
  SSL_library_init();
  SSL_load_error_strings();

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
