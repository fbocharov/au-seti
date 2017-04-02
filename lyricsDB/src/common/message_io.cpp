#include "message_io.h"

#include <algorithm>

size_t constexpr BUCKET_SIZE = 1024;

void send_message(stream_socket & socket, message_ptr message)
{
	auto bytes = message->serialize();

	uint64_t size = bytes.size();
	socket.send(&size, sizeof(size));

	uint64_t sendSize = 0;
	uint8_t const * data = bytes.data();
	while (sendSize < size) {
		uint64_t needSend = std::min(BUCKET_SIZE, size - sendSize);
		socket.send(data + sendSize, needSend);
		sendSize += needSend;
	}
}

message_ptr recv_message(stream_socket & socket)
{
	uint64_t size = 0;
	socket.recv(&size, sizeof(size));

	message_bytes bytes(size);
	uint64_t recvSize = 0;
	uint8_t * data = bytes.data();
	while (size > 0) {
		uint64_t needRecv = std::min(BUCKET_SIZE, size - recvSize);
		socket.recv(data + recvSize, needRecv);
		size -= needRecv;
	}

	return parse_message(bytes);
}
