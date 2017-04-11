#include <common/message_io.h>
#include <net/stream_socket.h>
#include <protocol/protocol.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

void usage(std::string const & name)
{
	std::cerr << "Usage: " << name << " [SERVER_ADDR] [SERVER_PORT]" << std::endl << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "  SERVER_ADDR [default = 127.0.0.1]  ip4-address of server with db" << std::endl;
	std::cerr << "  SERVER_PORT [default = 40001]      port of server with db" << std::endl;
}

void help()
{
	std::cerr << "  get <author>         get list of songs of author <author>" << std::endl;
	std::cerr << "  get <author> <song>  get song with name <song> of author <author>" << std::endl;
	std::cerr << "  add <author> <song>  upload song from file <song> of author <author>" << std::endl;
	std::cerr << "  help                 see this help" << std::endl;
	std::cerr << "  exit                 stop using this app" << std::endl;
}

bool validate_command(std::string const & command)
{
	return command == "get" || command == "add";
}

struct server_response_visitor: public response_visitor {

	void visit(get_song_list_response & request) override
	{
		songs = request.get_songs();
	}

	void visit(get_song_response & request) override
	{
		songs = { request.get_text() };
	}

	void visit(add_song_response & request) override
	{
		result = request.get_result();
	}

	std::string result;
	std::vector<std::string> songs;
};

class requester {
public:
	explicit requester(client_socket_ptr socket)
		: m_socket(socket)
	{}

	std::vector<std::string> request_get_song_list(std::string const & author)
	{
		get_song_list_request request(author);
		send_message(*m_socket, request);

		auto response = recv_message(*m_socket);
		server_response_visitor v;
		response->accept(v);

		return v.songs;
	}

	std::string request_get_song(std::string const & author, std::string const & song)
	{
		get_song_request request(author, song);
		send_message(*m_socket, request);

		auto response = recv_message(*m_socket);
		server_response_visitor v;
		response->accept(v);

		return v.songs.front();
	}

	std::string request_add_song(
		std::string const & author,
		std::string const & song,
		std::string const & text)
	{
		add_song_request request(author, song, text);
		send_message(*m_socket, request);

		auto response = recv_message(*m_socket);
		server_response_visitor v;
		response->accept(v);

		return v.result;
	}

private:
	client_socket_ptr m_socket;
};

std::string load_file(std::string const & path)
{
	std::ifstream stream(path);
	return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

void loop(client_socket_ptr & socket)
{
	std::cout << "Welcome to lyrics DB 1.0!" << std::endl;
	std::cout << "Type `help` to see list of supported commands." << std::endl;

	std::string command;
	requester r(socket);
	while (std::cin) {
		std::cout << "> ";
		std::getline(std::cin, command);

		if (command.empty())
			continue;

		if ("help" == command) {
			help();
			continue;
		}

		if ("exit" == command)
			break;

		std::stringstream ss(command);

		std::string cmd;
		ss >> cmd;
		if (!validate_command(cmd)) {
			std::cerr << "invalid command, type `help` to see list of supported commands" << std::endl;
			continue;
		}

		std::string author;
		ss >> author;
		if (author.empty()) {
			std::cerr << "invalid command arguments, type `help` to see list of supported commands" << std::endl;
			continue;
		}

		std::string song;
		ss >> song;
		if (song.empty()) {
			for (auto& song: r.request_get_song_list(author)) {
				std::cout << song << std::endl;
				std::cout  << "==============================" << std::endl;
			}
		} else {
			if (cmd == "get")
				std::cout << r.request_get_song(author, song) << std::endl;
			else { // cmd == "add"
				std::string textFile;
				ss >> textFile;
				if (textFile.empty()) {
					std::cerr << "you should specify file with text" << std::endl;
					continue;
				}
				std::cout << r.request_add_song(author, song, load_file(textFile)) << std::endl;
			}
		}
	}
	std::cout << std::endl;
}

int main(int argc, char * argv[])
{
	if (argc > 3 || (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
		usage(argv[0]);
		return (argc == 2 ? 0 : 1);
	}

	std::string address = "127.0.0.1";
	uint16_t port = 40001;
	(void) port;

	if (argc >= 2) {
		address = argv[1];
	}

	if (argc >= 3) {
		uint64_t p = std::stoul(argv[2]);
		uint16_t maxPort = std::numeric_limits<uint16_t>::max();
		if (p > maxPort) {
			std::cerr << "invalid port: should be in interval [0, " << maxPort << "]" << std::endl;
			return 1;
		}
		port = p;
	}

	auto socket = make_client_socket(address, port, true);
	loop(socket);

	return 0;
}
