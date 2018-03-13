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

class DataStore;
class INIReader;

class NodeDiscovery;
class DbNode;
class DbChannel;

class ProtocolHandler {
	public:
		ProtocolHandler(DataStore *store, INIReader *reader);
		~ProtocolHandler();

	private:
		int sock;

		void createSocket();

	private:
		friend void ProtocolHandlerEntry(void *ctx);

		std::thread *worker;
		std::atomic_bool run;

		void threadEntry();
		void handlePacket(void *packet, size_t length, struct msghdr *msg);

	public:
		void adoptNode(DbNode *node);

		void sendDataToNode(DbChannel *channel, void *data, size_t numPixels, bool isRGBW);

	private:
		std::vector<std::tuple<uint32_t, DbNode *>> pendingAdoptions;
		std::vector<std::tuple<uint32_t, DbNode *, std::chrono::time_point<std::chrono::high_resolution_clock>>> pendingFBDataWrites;

	private:
		DataStore *store;
		INIReader *config;

		NodeDiscovery *discovery;
};

#endif
