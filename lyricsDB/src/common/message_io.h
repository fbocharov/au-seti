#pragma once

#include <net/stream_socket.h>
#include <protocol/protocol.h>

void send_message(stream_socket & socket, message const & message);

message_ptr recv_message(stream_socket & socket);
