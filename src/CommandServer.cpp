#include "CommandServer.h"

#include "json.hpp"
#include <glog/logging.h>

#include <thread>

#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ucred.h>

using namespace std;
using json = nlohmann::json;

/**
 * Main server entry point
 */
void CommandServerEntry(void *ctx) {
	CommandServer *srv = static_cast<CommandServer *>(ctx);
	srv->threadEntry();
}

/**
 * Client thread entry point
 */
void CommandClientEntry(void *ctx) {
	CommandServer::ClientThreadCtx *info = static_cast<CommandServer::ClientThreadCtx *>(ctx);
	info->srv->clientThreadEntry(info->fd);
}


/**
 * Initializes the command server. This creates some internal structures and
 * prepares for the thread to start.
 */
CommandServer::CommandServer(string socket) {
	this->socketPath = socket;
}

/**
 * Closes the listening socket.
 */
CommandServer::~CommandServer() {

}

/**
 * Starts the command server thread.
 */
void CommandServer::start() {
	LOG(INFO) << "Starting command server thread";
	this->worker = new thread(CommandServerEntry, this);
}

/**
 * Prepares the server to stop. This closes down the listening socket and kills
 * the thread.
 */
void CommandServer::stop() {
	int err = 0;

	LOG(INFO) << "Shutting down command server...";

	// signal for the thread to stop, and force the socket to close
	this->run = false;

	// close the socket; this'll terminate the accept() call
	err = close(this->sock);
	LOG_IF(ERROR, err != 0) << "Couldn't close command socket: " << strerror(errno);

	// wait for the thread to terminate
	this->worker->join();

	// now, terminate any remaining clients
	if(!this->clients.empty()) {
		LOG(INFO) << "Closing " << this->clients.size() << " active client connections";

		for(auto value : this->clients) {
			int fd = get<0>(value);
			thread *t = get<1>(value);

			// check if the fd is closed already
		   	if(fcntl(fd, F_GETFD) != -1) {
				// if not, close it
		   		err = close(fd);
		   		LOG_IF(ERROR, err != 0) << "Couldn't close client socket: " << strerror(errno);
		   	}

			// now, wait for the thread to terminate
			t->join();
		}
	} else {
		LOG(INFO) << "No active client connections to close, we're done";
	}
}

/**
 * Main loop for the command server thread; this is called when the thread is
 * first initialized. This sets up the socket and waits for commands.
 */
void CommandServer::threadEntry() {
	int msgsock = 0;
	int err = 0;

	// create the socket
	this->createSocket();

	// listen on the socket
	while(this->run) {
		// accept incoming connections
		msgsock = accept(sock, 0, 0);

		// check return value; if it's -1, there was an error
		if(msgsock == -1) {
			// ignore ECONNABORTED messages if we're shutting down
			if(errno != ECONNABORTED && this->run == true) {
				LOG(WARNING) << "Couldn't accept command connection: " << strerror(errno);
			}

			// continue the loop
			continue;
		}

		// otherwise, accept the connection and start a new thread
		this->acceptClient(msgsock);
	}

	// clean up the socket (i.e. delete the file on disk)
	LOG(INFO) << "Closing command connection";

	err = unlink(this->socketPath.c_str());
	LOG_IF(ERROR, err != 0) << "Couldn't unlink command socket: " << strerror(errno);
}

/**
 * Creates the domain socket (using the stream mode) and prepares it for
 * listening.
 */
void CommandServer::createSocket() {
	struct sockaddr_un server;
	int err = 0;

	// create the bare socket
	this->sock = socket(AF_UNIX, SOCK_STREAM, 0);
	PCHECK(this->sock > 0) << "Creating command server socket failed";

	// set up properties for binding
	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, this->socketPath.c_str()); // TODO: length checking

	// bind the socket
	err = ::bind(this->sock, (struct sockaddr *) &server, (socklen_t) sizeof(struct sockaddr_un));
	PCHECK(err == 0) << "Binding command server socket failed on " << this->socketPath;

	// accept connections
	err = listen(this->sock, 5);
	PCHECK(err == 0) << "Listening on command server socket failed";

	// log some info
	LOG(INFO) << "Created command socket at " << this->socketPath;
	this->run = true;
}

/**
 * Processes a request from a client. This has the effect of creating a new
 * thread for that client and continuously reading from that socket.
 */
void CommandServer::acceptClient(int fd) {
	// allocate context and pass it to the new thread
	ClientThreadCtx *ctx = new ClientThreadCtx(this, fd);
	thread *t = new thread(CommandClientEntry, ctx);

	// store this info for later so we can cleanly terminate connections
	this->clients.push_back(make_tuple(fd, t));
}

/**
 * Entry point for the client thread.
 */
void CommandServer::clientThreadEntry(int fd) {
	int err = 0, rsz = 0, len = 0;

	// get the process that opened this connection
	struct ucred ucred;
	len = sizeof(struct ucred);

	/*
	 * get the pid of the remote process. this is only done for logging purposes
	 * so on systems where this isn't supported (*cough* macOS) it just doesn't
	 * get compiled in. really lame, but meh.
	 */
#ifdef SO_PEERCRED
	err = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len);

	LOG_IF(ERROR, err != 0) << "Couldn't get peer info on " << fd << ": " << strerror(errno);

	if(err == 0) {
		LOG(INFO) << "Received connection from pid " << ucred.pid << " on fd " << fd;
	}
#endif

	// allocate a read buffer
	char *buffer = new char[kClientBufferSz];
	std::fill(buffer, buffer + kClientBufferSz, 0);

	// keep reading from the socket while we are running
	while(this->run) {
		rsz = read(fd, buffer, kClientBufferSz);

		// if the read size was zero, the connection was closed
		if(rsz == 0) {
			LOG(WARNING) << "Connection " << fd << " closed by host";
			break;
		}
	}

	/*
	 * we _could_ remove this thread from the clients vector, but since the
	 * socket is already closed, we can simply check if it's been closed by
	 * the shutdown procedure (or another external source) and only close it
	 * if needed. the shutdown procedure will only close file descriptors that
	 * haven't been closed and wait for the thread to terminate.
 	 */
	if(fcntl(fd, F_GETFD) != -1) {
		err = close(fd);
		LOG_IF(ERROR, err != 0) << "Couldn't close client socket: " << strerror(errno);
	}

	// de-allocate the buffer
	delete[] buffer;
}
