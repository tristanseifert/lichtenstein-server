/**
 * Implements the command server, a server that listens for JSON-formatted
 * requests on an UNIX socket and responds to them. This is the primary
 * interface through which the server is controlled and can be configured, for
 * example, from a web application.
 */
#ifndef COMMANDSERVER_H
#define COMMANDSERVER_H

#include <thread>
#include <string>
#include <atomic>
#include <vector>

#include "json.hpp"

#include "DataStore.h"

class CommandServer {
	public:
		CommandServer(std::string socket, DataStore *store);
		~CommandServer();

		void start();
		void stop();

	private:
		void threadEntry();

		void createSocket();

		void acceptClient(int fd);
		void clientThreadEntry(int fd);

		void processClientRequest(nlohmann::json &j, int fd);

		void clientRequestStatus(nlohmann::json &response);

	private:
		enum MessageType {
			kMessageStatus = 0,
		};

	private:
		friend void CommandServerEntry(void *ctx);
		friend void CommandClientEntry(void *ctx);

		class ClientThreadCtx {
			public:
				ClientThreadCtx(CommandServer *srv, int fd) : srv(srv), fd(fd) {}

				CommandServer *srv;
				int fd;
		};

	private:
		DataStore *store;

		std::string socketPath;
		std::thread *worker = nullptr;

		std::atomic_bool run;

		std::vector<std::tuple<int, std::thread *>> clients;

		int sock = 0;

	private:
		static const size_t kClientBufferSz = (1024 * 32);
};

#endif
