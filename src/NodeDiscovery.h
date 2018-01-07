#ifndef NODEDISCOVERY_H
#define NODEDISCOVERY_H

#include <thread>
#include <string>
#include <atomic>

#include <cstddef>

#include "INIReader.h"

class DataStore;

class NodeDiscovery {
	friend class ProtocolHandler;

	public:
		NodeDiscovery(DataStore *store, INIReader *reader, int sock);
		~NodeDiscovery();

	private:
		void leaveMulticastGroup();
		void setUpMulticast();

		void handleMulticastPacket(void *data, size_t length);
		void processNodeAnnouncement(void *data, size_t length);

	private:
		DataStore *store;
		INIReader *config;

		int sock = 0;

	private:
		static const uint16_t kLichtensteinMulticastPort = 7420;

		// size of the read buffer for multicasts
		static const size_t kClientBufferSz = (1024 * 8);
};

#endif
