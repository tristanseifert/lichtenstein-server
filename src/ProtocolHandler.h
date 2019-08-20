/**
 * This class handles the Lichtenstein protocol. It sets up the listening socket
 * and parses all requests it receives, including multicast node announcements.
 */
#ifndef PROTOCOLHANDLER_H
#define PROTOCOLHANDLER_H

#include "api/API.h"

#include <memory>

class DataStore;
class INIReader;

class DbNode;
class DbChannel;


class ProtocolHandler {
	public:
		ProtocolHandler(DataStore *store, INIReader *reader);
		~ProtocolHandler();

	public:
    void sendPixelData(DbChannel *channel, void *data, size_t numPixels,
                       bool isRGBW);

		void sendOutputEnableForAllNodes(void);

		void prepareForShutDown(void);


	private:
		DataStore *store;
		INIReader *config;

    std::unique_ptr<api::API> serverApi;
};

#endif
