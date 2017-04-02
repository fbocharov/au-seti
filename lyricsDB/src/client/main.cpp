#include <net/stream_socket.h>

#include <cstring>
#include <iostream>
#include <limits>
#include <string>

void usage(std::string const & name)
{
	std::cerr << "Usage: " << name << " [SERVER_ADDR] [SERVER_PORT]" << std::endl << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "  SERVER_ADDR [default = 127.0.0.1]  ip4-address of server with db" << std::endl;
	std::cerr << "  SERVER_PORT [default = 40001]      port of server with db" << std::endl;
}

void help()
{
	std::cerr << "  Supported commands:" << std::endl;
	std::cerr << "    get <author>         get all songs of author <author>" << std::endl;
	std::cerr << "    get <author> <song>  get song with name <song> of author <author>" << std::endl;
	std::cerr << "    add <author> <song>  upload song from file <song> of author <author>" << std::endl;
	std::cerr << "    help                 see this help" << std::endl;
	std::cerr << "    exit                 stop using this app" << std::endl;
}

void loop(client_socket_ptr socket)
{
	std::cout << "Welcome to lyrics DB 1.0!" << std::endl;
	std::cout << "Type `help` to see list of supported commands." << std::endl;

	std::string command;
	while (std::cin) {
		std::cout << "> ";
		std::getline(std::cin, command);

		if ("help" == command) {
			help();
			continue;
		}

		if ("exit" == command)
			break;
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

//	loop(make_client_socket(address, port, true));
	loop(nullptr);

	return 0;
}
