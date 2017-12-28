#ifndef COMMANDSERVER_H
#define COMMANDSERVER_H

#include <thread>
#include <string>
#include <atomic>
#include <vector>

class CommandServer {
	public:
		CommandServer(std::string socket);
		~CommandServer();

		void start();
		void stop();

	private:
		void threadEntry();

		void createSocket();

		void acceptClient(int fd);
		void clientThreadEntry(int fd);

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
		std::string socketPath;
		std::thread *worker = nullptr;

		std::atomic_bool run;

		std::vector<std::tuple<int, std::thread *>> clients;

		int sock = 0;

	private:
		static const size_t kClientBufferSz = (1024 * 32);
};

#endif
