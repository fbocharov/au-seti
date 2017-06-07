#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <stdexcept>

/*
 * All the functions in the interface are blocking.
 */

struct socket_exception: public std::runtime_error {
	socket_exception(std::string const & what)
		: std::runtime_error(what)
	{}
};

struct stream_socket
{
	virtual ~stream_socket() = default;
	/*
	 * Either sends all the data in buf or throws.
	 * If exception is thrown then this socket would probably
	 * throw them on all further sends.
	 * Also in case of exception there is no guarantee about
	 * the portion of data that has reached the other endpoint
	 * and that has been processed by it.
	 * You can't call this method in >1 threads simultaneously
	 * - locking required;
	 */
	virtual void send(void const * buf, size_t size) = 0;
	/*
	 * Either reads size bytes of data into buf or throws.
	 * Buf may be modified even if exception was thrown.
	 * If exception is thrown then this socket would probably
	 * throw them on all further recvs.
	 * You can't call this method in >1 threads simultaneously
	 * - locking required;
	 */
	virtual void recv(void * buf, size_t size) = 0;
};
using socket_ptr = std::shared_ptr<stream_socket>;

struct stream_client_socket: stream_socket
{
	/*
	 * If exception is thrown then this socket would probably
	 * throw them on all further connects.
	 */
	virtual void connect() = 0;
};
using client_socket_ptr = std::shared_ptr<stream_client_socket>;

struct stream_server_socket
{
	virtual ~stream_server_socket() = default;
	/*
	 * If exception is thrown then this socket would probably
	 * throw them on all further accepts.
	 */
	virtual socket_ptr accept_one_client() = 0;
};
using server_socket_ptr = std::shared_ptr<stream_server_socket>;

///////////////////////////////////////////////////////////////////////////////

client_socket_ptr make_client_socket(
	std::string const& hostname,
	uint16_t port,
	bool connect = false);

server_socket_ptr make_server_socket(std::string const & hostname, uint16_t port);
