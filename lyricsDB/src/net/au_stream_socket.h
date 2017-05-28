#pragma once

#include "au_socket_types.h"
#include "stream_socket.h"

#include <common/with_descriptor.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/socket.h>

///////////////////////////////////////////////////////////////////////////////

class ReceiveBuffer {
public:
	bool empty() const;
	bool has_free_space() const;
	uint64_t get_max_packet_number() const;

	void add_packet(MyCPDataPacket & packet);
	size_t read(void * buf, size_t size);
};

///////////////////////////////////////////////////////////////////////////////

struct Connection {
	int m_socket;
	uint16_t m_localPort;
	sockaddr m_remoteAddr;
	uint16_t m_remotePort;

	std::atomic_ullong m_nextPacketNumber;

	ReceiveBuffer m_buffer;
	std::mutex m_bufferGuard;
	std::condition_variable m_readEvent;

	uint64_t m_timeout; // in ms

	std::vector<MyCPAckPacket> m_acks;
	std::mutex m_acksGuard;
	std::condition_variable m_ackEvent;
};
using ConnectionPtr = std::shared_ptr<Connection>;

///////////////////////////////////////////////////////////////////////////////

class au_stream_client_socket: public stream_client_socket, public with_descriptor {
public:
	au_stream_client_socket(sockaddr const& removeAddr, uint16_t remotePort, uint16_t localPort);
	au_stream_client_socket(std::string const & hostname, uint16_t port);
	~au_stream_client_socket();

	void connect() override;

	void send(void const * buf, size_t size) override;
	void recv(void * buf, size_t size) override;

private:
	void initialize_connection(uint16_t port);

	MyCPDataPacket create_packet(void const * buf, size_t size);

private:
	ConnectionPtr m_connection;
};

///////////////////////////////////////////////////////////////////////////////

class au_stream_server_socket: public stream_server_socket, public with_descriptor {
public:
	explicit au_stream_server_socket(std::string const & hostname, uint16_t port);

	socket_ptr accept_one_client() override;
};

