#include "tcp_stream_socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>

#include <algorithm>
#include <cstring>

constexpr auto TCP_SERVER_SOCKET_BACKLOG_LENGTH = 128;

namespace {

void throw_errno(std::string const & msg)
{
	throw socket_exception(msg + ": " + strerror(errno));
}

sockaddr_in create_addr(
	std::string const & hostname,
	uint16_t port,
	bool passive)
{
	addrinfo hints;
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


with_descriptor::with_descriptor(int descriptor)
	: m_descriptor(descriptor)
{}

with_descriptor::with_descriptor(with_descriptor && other)
{
	m_descriptor = other.m_descriptor;
	other.m_descriptor = -1;
}

with_descriptor & with_descriptor::operator=(with_descriptor && other)
{
	std::swap(m_descriptor, other.m_descriptor);
	return *this;
}

with_descriptor::~with_descriptor()
{
	if (is_valid())
		close(m_descriptor);
}

bool with_descriptor::is_valid() const
{
	return m_descriptor >= 0;
}

///////////////////////////////////////////////////////////////////////////////

tcp_stream_client_socket::tcp_stream_client_socket(int sockfd)
	: with_descriptor(sockfd)
{
	if (!is_valid())
		throw_errno("invalid socket descriptor");
}

tcp_stream_client_socket::tcp_stream_client_socket(std::string const & hostname, uint16_t port)
	: with_descriptor(-1)
	, m_hostname(hostname)
	, m_port(port)
{}

void tcp_stream_client_socket::send(void const * buf, size_t size)
{
	if (!is_valid())
		throw socket_exception("socket not connected");

	ssize_t sendSize = ::send(m_descriptor, buf, size, MSG_NOSIGNAL);
	if (sendSize < 0 || size_t(sendSize) != size)
		throw_errno("failed to send " + std::to_string(size) + " bytes");
}

void tcp_stream_client_socket::recv(void * buf, size_t size)
{
	if (!is_valid())
		throw socket_exception("socket not connected");

	ssize_t recvSize = ::recv(m_descriptor, buf, size, MSG_NOSIGNAL);
	if (recvSize < 0 || size_t(recvSize) != size)
		throw_errno("failed to recv " + std::to_string(size) + " bytes");
}

void tcp_stream_client_socket::connect()
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

///////////////////////////////////////////////////////////////////////////////

tcp_stream_server_socket::tcp_stream_server_socket(std::string const & hostname, uint16_t port)
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

socket_ptr tcp_stream_server_socket::accept_one_client()
{
	int client = ::accept(m_descriptor, nullptr, nullptr);
	if (client < 0)
		throw_errno("failed to accept client");
	return socket_ptr(new tcp_stream_client_socket(client));
}
