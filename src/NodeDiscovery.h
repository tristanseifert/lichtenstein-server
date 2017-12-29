#ifndef NODEDISCOVERY_H
#define NODEDISCOVERY_H

#include <thread>
#include <string>
#include <atomic>

#include "DataStore.h"

class NodeDiscovery {
	public:
		NodeDiscovery(DataStore *store);
		~NodeDiscovery();

		void start();
		void stop();

	private:
		void threadEntry();

		void createSocket();

	private:
		friend void NodeDiscoveryEntry(void *ctx);

	private:
		DataStore *store;

		std::thread *worker = nullptr;
		std::atomic_bool run;

		int sock = 0;

	private:
		static const uint16_t kLichtensteinMulticastPort = 7420;

		// size of the read buffer for multicasts
		static const size_t kClientBufferSz = (1024 * 8);
};

#endif
