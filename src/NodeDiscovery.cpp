#include "NodeDiscovery.h"

#include <glog/logging.h>
#include <pthread/pthread.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "lichtenstein_proto.h"
#include "LichtensteinUtils.h"

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
			this->handleMulticastPacket(buffer, rsz);
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

/**
 * Handles a multicast packet received over the wire. This first checks it for
 * validity before processing it.
 */
void NodeDiscovery::handleMulticastPacket(void *data, size_t length) {
	LichtensteinUtils::PacketErrors err;

	// check validity
	err = LichtensteinUtils::validatePacket(data, length);

	if(err != LichtensteinUtils::kNoError) {
		LOG(ERROR) << "Couldn't verify multicast packet: " << err;
		return;
	}

	// check to see what type of packet it is
	LichtensteinUtils::convertToHostByteOrder(data, length);
	lichtenstein_header_t *header = static_cast<lichtenstein_header_t *>(data);

	// handle node announcements
	if(header->opcode == kOpcodeNodeAnnouncement) {
		this->processNodeAnnouncement(data, length);
	}
}

/**
 * Processes a multicasted node announcement. This checks in the database if
 * this node (nodes are identified by their MAC address) has been seen before;
 * if so, and the node was previously adopted, attempt to adopt the node again.
 * Otherwise, store the information about the node in the database so that the
 * node could be adopted at a later time.
 *
 * Either way, information such as IP and hostname are updated in the database.
 */
void NodeDiscovery::processNodeAnnouncement(void *data, size_t length) {
	DataStore::Node *node;

	// get the packet
	lichtenstein_node_announcement_t *packet = static_cast<lichtenstein_node_announcement_t *>(data);

	// see if we've found a node with this MAC address before
	node = this->store->findNodeWithMac(packet->macAddr);

	if(node == nullptr) {
		// convert the mac into a string real quick
		char mac[24];
		snprintf(mac, 24, "%02X-%02X-%02X-%02X-%02X-%02X", packet->macAddr[0],
				 packet->macAddr[1], packet->macAddr[2], packet->macAddr[3],
			 	 packet->macAddr[4], packet->macAddr[5]);

		LOG(INFO) << "Found new node with MAC " << mac << ", adding it to database";

		node = new DataStore::Node();
	}

	// fill the node's info with what we found in the packet
	memcpy(node->macAddr, packet->macAddr, 6);
	node->ip = packet->ip;

	node->hwVersion = packet->hwVersion;
	node->swVersion = packet->swVersion;

	node->lastSeen = time(nullptr);

	// if we get the node solicitation, then it's not been adopted
	node->adopted = 0;

	// update it in the db
	this->store->updateNode(node);
}
