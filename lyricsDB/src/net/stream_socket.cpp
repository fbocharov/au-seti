#include "stream_socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <iostream>

constexpr auto TCP_SERVER_SOCKET_BACKLOG_LENGTH = 128;

namespace {

class with_descriptor {
protected:
	with_descriptor(int descriptor)
		: m_descriptor(descriptor)
	{}

	~with_descriptor()
	{
		if (is_valid())
			close(m_descriptor);
	}

	bool is_valid() const
	{
		return m_descriptor >= 0;
	}

protected:
	int m_descriptor;
};

void throw_errno(std::string const & msg)
{
	throw socket_exception(msg + ": " + strerror(errno));
}

sockaddr_in create_addr(
	std::string const & hostname,
	uint16_t port,
	bool passive)
{
	addrinfo hints = { 0 };
	addrinfo * result = nullptr;

	hints.ai_addr = nullptr;
	hints.ai_canonname = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_flags = passive ? AI_PASSIVE : 0;
	hints.ai_next = nullptr;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_socktype = SOCK_STREAM;

	auto portStr = std::to_string(port);
	int ret = getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &result);
	if (ret)
		throw socket_exception(std::string("failed to get addr info: ") + gai_strerror(ret));

	sockaddr_in addr;
	if (result->ai_addr)
		// pick first address
		memcpy(&addr, result->ai_addr, result->ai_addrlen);
	else {
		freeaddrinfo(result);
		throw socket_exception("addr info is null");
	}
	freeaddrinfo(result);

	memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

	return addr;
}

} // namespace

class tcp_stream_client_socket: public stream_client_socket, public with_descriptor {
public:
	explicit tcp_stream_client_socket(int sockfd)
		: with_descriptor(sockfd)
	{
		if (!is_valid())
			throw_errno("invalid socket descriptor");
	}

	tcp_stream_client_socket(std::string const & hostname, uint16_t port)
		: with_descriptor(-1)
		, m_hostname(hostname)
		, m_port(port)
	{}

	void send(void const * buf, size_t size) override
	{
		if (!is_valid())
			throw socket_exception("socket not connected");

		ssize_t sendSize = ::send(m_descriptor, buf, size, 0);
		if (sendSize < 0 || size_t(sendSize) != size)
			throw_errno("failed to send " + std::to_string(size) + " bytes");
	}

	void recv(void * buf, size_t size) override
	{
		if (!is_valid())
			throw socket_exception("socket not connected");

		ssize_t recvSize = ::recv(m_descriptor, buf, size, 0);
		if (recvSize < 0 || size_t(recvSize) != size)
			throw_errno("failed to recv " + std::to_string(size) + " bytes");
	}

	void connect() override
	{
		if (is_valid())
			throw socket_exception("socket already connected");

		m_descriptor = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!is_valid())
			throw_errno("failed to create socket");
		int option = 1;
		if (setsockopt(m_descriptor, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
			throw_errno("faild to set socket options");
		}

		sockaddr_in addr = create_addr(m_hostname, m_port, false);
		if (::connect(m_descriptor, (sockaddr *) &addr, sizeof(addr)) < 0)
			throw_errno("failed to connect to host");
	}

private:
	std::string m_hostname;
	uint16_t m_port;
};

class tcp_stream_server_socket: public stream_server_socket, public with_descriptor {
public:
	explicit tcp_stream_server_socket(std::string const & hostname, uint16_t port)
		: with_descriptor(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
	{
		if (!is_valid())
			throw_errno("failed to create socket");

		sockaddr_in addr = create_addr(hostname, port, true);
		if (::bind(m_descriptor, (sockaddr *) &addr, sizeof(addr)) < 0)
			throw_errno("failed to bind socket");

		if (::listen(m_descriptor, TCP_SERVER_SOCKET_BACKLOG_LENGTH) < 0)
			throw_errno("failed to start listen");
	}

	socket_ptr accept_one_client() override
	{
		int client = ::accept(m_descriptor, nullptr, nullptr);
		if (client < 0)
			throw_errno("failed to accept client");
		return std::make_shared<tcp_stream_client_socket>(client);
	}
};

///////////////////////////////////////////////////////////////////////////////

client_socket_ptr make_client_socket(
	std::string const& hostname,
	uint16_t port,
	bool connect)
{
	auto socket = std::make_shared<tcp_stream_client_socket>(hostname, port);
	if (connect) {
		socket->connect();
	}
	return socket;
}

server_socket_ptr make_server_socket(std::string const & hostname, uint16_t port)
{
	return std::make_shared<tcp_stream_server_socket>(hostname, port);
}
