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
#include <queue>

#include <cassert>
#include <cstring>
#include <netdb.h>
#include <netinet/ip.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

namespace {

///////////////////////////////////////////////////////////////////////////////

constexpr auto MTU_SIZE = 1500;

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

sockaddr create_addr(std::string const & hostname, uint16_t port)
{
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;

	addrinfo * result = nullptr;
	auto portStr = std::to_string(port);
	int ret = getaddrinfo(hostname.c_str(), portStr.c_str(), &hints, &result);
	if (ret)
		throw socket_exception(std::string("failed to get addr info: ") + gai_strerror(ret));

	sockaddr addr;
	if (result->ai_addr)
		// pick first address
		memcpy(&addr, result->ai_addr, result->ai_addrlen);
	else {
		freeaddrinfo(result);
		throw socket_exception("addr info is null");
	}
	freeaddrinfo(result);

	return addr;
}

uint16_t get_random_port()
{
	static uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
	std::default_random_engine generator(seed);
	std::uniform_int_distribution<uint16_t> distribution;
	return distribution(generator);
}

uint32_t calculate_checksum(void const * packet, size_t size)
{
	uint8_t * bytes = (uint8_t *) packet;
	uint32_t checksum = 0;

	for (size_t i = 0; i < size; ++i)
		  checksum += *bytes++;
	return checksum;
}

uint64_t get_now()
{
	auto now = std::chrono::steady_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

///////////////////////////////////////////////////////////////////////////////

struct PacketIO {
	static bool send(int socket, AddrInfo const & addr, MyCPDataPacket & packet)
	{
		packet.m_header.m_srcPort = addr.m_localPort;
		packet.m_header.m_dstPort = addr.m_remotePort;

		packet.m_header.m_size = sizeof(packet);
		packet.m_header.m_headerChecksum = calculate_checksum(&packet.m_header, sizeof(packet.m_header));
		packet.m_header.m_bodyChecksum = calculate_checksum(packet.m_data, MYCP_MAX_DATA_SIZE);
		packet.m_header.m_bodyChecksum += calculate_checksum(&packet.m_size, sizeof(packet.m_size));

		return do_send(socket, addr, &packet, sizeof(packet));
	}

	static bool send(int socket, AddrInfo const & addr, MyCPAckPacket & ack)
	{
		ack.m_header.m_srcPort = addr.m_localPort;
		ack.m_header.m_dstPort = addr.m_remotePort;

		ack.m_header.m_size = sizeof(ack);
		ack.m_header.m_headerChecksum = calculate_checksum(&ack.m_header, sizeof(ack.m_header));
		ack.m_header.m_bodyChecksum += calculate_checksum(&ack.m_window, sizeof(ack.m_window));

		return do_send(socket, addr, &ack, sizeof(ack));
	}

	static bool send(int socket, AddrInfo const & addr, MyCPHeader & hdr)
	{
		hdr.m_srcPort = addr.m_localPort;
		hdr.m_dstPort = addr.m_remotePort;

		hdr.m_size = sizeof(hdr);
		hdr.m_headerChecksum = calculate_checksum(&hdr, sizeof(hdr));
		hdr.m_bodyChecksum = 0;

		return do_send(socket, addr, &hdr, sizeof(hdr));

	}

	static bool receiveAny(
		int socket,
		uint16_t port,
		sockaddr * remoteAddr,
		MyCPHeader * header,
		uint8_t * bytes)
	{
		return false;
	}

	static bool receive(
		int socket,
		AddrInfo const & addr,
		MyCPHeader * header,
		uint8_t * bytes)
	{
		char buf[MTU_SIZE];
		sockaddr recvAddr;
		socklen_t recvAddrLen;
		while (true) {
			ssize_t recvSize = recvfrom(socket, buf, MTU_SIZE, 0, &recvAddr, &recvAddrLen);
			if (recvSize < 0) {
				if (EAGAIN == errno || EWOULDBLOCK == errno)
					return false;
				throw socket_exception("failed to receive data: " + std::string(strerror(errno)));
			}

			sockaddr_in * expected = (sockaddr_in *) &addr.m_remoteAddr;
			sockaddr_in * actual = (sockaddr_in *) &addr.m_remoteAddr;
			if (expected->sin_addr.s_addr != actual->sin_addr.s_addr)
				continue;

			size_t ipSize = sizeof(ip);
			memcpy(bytes, buf + ipSize, MTU_SIZE - ipSize);
			header = (MyCPHeader *) bytes;
			if (header->m_dstPort != addr.m_localPort)
				continue;

			if (!check_integrity(header, bytes))
				continue;

			return true;
		}
	}

private:
	static bool do_send(int socket, AddrInfo const & addr, void * bytes, size_t size)
	{
		ssize_t ret = sendto(socket, bytes, size, 0, &addr.m_remoteAddr, sizeof(addr.m_remoteAddr));
		if (ret < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return false;
			throw socket_exception("failed to send data: " + std::string(strerror(errno)));
		}

		return size_t(ret) == size;
	}

	static bool do_receive(
			int socket,
			uint16_t port,
			sockaddr * addr,
			MyCPHeader * header,
			uint8_t * bytes)
	{
		char buf[MTU_SIZE];
		socklen_t addrLen;
		ssize_t recvSize = recvfrom(socket, buf, MTU_SIZE, 0, addr, &addrLen);
		if (recvSize < 0) {
			if (EAGAIN == errno || EWOULDBLOCK == errno)
				return false;
			throw socket_exception("failed to receive data: " + std::string(strerror(errno)));
		}

		size_t ipSize = sizeof(ip);
		memcpy(bytes, buf + ipSize, MTU_SIZE - ipSize);
		header = (MyCPHeader *) bytes;

		return header->m_dstPort == port && check_integrity(header, bytes);
	}

	static bool check_integrity(MyCPHeader * header, uint8_t * bytes)
	{
		uint32_t headerChecksum = header->m_headerChecksum;
		uint32_t bodyChecksum = header->m_bodyChecksum;
		header->m_headerChecksum = 0;
		header->m_bodyChecksum = 0;

		if (headerChecksum != calculate_checksum(header, sizeof(*header)))
			return false;

		uint8_t * body = bytes + sizeof(*header);
		if (bodyChecksum != calculate_checksum(body, header->m_size - sizeof(*header)))
			return false;

		header->m_headerChecksum = headerChecksum;
		header->m_bodyChecksum = bodyChecksum;

		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////

class ReceiveBuffer {
public:
	explicit ReceiveBuffer(uint32_t maxSize)
		: m_nextPacket(0)
		, m_packetOffset(0)
		, m_maxSize(maxSize)
	{}

	bool empty() const
	{
		return m_packets.end() == m_packets.find(m_nextPacket);
	}

	bool has_free_space() const
	{
		return m_packets.size() < m_maxSize;
	}

	uint16_t get_free_space() const
	{
		return (m_maxSize - m_packets.size()) * MYCP_MAX_DATA_SIZE;
	}

	void add_packet(MyCPDataPacket & packet)
	{
		if (m_packets.find(packet.m_header.m_packetNumber) == m_packets.end())
			m_packets[packet.m_header.m_packetNumber] = packet;
	}

	size_t read(void * buf, size_t size)
	{
		size_t readSize = 0;
		uint8_t * data = (uint8_t *) buf;
		auto it = m_packets.find(m_nextPacket);
		while (it != m_packets.end() && readSize < size) {
			size_t leftInPacket = MYCP_MAX_DATA_SIZE - m_packetOffset;
			size_t needRead = size > leftInPacket ? leftInPacket : size;
			memcpy(data, it->second.m_data, needRead);

			data += needRead;
			size -= needRead;
			m_packetOffset = (m_packetOffset + needRead) % MYCP_MAX_DATA_SIZE;

			if (!m_packetOffset) {
				// read full packet
				m_packets.erase(m_nextPacket);
				it = m_packets.find(++m_nextPacket);
			}
		}

		return readSize;
	}

private:
	std::unordered_map<uint64_t, MyCPDataPacket> m_packets;
	uint64_t m_nextPacket;
	uint64_t m_packetOffset; // offset in current packet

	uint32_t const m_maxSize;
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

constexpr auto MAX_SEND_QUEUE_SIZE = 128;
constexpr auto MAX_PACKETS_COUNT = MAX_SEND_QUEUE_SIZE;

class NetworkManager;

class Connection {
public:
	Connection(int socket, AddrInfo const & addr)
		: m_socket(socket)
		, m_address(addr)
		, m_isClosed(false)
		, m_buffer(MAX_PACKETS_COUNT)
	{}

	void read(void * buf, size_t size)
	{
		std::unique_lock<std::mutex> lock(m_bufferGuard);
		uint8_t * data = (uint8_t *) buf;
		while (size > 0) {
			while (m_buffer.empty())
				m_readEvent.wait(lock);

			if (m_isClosed)
				throw socket_exception("peer closed connection");

			size_t readSize = m_buffer.read(data, size);
			size -= readSize;
			data += readSize;
		}
	}

	void write(void const * buf, size_t size)
	{
		MyCPDataPacket packet;
		std::unique_lock<std::mutex> lock(m_sendQGuard);
		uint8_t * data = (uint8_t *) buf;
		while (size > 0) {
			while (m_sendQueue.size() == MAX_SEND_QUEUE_SIZE)
				m_sendEvent.wait(lock);

			packet.m_size = size > MYCP_MAX_DATA_SIZE ? MYCP_MAX_DATA_SIZE : size;
			memcpy(packet.m_data, data, packet.m_size);
			m_sendQueue.push(packet);

			data += packet.m_size;
			size -= packet.m_size;
		}
	}

private:
	int m_socket;
	AddrInfo m_address;

	std::atomic_bool m_isClosed;

	std::queue<MyCPDataPacket> m_sendQueue;
	std::mutex m_sendQGuard;
	std::condition_variable m_sendEvent;
	uint64_t m_nextPacketNum = 0;

	std::list<MyCPDataPacket> m_sentPackets;

	uint64_t m_timeout = DEFAULT_TIMEOUT;
	uint32_t m_maxSentPackets = 1;
	uint16_t m_receiverWindow = MYCP_MAX_WINDOW_SIZE;

	ReceiveBuffer m_buffer;
	std::mutex m_bufferGuard;
	std::condition_variable m_readEvent;

	std::queue<MyCPAckPacket> m_acks;

	friend class NetworkManager;
};

///////////////////////////////////////////////////////////////////////////////

class NetworkManager {
public:
	NetworkManager()
		: m_pollFd(::epoll_create(1))
		, m_stopFd(::eventfd(1, EFD_NONBLOCK))
	{
		if (m_pollFd < 0)
			throw std::runtime_error("failed to create epoll");

		if (m_stopFd < 0)
			throw std::runtime_error("failed to create control descriptor");
		add_socket(m_stopFd);

		std::thread t(std::bind(&NetworkManager::main_loop, this));
		t.detach();
	}

	ConnectionPtr register_connection(int socket, AddrInfo const & addr)
	{
		assert(socket >= 0);

		std::lock_guard<std::mutex> lock(m_connectionsGuard);
		auto it = m_connections.emplace(socket, std::make_shared<Connection>(socket, addr));
		add_socket(socket, true);
		return it.first->second;
	}

	void unregister_connection(int socket)
	{
		std::lock_guard<std::mutex> lock(m_connectionsGuard);
		m_connections.erase(socket);
	}

private:
	void main_loop()
	{
		while (true) {
			std::vector<epoll_event> events(MAX_SOCKET_COUNT);
			int timeout = get_timeout();
			int readyEvents = epoll_wait(m_pollFd, &events[0], MAX_SOCKET_COUNT, timeout);
			if (readyEvents < 0)
				throw std::runtime_error("failed to poll sockets: " + std::string(strerror(errno)));

			std::lock_guard<std::mutex> lock(m_connectionsGuard);
			for (int i = 0; i < readyEvents; ++i) {
				auto& event = events[i];

				if (event.data.u32 == uint32_t(m_stopFd))
					return;

				auto it = m_connections.find(event.data.u32);
				if (it == m_connections.end())
					continue;

				auto connection = it->second;
				if (event.events & EPOLLRDHUP) {
					connection->m_isClosed = true;
				}

				if (event.events & EPOLLIN) {
					do_reads(connection);
				}

				if (event.events & EPOLLOUT) {
					do_writes(connection);
				}
			}
		}
	}

	void do_reads(ConnectionPtr connection)
	{
		uint16_t window = MYCP_MAX_DATA_SIZE;
		uint64_t maxAckedNum = 0;
		bool readAck = false;

		std::lock_guard<std::mutex> lock(connection->m_bufferGuard);
		uint8_t bytes[MTU_SIZE];
		MyCPHeader header;
		while (PacketIO::receive(connection->m_socket, connection->m_address, &header, bytes)) {
			switch (header.m_type) {
				case MyCPMessageType::MyCP_ACK: {
					MyCPAckPacket ack = to_ack(bytes);
					maxAckedNum = ack.m_header.m_packetNumber;
					window = std::min(window, ack.m_window);
					readAck = true;
					break;
				}
				case MyCPMessageType::MyCP_DATA: {
					MyCPDataPacket packet = to_data(bytes);
					if (connection->m_buffer.has_free_space()) {
						connection->m_buffer.add_packet(packet);

						MyCPAckPacket ack;
						ack.m_header.m_type = MyCPMessageType::MyCP_ACK;
						ack.m_header.m_packetNumber = packet.m_header.m_packetNumber;
						ack.m_header.m_timestamp = packet.m_header.m_timestamp;
						ack.m_window = connection->m_buffer.get_free_space();
						connection->m_acks.push(ack);
					}
					break;
				}
				default: break;
			}
		}

		if (readAck) {
			connection->m_receiverWindow = window;
			// can calculate RTT?
			for (auto it = connection->m_sentPackets.begin(); connection->m_sentPackets.end() != it; )
				if (it->m_header.m_packetNumber <= maxAckedNum)
					it = connection->m_sentPackets.erase(it);
				else
					++it;
		}

		if (!connection->m_buffer.empty())
			connection->m_readEvent.notify_one();
	}

	void do_writes(ConnectionPtr connection)
	{
		int socket = connection->m_socket;
		auto const & addr = connection->m_address;

		// retransmit
		auto now = get_now();
		bool hasTimedoutPackets = false;
		for (auto& packet: connection->m_sentPackets) {
			if (packet.m_header.m_timestamp + connection->m_timeout > now)
				continue;

			hasTimedoutPackets = true;
			packet.m_header.m_timestamp = now;
			if (!PacketIO::send(socket, addr, packet))
				break;
		}
		if (hasTimedoutPackets)
			connection->m_maxSentPackets /= 2;
		else
			connection->m_maxSentPackets += 1;

		// send acks
		while (!connection->m_acks.empty()) {
			auto& ack = connection->m_acks.front();
			if (!PacketIO::send(socket, addr, ack))
				break;
			connection->m_acks.pop();
		}

		// send packets
		{
			std::unique_lock<std::mutex> lock(connection->m_sendQGuard);
			while (!connection->m_sendQueue.empty()) {
				if (connection->m_sentPackets.size() >= connection->m_maxSentPackets)
					break;

				auto& packet = connection->m_sendQueue.front();
				packet.m_header.m_type = MyCPMessageType::MyCP_DATA;
				packet.m_header.m_packetNumber = connection->m_nextPacketNum++;
				packet.m_header.m_timestamp = get_now();
				if (!PacketIO::send(socket, addr, packet))
					break;
				connection->m_sentPackets.push_back(packet);
				connection->m_sendQueue.pop();
			}

			if (connection->m_sendQueue.size() < MAX_SEND_QUEUE_SIZE)
				connection->m_sendEvent.notify_one();
		}
	}

	MyCPAckPacket to_ack(uint8_t * bytes)
	{
		MyCPAckPacket packet;
		memcpy(&packet, bytes, sizeof(packet));
		return packet;
	}

	MyCPDataPacket to_data(uint8_t * bytes)
	{
		MyCPDataPacket packet;
		memcpy(&packet, bytes, sizeof(packet));
		return packet;
	}

	void add_socket(int socket, bool checkWrite = false)
	{
		epoll_event event{0};
		event.data.u32 = socket;
		event.events = EPOLLOUT | EPOLLRDHUP;
		if (checkWrite)
			event.events |= EPOLLIN;

		if (epoll_ctl(m_pollFd, EPOLL_CTL_ADD, socket, &event) < 0)
			throw std::runtime_error("failed to add socket to epoll");
	}

	int get_timeout()
	{
		std::lock_guard<std::mutex> lock(m_connectionsGuard);
		int timeout = -1;
		for (auto it: m_connections)
			timeout = std::min(timeout, int(it.second->m_timeout));
		return timeout;
	}

private:
	int m_pollFd;
	int m_stopFd;

	std::unordered_map<int, ConnectionPtr> m_connections;
	std::mutex m_connectionsGuard;
};

static NetworkManager manager;

///////////////////////////////////////////////////////////////////////////////

au_stream_client_socket::au_stream_client_socket(
		sockaddr const & remoteAddr,
		uint16_t remotePort,
		uint16_t localPort,
		bool connected)
	: with_descriptor(create_socket(connected))
{
	m_address.m_localPort = localPort;
	m_address.m_remoteAddr = remoteAddr;
	m_address.m_remotePort = remotePort;

	if (connected)
		m_connection = manager.register_connection(m_descriptor, m_address);
}

au_stream_client_socket::au_stream_client_socket(std::string const & hostname, uint16_t port)
	: au_stream_client_socket(create_addr(hostname, port), port, get_random_port(), false)
{}

au_stream_client_socket::~au_stream_client_socket()
{
	manager.unregister_connection(m_descriptor);
}

void au_stream_client_socket::connect()
{
	if (m_connection)
		throw socket_exception("socket already connected");

	MyCPHeader hdr;
	memset(&hdr, 0, sizeof(hdr));
	hdr.m_type = MyCPMessageType::MyCP_SYN;
	if (!PacketIO::send(m_descriptor, m_address, hdr))
		throw socket_exception("failed to connect: unknown error 1");

	memset(&hdr, 0, sizeof(hdr));
	uint8_t bytes[MTU_SIZE];
	if (!PacketIO::receive(m_descriptor, m_address, &hdr, bytes))
		throw socket_exception("failed to connect: unknown error 2");

	if (hdr.m_type != MyCPMessageType::MyCP_SYNACK)
		throw socket_exception("failed to connect: unknown peer response");

	int flags = fcntl(m_descriptor, F_GETFL, 0);
	if (flags < 0)
		throw socket_exception("failed to connect: can't get socket state");

	flags |= O_NONBLOCK;
	if (fcntl(m_descriptor, F_SETFL, flags) < 0)
		throw socket_exception("failed to connect: can't set socket state");

	m_connection = manager.register_connection(m_descriptor, m_address);
}

void au_stream_client_socket::send(void const * buf, size_t size)
{
	m_connection->write(buf, size);
}

void au_stream_client_socket::recv(void * buf, size_t size)
{
	m_connection->read(buf, size);
}

///////////////////////////////////////////////////////////////////////////////

au_stream_server_socket::au_stream_server_socket(std::string const & hostname, uint16_t port)
	: with_descriptor(create_socket(false))
{
	m_address.m_localPort = port;
}

socket_ptr au_stream_server_socket::accept_one_client()
{
	uint8_t bytes[MTU_SIZE];
	while (true) {
		MyCPHeader hdr;
		sockaddr addr;
		if (!PacketIO::receiveAny(m_descriptor, m_address.m_localPort, &addr, &hdr, bytes)) {
			throw socket_exception("failed to accept: unknown error 1");
		}

		if (hdr.m_type != MyCPMessageType::MyCP_SYN)
			continue;

		AddrInfo remoteAddr;
		remoteAddr.m_localPort = m_address.m_localPort;
		remoteAddr.m_remoteAddr = addr;
		remoteAddr.m_remotePort = hdr.m_srcPort;

		hdr.m_type = MyCPMessageType::MyCP_SYNACK;
		if (!PacketIO::send(m_descriptor, remoteAddr, hdr))
			throw socket_exception("failed to accept: can't send ack");

		return std::make_shared<au_stream_client_socket>(addr, remoteAddr.m_localPort, remoteAddr.m_remotePort, true);
	}
}
