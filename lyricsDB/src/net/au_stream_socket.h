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

class Connection;
using ConnectionPtr = std::shared_ptr<Connection>;

struct AddrInfo {
	uint16_t m_localPort;

	sockaddr m_remoteAddr;
	uint16_t m_remotePort;
};

///////////////////////////////////////////////////////////////////////////////

class au_stream_client_socket: public stream_client_socket, public with_descriptor {
public:
	au_stream_client_socket(
		sockaddr const& remoteAddr,
		uint16_t remotePort,
		uint16_t localPort,
		bool connected = true);
	au_stream_client_socket(std::string const & hostname, uint16_t port);
	~au_stream_client_socket();

	void connect() override;

	void send(void const * buf, size_t size) override;
	void recv(void * buf, size_t size) override;

private:
	AddrInfo m_address;
	ConnectionPtr m_connection;
};

///////////////////////////////////////////////////////////////////////////////

class au_stream_server_socket: public stream_server_socket, public with_descriptor {
public:
	explicit au_stream_server_socket(std::string const & hostname, uint16_t port);

	socket_ptr accept_one_client() override;
};

