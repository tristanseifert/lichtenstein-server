/**
 * Various utilities for dealing with Lichtenstein packets, including checksum
 * verification.
 */
#ifndef LICHTENSTEINUTILS_H
#define LICHTENSTEINUTILS_H

#include <cstdint>
#include <cstddef>

class LichtensteinUtils {
	public:
		enum PacketErrors {
			kNoError = 0,

			kInvalidChecksum = 0x100,
			kPacketTooSmall,
			kInvalidMagic
		};

	public:
		static PacketErrors validatePacket(void *data, size_t length);
		static void convertToHostByteOrder(void *data, size_t length);

		static bool validatePacketSimple(void *data, size_t length) {
			return (validatePacket(data, length) == kNoError);
		}

	private:
		static void _convertToHostNodeAnnouncement(void *data, size_t length);
};

#endif
