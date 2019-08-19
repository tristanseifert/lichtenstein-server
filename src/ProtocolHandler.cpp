#include "ProtocolHandler.h"

#include <chrono>

#include <glog/logging.h>
#include <pthread.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "db/DataStore.h"
#include "db/Node.h"
#include "db/Channel.h"


/**
 * Sets up the lichtenstein protocol server.
 */
ProtocolHandler::ProtocolHandler(DataStore *store, INIReader *reader) {
	this->store = store;
	this->config = reader;
}

/**
 * Deallocates some structures that were created.
 */
ProtocolHandler::~ProtocolHandler() {
}


/**
 * Sends an adoption packet to the given node.
 */
void ProtocolHandler::adoptNode(DbNode *node) {

}


/**
 * Sends pixel data to the node.
 */
void ProtocolHandler::sendDataToNode(DbChannel *channel, void *pixelData, size_t numPixels, bool isRGBW) {

}

/**
 * Waits for all pixel data frames to be sent to the nodes, or for a timeout on
 * waiting for a response, in case a node is unreachable.
 */
void ProtocolHandler::waitForOutstandingFramebufferWrites(void) {
  /* not implemented right now */
}

/**
 * Multicasts the "output enable" command.
 */
void ProtocolHandler::sendOutputEnableForAllNodes(void) {
  return; /* not currently implemented, lol */
}

/**
 * Prepares for a shutdown of the effect thread. This really just signals the
 * lock that thread might be waiting on.
 */
void ProtocolHandler::prepareForShutDown(void) {

}
