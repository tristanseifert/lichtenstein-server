#include "NodeDiscovery.h"

#include "DataStore.h"

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

#include "ProtocolHandler.h"

using namespace std;

/**
 * Sets up the node discovery server.
 */
NodeDiscovery::NodeDiscovery(DataStore *store, INIReader *reader, ProtocolHandler *proto, int sock) {
	this->store = store;
	this->config = reader;
	this->proto = proto;

	// copy the socket and join multicast group
	this->sock = sock;
	this->setUpMulticast();
}

/**
 * De-associates us from the multicast group.
 */
NodeDiscovery::~NodeDiscovery() {
	this->leaveMulticastGroup();
}

/**
 * Removes us from the multicast group.
 */
void NodeDiscovery::leaveMulticastGroup() {
	int err = 0;
	struct ip_mreq mreq;

	LOG(INFO) << "Shutting down multicast receiver";

	// get the port and IP address used for multicast
	string multiAddress = this->config->Get("server", "multicastGroup", "239.42.0.69");
	string address = this->config->Get("server", "listen", "0.0.0.0");

	err = inet_pton(AF_INET, multiAddress.c_str(), &mreq.imr_multiaddr.s_addr);
	PLOG_IF(FATAL, err != 1) << "Couldn't convert IP address: ";

	err = inet_pton(AF_INET, address.c_str(), &mreq.imr_interface.s_addr);
	PLOG_IF(FATAL, err != 1) << "Couldn't convert IP address: ";

	// request to drop the multicast group
	err = setsockopt(this->sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
	PLOG_IF(FATAL, err < 0) << "Couldn't drop multicast group";
}

/**
 * Joins the multicast group on the existing socket.
 */
void NodeDiscovery::setUpMulticast() {
	int err = 0;
	struct ip_mreq mreq;

	// get the configuration
	string multiAddress = this->config->Get("server", "multicastGroup", "239.42.0.69");
	string address = this->config->Get("server", "listen", "0.0.0.0");

	LOG(INFO) << "Joining multicast group " << multiAddress;

	err = inet_pton(AF_INET, address.c_str(), &mreq.imr_interface.s_addr);
	PLOG_IF(FATAL, err != 1) << "Couldn't convert IP address: ";
	err = inet_pton(AF_INET, multiAddress.c_str(), &mreq.imr_multiaddr.s_addr);
	PLOG_IF(FATAL, err != 1) << "Couldn't convert IP address: ";

	// request to join the multicast group
	err = setsockopt(this->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
	PLOG_IF(FATAL, err < 0) << "Couldn't join multicast group";

	// disable multicast loopback
	int no = 0;
	setsockopt(this->sock, IPPROTO_IP, IP_MULTICAST_LOOP, &no, sizeof(no));
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

	VLOG(2) << "Received multicast packet with opcode " << header->opcode;

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
	DbNode *node;

	// get the packet
	lichtenstein_node_announcement_t *packet = static_cast<lichtenstein_node_announcement_t *>(data);

	// see if we've found a node with this MAC address before
	node = this->store->findNodeWithMac(packet->macAddr);

	if(node == nullptr) {
		LOG(INFO) << "Found new node with MAC "
				  << DbNode::macToString(packet->macAddr)
				  << ", adding it to database";

		node = new DbNode();
	}

	// fill the node's info with what we found in the packet
	memcpy(node->macAddr, packet->macAddr, 6);
	node->ip = packet->ip;

	node->hwVersion = packet->hwVersion;
	node->swVersion = packet->swVersion;

	node->lastSeen = time(nullptr);

	// if we get the node solicitation, then it's not been adopted
	node->adopted = 0;

	// process the hostname
	int hostnameLen = packet->hostnameLen;
	char *hostnameBuf = new char[hostnameLen + 1];

	std::fill(hostnameBuf, hostnameBuf + hostnameLen + 1, 0);
	memcpy(hostnameBuf, packet->hostname, hostnameLen);

	node->hostname = string(hostnameBuf);

	// get the framebuffer size and number of channels
	node->numChannels = packet->channels;
	node->fbSize = packet->fbSize;

	// update it in the db
	this->store->update(node);

	// adopt the node: they should only announce if not adopted
	LOG(INFO) << "Adopting node " << DbNode::macToString(packet->macAddr);
	this->proto->adoptNode(node);

	// clean up
	delete[] hostnameBuf;
}
