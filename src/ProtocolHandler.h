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

class DataStore;
class INIReader;

class NodeDiscovery;
class DbNode;

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

	private:
		std::vector<std::tuple<uint32_t, DbNode *>> pendingAdoptions;

	private:
		DataStore *store;
		INIReader *config;

		NodeDiscovery *discovery;
};

#endif
