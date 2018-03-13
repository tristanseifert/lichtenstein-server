#include "ProtocolHandler.h"

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

#include "db/DataStore.h"
#include "db/Node.h"

/// packet buffer size
static const size_t kClientBufferSz = (1024 * 8);
/// control buffer size for recvfrom
static const size_t kControlBufSz = (1024);

using namespace std;

/**
 * Main server entry point
 */
void ProtocolHandlerEntry(void *ctx) {
#ifdef __APPLE__
	pthread_setname_np("Protocol Handler");
#else
	pthread_setname_np(pthread_self(), "Protocol Handler");
#endif

	ProtocolHandler *srv = static_cast<ProtocolHandler *>(ctx);
	srv->threadEntry();
}

/**
 * Sets up the lichtenstein protocol server.
 */
ProtocolHandler::ProtocolHandler(DataStore *store, INIReader *reader) {
	this->store = store;
	this->config = reader;

	// create the background thread
	this->run = true;

	LOG(INFO) << "Starting protocol handler thread";
	this->worker = new thread(ProtocolHandlerEntry, this);
}

/**
 * Deallocates some structures that were created.
 */
ProtocolHandler::~ProtocolHandler() {
	int err;

	// delete the node discovery thing
	delete this->discovery;

	// close the worker thread
	this->run = false;

	err = close(this->sock);
	LOG_IF(ERROR, err != 0) << "Couldn't close multicast socket: " << strerror(errno);

	this->worker->join();
	delete this->worker;
}

#pragma mark Socket Handling and Worker Thread
/**
 * Creates the server's listening socket.
 */
void ProtocolHandler::createSocket() {
	int err = 0;
	struct sockaddr_in addr;
	int nbytes, addrlen;
	struct ip_mreq mreq;

	unsigned int yes = 1;

	// get the port and IP address to listen on
	int port = this->config->GetInteger("server", "port", 7420);
	string address = this->config->Get("server", "listen", "0.0.0.0");

	err = inet_pton(AF_INET, address.c_str(), &addr.sin_addr.s_addr);
	PLOG_IF(FATAL, err != 1) << "Couldn't convert IP address: ";

	// create the socket
	this->sock = socket(AF_INET, SOCK_DGRAM, 0);
	PLOG_IF(FATAL, this->sock < 0) << "Couldn't create listening socket";

	// allow re-use of the address
	err = setsockopt(this->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	PLOG_IF(FATAL, err < 0) << "Couldn't set SO_REUSEADDR";

	// enable the socket info struct
	err = setsockopt(this->sock, IPPROTO_IP, IP_PKTINFO, &yes, sizeof(yes));
	PLOG_IF(FATAL, err < 0) << "Couldn't set SO_REUSEADDR";

	// set up the destination address
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	// addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	// bind to this address
	err = ::bind(this->sock, (struct sockaddr *) &addr, sizeof(addr));
	PLOG_IF(FATAL, err < 0) << "Couldn't bind listening socket on port " << port;

	LOG(INFO) << "Listening for packets on port " << port;
}

/**
 * Thread entry point
 */
void ProtocolHandler::threadEntry() {
	int err = 0, rsz;

	// allocate the read buffer
	char *buffer = new char[kClientBufferSz];

	// used for recvmsg
	struct msghdr msg;
	struct sockaddr_storage srcAddr;

	struct iovec iov[1];
	iov[0].iov_base = buffer;
	iov[0].iov_len = kClientBufferSz;

	char *controlBuf = new char[kControlBufSz];

	// create the socket
	this->createSocket();

	// create multicast receiver
	this->discovery = new NodeDiscovery(this->store, this->config, this, this->sock);

	// listen on the socket
	while(this->run) {
		// populate the message buffer
		memset(&msg, 0, sizeof(msg));

		msg.msg_name = &srcAddr;
		msg.msg_namelen = sizeof(srcAddr);
		msg.msg_iov = iov;
		msg.msg_iovlen = 1;
		msg.msg_control = controlBuf;
		msg.msg_controllen = kControlBufSz;

		// clear the buffer, then read from the socket
		std::fill(buffer, buffer + kClientBufferSz, 0);

		rsz = recvmsg(this->sock, &msg, 0);

		// if the read size was zero, the connection was closed
		if(rsz == 0) {
			LOG(WARNING) << "Connection " << this->sock << " closed by host";
			break;
		}
		// handle error conditions
		else if(rsz == -1) {
			// ignore messages if we're shutting down
			if(this->run == true) {
				PLOG(WARNING) << "Couldn't read from socket: ";
				break;
			}
		}
		// otherwise, try to parse the packet
		else {
			VLOG(1) << "Received " << rsz << " bytes";
			this->handlePacket(buffer, rsz, &msg);
		}
	}

	// clean up any resources we allocated
	LOG(INFO) << "Closing listening socket";

	delete[] buffer;
	delete[] controlBuf;
}

/**
 * Handles a packet received on the socket.
 */
void ProtocolHandler::handlePacket(void *packet, size_t length, struct msghdr *msg) {
	int err;
	struct cmsghdr *cmhdr;

	static const socklen_t destAddrSz = 128;
	char destAddr[destAddrSz];

	bool isMulticast = false;

	// go through the buffer and find IP_PKTINFO
	for(cmhdr = CMSG_FIRSTHDR(msg); cmhdr != nullptr; cmhdr = CMSG_NXTHDR(msg, cmhdr)) {
		// check if it's the right type
		if(cmhdr->cmsg_level == IPPROTO_IP && cmhdr->cmsg_type == IP_PKTINFO) {
			void *data = CMSG_DATA(cmhdr);
			struct in_pktinfo *info = static_cast<struct in_pktinfo *>(data);

			// check if it's multicast (they're class D, i.e. 1110 MSB)
			unsigned int addr = ntohl(info->ipi_addr.s_addr);
			isMulticast = ((addr >> 28) == 0x0E);

			// convert the destination address
			const char *ptr = inet_ntop(AF_INET, &info->ipi_addr, destAddr, destAddrSz);
			CHECK(ptr != nullptr) << "Couldn't convert destination address";
		}
    }

	// if it's a multicast packet, pass it to the discovery handler
	if(isMulticast) {
		VLOG(2) << "Received multicast packet: forwarding to discovery handler";
		this->discovery->handleMulticastPacket(packet, length);
	}
	// it's not a multicast packet. neat
	else {
		LichtensteinUtils::PacketErrors pErr;

		// check validity
		pErr = LichtensteinUtils::validatePacket(packet, length);

		if(err != LichtensteinUtils::kNoError) {
			LOG(ERROR) << "Couldn't verify multicast packet: " << err;
			return;
		}

		// check to see what type of packet it is
		LichtensteinUtils::convertToHostByteOrder(packet, length);
		lichtenstein_header_t *header = static_cast<lichtenstein_header_t *>(packet);

		VLOG(2) << "Received unicast packet with opcode " << header->opcode;

		// is it an acknowledgement?
		if((header->flags & kFlagAck)) {
			switch(header->opcode) {
				// node adoption?
				case kOpcodeNodeAdoption: {
					// find the transaction number in the pending adoptions
					uint32_t txn = header->txn;

					for(size_t i = 0; i < this->pendingAdoptions.size(); i++) {
						// get the tuple
						auto tuple = this->pendingAdoptions[i];

						// does the transaction number match?
						if(std::get<0>(tuple) == txn) {
							// set the node to be adopted and update it in the DB
							DbNode *node = std::get<1>(tuple);
							node->adopted = 1;

							// update it in the DB, then delete it
							this->store->update(node);
							delete node;

							// remove it from the list
							this->pendingAdoptions.erase(this->pendingAdoptions.begin() + i);

							// logging
							VLOG(2) << "Successfully adopted " << node;
							return;
						}
					}

					// otherwise, if we get here, we couldn't find the node
					LOG(WARNING) << "Received ack for adoption with txn " << txn;
					break;
				}
			}
		}
	}
}

/**
 * Sends an adoption packet to the given node.
 */
void ProtocolHandler::adoptNode(DbNode *node) {
	uint32_t txn;
	int err;
	LichtensteinUtils::PacketErrors pErr;

	// allocate buffer for packet
	size_t totalPacketLen = sizeof(lichtenstein_node_adoption_t);
	totalPacketLen += (sizeof(uint32_t) * node->numChannels);

	void *data = malloc(totalPacketLen);

	// clear its memory
	lichtenstein_node_adoption_t *adopt = static_cast<lichtenstein_node_adoption_t *>(data);
	memset(data, 0, totalPacketLen);

	// fill in header
	LichtensteinUtils::populateHeader(&adopt->header, kOpcodeNodeAdoption);
	txn = adopt->header.txn;

	adopt->header.payloadLength = sizeof(lichtenstein_node_adoption_t) - sizeof(lichtenstein_header_t);

	// put in the server's port
	adopt->port = this->config->GetInteger("server", "port", 7420);

	// get IP and convert
	string address = this->config->Get("server", "listen", "0.0.0.0");

	struct in_addr addr;
	err = inet_pton(AF_INET, address.c_str(), &addr);
	PLOG_IF(FATAL, err != 1) << "Couldn't convert IP address: ";

	adopt->ip = addr.s_addr;

	// fill in the contents of the packet
	adopt->numChannels = node->numChannels;

	for(size_t i = 0; i < node->numChannels; i++) {
		adopt->pixelsPerChannel[i] = 150;
	}

	// byteswap, apply checksum
	err = LichtensteinUtils::convertToNetworkByteOrder(data, totalPacketLen);
	CHECK(err == 0) << "Couldn't convert byte order: " << err;

	pErr = LichtensteinUtils::applyChecksum(data, totalPacketLen);
	CHECK(pErr == LichtensteinUtils::kNoError) << "Error applying checksum: " << pErr;

	// send it to the node
	struct sockaddr_in sockAddr;

	memset(&sockAddr, 0, sizeof(sockAddr));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = node->ip;
	sockAddr.sin_port = htons(7420); // TODO: nodes may have different port

	if(sendto(this->sock, data, totalPacketLen, 0, (struct sockaddr *) &sockAddr, sizeof(sockAddr)) < 0) {
		PLOG_IF(ERROR, errno != 0) << "Couldn't send announcement packet: ";
	} else {
		// write this node's info into the pending adoptions
		this->pendingAdoptions.push_back(std::make_tuple(txn, node));
	}

	// delete memory
	free(data);
}
