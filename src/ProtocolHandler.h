/**
 * This class handles the Lichtenstein protocol. It sets up the listening socket
 * and parses all requests it receives, including multicast node announcements.
 */
#ifndef PROTOCOLHANDLER_H
#define PROTOCOLHANDLER_H

#include <thread>
#include <atomic>

class DataStore;
class INIReader;

class NodeDiscovery;

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

	private:
		DataStore *store;
		INIReader *config;

		NodeDiscovery *discovery;
};

#endif
