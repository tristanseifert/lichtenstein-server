/**
 * This class handles the Lichtenstein protocol. It sets up the listening socket
 * and parses all requests it receives, including multicast node announcements.
 */
#ifndef PROTOCOLHANDLER_H
#define PROTOCOLHANDLER_H

#include <thread>
#include <atomic>
#include <vector>
#include <tuple>
#include <chrono>
#include <mutex>

#include <cpptime.h>

class DataStore;
class INIReader;

class NodeDiscovery;
class DbNode;
class DbChannel;

class ProtocolHandler {
	public:
		ProtocolHandler(DataStore *store, INIReader *reader);
		~ProtocolHandler();

	public:
		void adoptNode(DbNode *node);

		void sendDataToNode(DbChannel *channel, void *data, size_t numPixels, bool isRGBW);
		void waitForOutstandingFramebufferWrites(void);

		void sendOutputEnableForAllNodes(void);

		void prepareForShutDown(void);


	private:
		DataStore *store;
		INIReader *config;
};

#endif
