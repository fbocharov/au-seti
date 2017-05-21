#include "au_stream_socket.h"
#include "tcp_stream_socket.h"
#include "stream_socket.h"

client_socket_ptr make_client_socket(
	std::string const& hostname,
	uint16_t port,
	bool connect)
{
	auto socket = client_socket_ptr(new tcp_stream_client_socket(hostname, port));
	if (connect) {
		socket->connect();
	}
	return socket;
}

server_socket_ptr make_server_socket(std::string const & hostname, uint16_t port)
{
	return server_socket_ptr(new tcp_stream_server_socket(hostname, port));
}
