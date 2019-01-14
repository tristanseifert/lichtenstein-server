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

class DataStore;
class EffectRunner;
class INIReader;

class CommandServer {
	public:
		CommandServer(DataStore *store, INIReader *reader, EffectRunner *runner);
		~CommandServer();

		void start();
		void stop();

	private:
		void threadEntry();

		void cleanUpSocket(void);
		void createSocket(void);

		void createSocketUnix(void);
		void createSocketTcp(void);

		void acceptClient(int fd);
		void clientThreadEntry(int fd);

		void processClientRequest(nlohmann::json &j, int fd);

		void clientRequestAddMapping(nlohmann::json &response, nlohmann::json &request);
    void clientRequestRemoveMapping(nlohmann::json &response, nlohmann::json &request);

    void clientRequestGetBrightness(nlohmann::json &response, nlohmann::json &request);
		void clientRequestSetBrightness(nlohmann::json &response, nlohmann::json &request);

		void clientRequestStatus(nlohmann::json &response, nlohmann::json &request);

    void clientRequestListNodes(nlohmann::json &response, nlohmann::json &request);
		void clientRequestUpdateNode(nlohmann::json &response, nlohmann::json &request);

    void clientRequestListGroups(nlohmann::json &response, nlohmann::json &request);
    void clientRequestUpdateGroup(nlohmann::json &response, nlohmann::json &request);
		void clientRequestNewGroup(nlohmann::json &response, nlohmann::json &request);

    void clientRequesListRoutines(nlohmann::json &response, nlohmann::json &request);
    void clientRequestUpdateRoutine(nlohmann::json &response, nlohmann::json &request);
    void clientRequestNewRoutine(nlohmann::json &response, nlohmann::json &request);

    void clientRequesListChannels(nlohmann::json &response, nlohmann::json &request);
    void clientRequesUpdateChannel(nlohmann::json &response, nlohmann::json &request);
    void clientRequesNewChannel(nlohmann::json &response, nlohmann::json &request);
	private:
		enum MessageType {
			kMessageStatus = 0,

      kMessageAddMapping = 1,
			kMessageRemoveMapping = (kMessageAddMapping + 1),

      kMessageGetBrightness = 3,
      kMessageSetBrightness = (kMessageGetBrightness + 1),

			kMessageGetNodes = 5,
      kMessageUpdateNode = (kMessageGetNodes + 1),


      kMessageGetGroups = 7,
      kMessageUpdateGroup = (kMessageGetGroups + 1),
      kMessageNewGroup = (kMessageGetGroups + 2),

      kMessageGetRoutines = 10,
      kMessageUpdateRoutine = (kMessageGetRoutines + 1),
      kMessageNewRoutine = (kMessageGetRoutines + 2),

      kMessageGetChannels = 13,
      kMessageUpdateChannel = (kMessageGetChannels + 1),
      kMessageNewChannel = (kMessageGetChannels + 2)
		};

		enum Error {
			kErrorSyscallError = 1000,
			kErrorInvalidRoutineId,
      kErrorInvalidGroupId,
			kErrorInvalidNodeId,
      kErrorInvalidChannelId,
      kErrorInvalidArguments
		};

		enum SocketMode {
			kSocketModeTcp,
			kSocketModeUnix
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
		INIReader *config;
		EffectRunner *runner;

		SocketMode socketMode;
		std::string socketPath;

		int sock = 0;

		std::thread *worker = nullptr;

		std::atomic_bool run;

		std::vector<std::tuple<int, std::thread *>> clients;

	private:
		static const size_t kClientBufferSz = (1024 * 32);
};

#endif
