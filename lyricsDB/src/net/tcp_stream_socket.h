#pragma once

#include "stream_socket.h"

#include <string>

class with_descriptor {
protected:
	with_descriptor(int descriptor);

	with_descriptor(with_descriptor const &) = delete;
	with_descriptor & operator=(with_descriptor const &) = delete;

	with_descriptor(with_descriptor && other);
	with_descriptor & operator=(with_descriptor && other);

	~with_descriptor();

	bool is_valid() const;

protected:
	int m_descriptor;
};

///////////////////////////////////////////////////////////////////////////////

class tcp_stream_client_socket: public stream_client_socket, public with_descriptor {
public:
	explicit tcp_stream_client_socket(int sockfd);

	tcp_stream_client_socket(std::string const & hostname, uint16_t port);

	void connect() override;

	void send(void const * buf, size_t size) override;
	void recv(void * buf, size_t size) override;

private:
	std::string m_hostname;
	uint16_t m_port;
};

///////////////////////////////////////////////////////////////////////////////

class tcp_stream_server_socket: public stream_server_socket, public with_descriptor {
public:
	explicit tcp_stream_server_socket(std::string const & hostname, uint16_t port);

	socket_ptr accept_one_client() override;
};
