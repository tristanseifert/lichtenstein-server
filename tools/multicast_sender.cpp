/**
 * Simulates a lichtenstein node by sending the correct multicast packets.
 *
 * Build using `clang multicast_sender.cpp -lstdc++ -o multicast_sender`
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../src/crc32/crc32.cpp"
#include "../src/lichtenstein_proto.h"

const uint16_t kLichtensteinMulticastPort = 7420;
const char *kLichtensteinMulticastAddress = "239.42.0.69";

int main(int argc, char *argv[]) {
	struct sockaddr_in addr;
	int fd, cnt;
	struct ip_mreq mreq;

	// set up an UDP socket
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	// set up the destination address of the multicast group
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(kLichtensteinMulticastAddress);
	addr.sin_port = htons(kLichtensteinMulticastPort);

	// test data
	static const uint8_t testMac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00};
	static const char *name = "test-node";
	static const size_t nameLen = strlen(name) + 1;

	// total length of the packet
	size_t totalPacketLen = sizeof(lichtenstein_node_announcement_t) + nameLen;

	// send the multicast node announcement every 10 seconds
	while(1) {
		lichtenstein_node_announcement_t *announce = static_cast<lichtenstein_node_announcement_t *>(malloc(totalPacketLen));
		memset(announce, 0, totalPacketLen);

		// configure the header
		announce->header.magic = htonl(kLichtensteinMagic);
		announce->header.version = htonl(kLichtensteinVersion10);

		announce->header.flags = htons(kFlagMulticast | kFlagChecksummed);
		announce->header.opcode = htons(kOpcodeNodeAnnouncement);

		announce->header.sequenceIndex = htons(0);
		announce->header.sequenceNumPackets = htons(1);

		announce->header.txn = htonl(rand());
		announce->header.payloadLength = sizeof(lichtenstein_node_announcement_t);
		announce->header.payloadLength -= sizeof(lichtenstein_header_t);
		announce->header.payloadLength += nameLen;

		size_t payloadLen = announce->header.payloadLength;
		announce->header.payloadLength = htonl(announce->header.payloadLength);


		// configure the payload
		announce->swVersion = htonl(0x00001000);
		announce->hwVersion = htonl(0x00001000);

		memcpy(announce->macAddr, testMac, 6);

		announce->port = htons(7420);
		announce->ip = htonl(0xFFFFFFFF);

		announce->fbSize = htonl((300 * 4 * 2));
		announce->channels = htons(2);

		announce->hostnameLen = htons(nameLen);
		strncpy((char *) announce->hostname, name, announce->hostnameLen);

		// calculate CRC
		size_t offset = offsetof(lichtenstein_node_announcement_t, header.opcode);
		void *ptr = ((uint8_t *) announce) + offset;
		size_t len = sizeof(lichtenstein_header_t) - offset + payloadLen;

		printf("Calculating CRC starting at byte %lu\n", offset);
		uint32_t crc = crc32_fast(ptr, len, 0);

		printf("Total packet length %lu, CRC32 = 0x%08x\n", totalPacketLen, crc);

		announce->header.checksum = htonl(crc);

		// send the packet
		puts("Sending multicast packet...");

		if(sendto(fd, announce, totalPacketLen, 0, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			perror("sendto");
			exit(1);
		}

		sleep(10);
	}
}
