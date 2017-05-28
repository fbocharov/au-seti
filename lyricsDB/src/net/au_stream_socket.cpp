#include "au_stream_socket.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <limits>
#include <list>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>

#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

///////////////////////////////////////////////////////////////////////////////

constexpr auto MAX_TIMEOUT = 5000ull; // 5s
constexpr auto MIN_TIMEOUT = 10ull; // 1ms
constexpr auto DEFAULT_TIMEOUT = MIN_TIMEOUT;

constexpr auto MAX_SOCKET_COUNT = std::numeric_limits<uint16_t>::max();

#define IPPROTO_MYCP 192

///////////////////////////////////////////////////////////////////////////////

int create_socket(bool nonblock = true)
{
	int type = SOCK_RAW;
	if (nonblock)
		type |= SOCK_NONBLOCK;
	return socket(AF_INET, type, IPPROTO_MYCP);
}

sockaddr create_addr(std::string const & hostname)
{
	sockaddr addr;
	// TODO: implement me!!!
	return addr;
}

uint16_t get_random_port()
{
	static uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<uint16_t> distribution;
	return distribution(generator);
}

uint32_t calculate_checksum(MyCPDataPacket const& packet)
{
	uint8_t * bytes = (uint8_t *)&packet;
	uint32_t checksum = 0;

	for (size_t i = 0; i < sizeof(packet); ++i)
		  checksum += *bytes++;
	return checksum;
}

///////////////////////////////////////////////////////////////////////////////

class Receiver {
	const uint32_t STOP_EVENT_ID = std::numeric_limits<uint32_t>::max();
	const uint32_t CONN_EVENT_ID = STOP_EVENT_ID - 1;

public:
	Receiver()
		: m_pollFd(epoll_create(1))
		, m_stopFd(eventfd(1, EFD_NONBLOCK))
		, m_connectionsFd(eventfd(1, EFD_NONBLOCK))
	{
		if (m_pollFd < 0)
			throw socket_exception("failed to create epoll descriptor");
		if (m_stopFd < 0 || m_connectionsFd < 0)
			throw socket_exception("failed to create control descriptor");

		add_read_socket(STOP_EVENT_ID, m_stopFd);
		add_read_socket(CONN_EVENT_ID, m_connectionsFd);

		std::thread t(std::bind(&Receiver::receive_loop, this));
		t.detach();
	}

	~Receiver()
	{
		notify(m_stopFd);
	}

	void register_connection(uint32_t id, ConnectionPtr connection)
	{
		add_read_socket(id, connection->m_socket);

		std::lock_guard<std::mutex> lock(m_connectionsGuard);
		m_connections[id] = connection;
		notify(m_connectionsFd);
	}

	void unregister_connection(uint32_t id)
	{
		// XXX: called after closing connection, no need to delete it from epoll
		std::lock_guard<std::mutex> lock(m_connectionsGuard);
		m_connections.erase(id);
		notify(m_connectionsFd);
	}

private:
	void add_read_socket(uint32_t id, int sockfd)
	{
		epoll_event event = {0};
		event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
		event.data.u32 = id;
		if (epoll_ctl(m_pollFd, EPOLL_CTL_ADD, sockfd, &event) < 0)
			throw socket_exception("failed to add connection to epoll");
	}

	void receive_loop()
	{
		while (true) {
			std::vector<epoll_event> events(MAX_SOCKET_COUNT);
			int readyEvents = epoll_wait(m_pollFd, &events[0], MAX_SOCKET_COUNT, -1);
			if (readyEvents < 0)
				throw socket_exception(std::string("error while polling sockets: ") + strerror(errno));

			std::lock_guard<std::mutex> lock(m_connectionsGuard);
			for (int i = 0; i < readyEvents; ++i) {
				auto id = events[i].data.u32;
				if (id == STOP_EVENT_ID)
					return;
				if (id == CONN_EVENT_ID)
					// connections have changed, will detect it on next iteration of while
					continue;

				auto it = m_connections.find(id);
				if (it == m_connections.end())
					// message for unknown connection, just drop it
					continue;
				auto connection = it->second;

				std::lock_guard<std::mutex> bufferLock(connection->m_bufferGuard);
				if (!connection->m_buffer.has_free_space())
					// no place to put new data, do nothing
					continue;

				uint64_t prevMaxNum = connection->m_buffer.get_max_packet_number();
				uint64_t maxNum = prevMaxNum;
				while (connection->m_buffer.has_free_space()) {
					// read all packets from this socket and try to sent ack
					MyCPDataPacket packet;
					// TODO: receive packet

					connection->m_buffer.add_packet(packet);
					maxNum = connection->m_buffer.get_max_packet_number();
				}

				if (prevMaxNum != maxNum) {
					// TODO: send ack for maxNum
					connection->m_readEvent.notify_one();
				}
			}
		}
	}

	void notify(int descriptor)
	{
		uint8_t num = 1;
		write(descriptor, &num, sizeof(num));
	}

private:
	int m_pollFd;
	int m_stopFd;
	int m_connectionsFd;

	std::unordered_map<uint32_t, ConnectionPtr> m_connections;
	std::mutex m_connectionsGuard;
};

Receiver receiver;

} // namespace

///////////////////////////////////////////////////////////////////////////////

bool ReceiveBuffer::empty() const
{
	return true;
}

bool ReceiveBuffer::has_free_space() const
{
	return false;
}

uint64_t ReceiveBuffer::get_max_packet_number() const
{
	return 0;
}

void ReceiveBuffer::add_packet(MyCPDataPacket & packet)
{
	(void) packet;
}

size_t ReceiveBuffer::read(void * buf, size_t size)
{
	(void) buf;
	(void) size;
	return 0;
}


///////////////////////////////////////////////////////////////////////////////

au_stream_client_socket::au_stream_client_socket(
		sockaddr const & removeAddr,
		uint16_t remotePort,
		uint16_t localPort)
	: with_descriptor(create_socket())
	, m_connection(std::make_shared<Connection>())
{
	m_connection->m_socket = m_descriptor;
	m_connection->m_localPort = localPort;

	m_connection->m_remoteAddr = removeAddr;
	m_connection->m_remotePort = remotePort;
}

au_stream_client_socket::au_stream_client_socket(std::string const & hostname, uint16_t port)
	: au_stream_client_socket(create_addr(hostname), port, get_random_port())
{}

au_stream_client_socket::~au_stream_client_socket()
{
	// TODO: send close message
	close(m_connection->m_socket);
	receiver.unregister_connection(m_connection->m_socket);
}

void au_stream_client_socket::connect()
{
	// TODO: connect
}

void au_stream_client_socket::send(void const * buf, size_t size)
{
	uint16_t window = MYCP_MAX_WINDOW_SIZE;
	uint16_t maxSentPacketsCount = 0;

	size_t packedOffset = 0;
	std::list<MyCPDataPacket> notAckedPackets;
	while (packedOffset < size && !notAckedPackets.empty()) {
		if (notAckedPackets.empty()) {
			maxSentPacketsCount += 1;
		} else {
			maxSentPacketsCount /= 2;
		}

		uint16_t sentPacketCount = 0;
		while (window > 0 && sentPacketCount <= maxSentPacketsCount) {
			size_t packetSize = window > MYCP_MAX_DATA_SIZE ? MYCP_MAX_DATA_SIZE : window;
			auto packet = create_packet((uint8_t*) buf + packedOffset, packetSize);
			notAckedPackets.push_back(packet);

			packedOffset += packetSize;
			window -= packetSize;
			sentPacketCount += 1;
		}

		for (auto const& packet: notAckedPackets) {
			(void) packet;
			// TODO: send packet
		}

		uint64_t maxAck = 0;
		{
			std::unique_lock<std::mutex> lock(m_connection->m_acksGuard);
			std::cv_status st = std::cv_status::no_timeout;
			while (m_connection->m_acks.empty())
				st = m_connection->m_ackEvent.wait_for(lock, std::chrono::milliseconds(m_connection->m_timeout));

			if (st == std::cv_status::no_timeout) {
				for (auto const& ack: m_connection->m_acks) {
					if (ack.m_header.m_packetNumber > maxAck) {
						maxAck = ack.m_header.m_packetNumber;
						window = std::max(window, ack.m_window);
					}
				}
			}
		}
		for (auto it = notAckedPackets.begin(); it != notAckedPackets.end(); )
			if (it->m_header.m_packetNumber <= maxAck)
				it = notAckedPackets.erase(it);
			else
				++it;
	}
}

void au_stream_client_socket::recv(void * buf, size_t size)
{
	std::unique_lock<std::mutex> lock(m_connection->m_bufferGuard);

	uint8_t * data = (uint8_t *) buf;
	while (size > 0) {
		while (m_connection->m_buffer.empty())
			m_connection->m_readEvent.wait(lock);

		size_t readSize = m_connection->m_buffer.read(data, size);
		size -= readSize;
		data += readSize;
	}
}

void au_stream_client_socket::initialize_connection(uint16_t port)
{
	m_connection->m_socket = m_descriptor;
	m_connection->m_localPort = port;
	m_connection->m_timeout = DEFAULT_TIMEOUT;

	receiver.register_connection(m_connection->m_socket, m_connection);
}

MyCPDataPacket au_stream_client_socket::create_packet(void const * buf, size_t size)
{
	if (size > MYCP_MAX_DATA_SIZE)
		throw socket_exception("packet data size is too big");

	MyCPDataPacket packet;

	packet.m_header.m_srcPort = m_connection->m_localPort;
	packet.m_header.m_dstPort = m_connection->m_remotePort;
	packet.m_header.m_type = MyCPMessageType::MyCP_DATA;
	packet.m_header.m_packetNumber = m_connection->m_nextPacketNumber++;

	memcpy(packet.m_data, buf, size);
	packet.m_size = size;

	packet.m_header.m_checksum = calculate_checksum(packet);

	return packet;
}

///////////////////////////////////////////////////////////////////////////////

au_stream_server_socket::au_stream_server_socket(std::string const & hostname, uint16_t port)
	: with_descriptor(create_socket(false))
{}

socket_ptr au_stream_server_socket::accept_one_client()
{
	// TODO: implement me!!
	return nullptr;
}
