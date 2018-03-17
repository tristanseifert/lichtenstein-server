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
		void fbSendTimeoutExpired(DbChannel *ch, uint32_t txn);
		void waitForOutstandingFramebufferWrites(void);

		void sendOutputEnableForAllNodes(void);

		void prepareForShutDown(void);

	private:
		std::atomic_int numPendingFBWrites;
		std::mutex pendingOutputMutex;

	private:
		// adoptions we're waiting on to complete
		std::vector<std::tuple<uint32_t, DbNode *>> pendingAdoptions;
		// nodes we're waiting on to acknowledge a framebuffer write
		std::vector<std::tuple<uint32_t, DbNode *, std::chrono::time_point<std::chrono::high_resolution_clock>, CppTime::timer_id>> pendingFBDataWrites;

		// timer used for timeouts
		CppTime::Timer timer;

	private:
		// after how many packets with errors we assume the node died
		static const size_t MaxPacketsWithErrors = 15;
		// how many packet "sends" to wait before sending more data
		static const size_t NumPacketsToWaitAfterError = 60;

	private:
		DataStore *store;
		INIReader *config;

		NodeDiscovery *discovery;
};

#endif
