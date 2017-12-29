#include "NodeDiscovery.h"

#include <glog/logging.h>
#include <pthread/pthread.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

const char *kLichtensteinMulticastAddress = "239.42.0.69";

/**
 * Main server entry point
 */
void NodeDiscoveryEntry(void *ctx) {
#ifdef __APPLE__
	pthread_setname_np("Multicast Discovery");
#else
	pthread_setname_np(pthread_self(), "Multicast Discovery");
#endif

	NodeDiscovery *srv = static_cast<NodeDiscovery *>(ctx);
	srv->threadEntry();
}

/**
 * Sets up the node discovery server.
 */
NodeDiscovery::NodeDiscovery(DataStore *store) {
	this->store = store;
}

/**
 * Deallocates some structures that were created.
 */
NodeDiscovery::~NodeDiscovery() {
	delete this->worker;
}

/**
 * Starts the multicast receiver thread.
 */
void NodeDiscovery::start() {
	this->run = true;

	LOG(INFO) << "Starting multicast receiver thread";
	this->worker = new thread(NodeDiscoveryEntry, this);
}

/**
 * Prepares the server to stop. This closes down the listening socket and kills
 * the thread.
 */
void NodeDiscovery::stop() {
	int err = 0;

	LOG(INFO) << "Shutting down multicast receiver";

	// signal for the thread to stop, and force the socket to close
	this->run = false;

	// request to drop the multicast group
	struct ip_mreq mreq;
	mreq.imr_multiaddr.s_addr = inet_addr(kLichtensteinMulticastAddress);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	err = setsockopt(this->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
	PLOG_IF(FATAL, err < 0) << "Couldn't drop multicast group";

	// then close the socket; this terminates the read() call
	err = close(this->sock);
	LOG_IF(ERROR, err != 0) << "Couldn't close multicast socket: " << strerror(errno);

	// wait for the thread to terminate
	this->worker->join();
}

/**
 * Entry point for the multicast discovery thread.
 */
void NodeDiscovery::threadEntry() {
	int err = 0, rsz;

	// create the socket
	this->createSocket();

	// allocate the read buffer
	char *buffer = new char[kClientBufferSz];

	// listen on the socket
	while(this->run) {
		// clear the buffer, then read from the socket
		std::fill(buffer, buffer + kClientBufferSz, 0);
		rsz = read(this->sock, buffer, kClientBufferSz);

		// if the read size was zero, the connection was closed
		if(rsz == 0) {
			LOG(WARNING) << "Connection " << this->sock << " closed by host";
			break;
		}
		// handle error conditions
		else if(rsz == -1) {
			// ignore messages if we're shutting down
			if(this->run == true) {
				PLOG(WARNING) << "Couldn't read from multicast socket: ";
				break;
			}
		}
		// otherwise, try to parse the packet
		else {
			LOG(INFO) << "Received " << rsz << " bytes via multicast";
		}
	}

	// clean up any resources we allocated
	LOG(INFO) << "Closing multicast connection";

	delete[] buffer;
}

/**
 * Creates the multicast socket.
 */
void NodeDiscovery::createSocket() {
	int err = 0;
	struct sockaddr_in addr;
	int nbytes, addrlen;
	struct ip_mreq mreq;

	// create the socket
	this->sock = socket(AF_INET, SOCK_DGRAM, 0);
	PLOG_IF(FATAL, this->sock < 0) << "Couldn't create multicast socket";

	// allow re-use of the address
	unsigned int yes = 1;
	err = setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	PLOG_IF(FATAL, err < 0) << "Couldn't set SO_REUSEADDR";

	// set up the destination address
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(kLichtensteinMulticastPort);

	// bind to this address
	err = ::bind(this->sock, (struct sockaddr *) &addr, sizeof(addr));
	PLOG_IF(FATAL, err < 0) << "Couldn't bind multicast socket";

	// request to join the multicast group
	mreq.imr_multiaddr.s_addr = inet_addr(kLichtensteinMulticastAddress);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	err = setsockopt(this->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
	PLOG_IF(FATAL, err < 0) << "Couldn't join multicast group";
}
