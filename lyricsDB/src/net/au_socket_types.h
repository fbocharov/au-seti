#pragma once

#include <limits>

#include <cstdint>

enum class MyCPMessageType: uint8_t {
	MyCP_SYN,
	MyCP_SYNACK,
	MyCP_ACK,
	MyCP_DATA,
	MyCP_CLOSE
};

struct __attribute__((packed)) MyCPHeader {
	MyCPMessageType m_type;

	uint16_t m_srcPort;
	uint16_t m_dstPort;

	uint32_t m_checksum; // 0 when evaluated
	uint64_t m_packetNumber;
};

constexpr auto MYCP_MAX_WINDOW_SIZE = std::numeric_limits<uint16_t>::max();

struct __attribute__((packed)) MyCPAckPacket {
	MyCPHeader m_header;
	uint16_t m_window;
};

constexpr auto MYCP_MAX_DATA_SIZE = 2 * 1024u;

struct __attribute__((packed)) MyCPDataPacket {
	MyCPHeader m_header;

	uint16_t m_size;
	uint8_t m_data[MYCP_MAX_DATA_SIZE];
};
