#include "au_stream_socket.h"
#include "tcp_stream_socket.h"
#include "stream_socket.h"

#include <cstdlib>

namespace {

bool au_socket_enabled() {
	auto sockType = std::getenv("STREAM_SOCK_TYPE");
	return sockType && sockType == std::string("au");
}

} // namespace

///////////////////////////////////////////////////////////////////////////////

client_socket_ptr make_client_socket(
	std::string const& hostname,
	uint16_t port,
	bool connect)
{
	client_socket_ptr socket;
	if (au_socket_enabled()) {
		socket = client_socket_ptr(new au_stream_client_socket(hostname, port));
	} else {
		socket = client_socket_ptr(new tcp_stream_client_socket(hostname, port));
	}

	if (connect) {
		socket->connect();
	}
	return socket;
}

server_socket_ptr make_server_socket(std::string const & hostname, uint16_t port)
{
	return au_socket_enabled()
			? server_socket_ptr(new au_stream_server_socket(hostname, port))
			: server_socket_ptr(new tcp_stream_server_socket(hostname, port));
}
