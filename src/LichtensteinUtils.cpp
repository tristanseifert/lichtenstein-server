#include "LichtensteinUtils.h"

#include <iostream>

#include <glog/logging.h>

#include "lichtenstein_proto.h"
#include "crc32/crc32.h"

using namespace std;

/**
 * Validates the given packet. If the packet cannot be verified, an error is
 * returned. This assumes that the packet is right off the wire, i.e. that all
 * multibyte values are still in network order.
 */
LichtensteinUtils::PacketErrors LichtensteinUtils::validatePacket(void *data, size_t length) {
	lichtenstein_header_t *header = static_cast<lichtenstein_header_t *>(data);

	// validate packet length
	if(length < sizeof(lichtenstein_header_t)) {
		LOG(WARNING) << "attempted to convert a packet smaller than the header";
		return kPacketTooSmall;
	}

	// verify the checksum, if requested
	if(header->flags & htons(kFlagChecksummed)) {
		size_t checksummedDataStart = offsetof(lichtenstein_header_t, opcode);
		void *checksummedData = static_cast<uint8_t *>(data) + checksummedDataStart;
		size_t checksummedLength = (length - checksummedDataStart);

		uint32_t crc = crc32_fast(checksummedData, checksummedLength);
		uint32_t packetCrc = ntohl(header->checksum);

		LOG_IF(WARNING, crc != packetCrc) << "CRC mismatch on packet 0x" << data
										  << "! Got " << packetCrc << ", expected"
										  << crc;

		if(crc != packetCrc) {
			return kInvalidChecksum;
		}
	}

	// validate the header magic value
	if(header->magic != htonl(kLichtensteinMagic)) {
		LOG(WARNING) << "invalid magic value, got 0x" << hex << ntohl(header->magic);
		return kInvalidMagic;
	}

	// if we get down here, the packet must be valid. neat!
	return kNoError;
}

/**
 * Converts the packet to host byte order, in-place.
 */
void LichtensteinUtils::convertToHostByteOrder(void *data, size_t length) {
	lichtenstein_header_t *header = static_cast<lichtenstein_header_t *>(data);

	if(length < sizeof(lichtenstein_header_t)) {
		LOG(WARNING) << "attempted to convert a packet smaller than the header";
		return;
	}

	// convert header fields
	header->magic = ntohl(header->magic);
	header->version = ntohl(header->version);
	header->checksum = ntohl(header->checksum);

	header->opcode = ntohs(header->opcode);
	header->flags = ntohs(header->flags);

	header->sequenceIndex = ntohs(header->sequenceIndex);
	header->sequenceNumPackets = ntohs(header->sequenceNumPackets);

	header->txn = ntohl(header->txn);
	header->payloadLength = ntohl(header->payloadLength);

	// now, convert the packet's values
	switch(header->opcode) {
		case kOpcodeNodeAnnouncement:
			LichtensteinUtils::_convertToHostNodeAnnouncement(data, length);
			break;
	}
}

/**
 * Converts the fields in the node announcement packet to the host byte order.
 */
void LichtensteinUtils::_convertToHostNodeAnnouncement(void *data, size_t length) {
	lichtenstein_node_announcement_t *packet = static_cast<lichtenstein_node_announcement_t *>(data);

	// verify the minimum size
	size_t minPayloadSz = sizeof(lichtenstein_node_announcement_t) - sizeof(lichtenstein_header_t);

	if(minPayloadSz > packet->header.payloadLength) {
		LOG(ERROR) << "node announcement packet is too small";
		return;
	}

	// convert the fields
	packet->swVersion = ntohl(packet->swVersion);
	packet->hwVersion = ntohl(packet->hwVersion);

	packet->port = ntohs(packet->port);
	packet->ip = ntohl(packet->ip);

	packet->fbSize = ntohl(packet->fbSize);
	packet->channels = ntohs(packet->channels);

	packet->numGpioDigitalIn = ntohs(packet->numGpioDigitalIn);
	packet->numGpioDigitalOut = ntohs(packet->numGpioDigitalOut);
	packet->numGpioAnalogIn = ntohs(packet->numGpioAnalogIn);
	packet->numGpioAnalogOut = ntohs(packet->numGpioAnalogOut);

	packet->hostnameLen = ntohs(packet->hostnameLen);
}
