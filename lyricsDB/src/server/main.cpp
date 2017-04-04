#include <net/stream_socket.h>
#include <common/message_io.h>

#include <cstring>
#include <future>
#include <limits>
#include <unordered_map>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

void usage(std::string const & name)
{
	std::cerr << "Usage: " << name << " [SERVER_ADDR] [SERVER_PORT]" << std::endl << std::endl;
	std::cerr << "Options:" << std::endl;
	std::cerr << "  SERVER_ADDR [default = 127.0.0.1]  ip4-address of server with db" << std::endl;
	std::cerr << "  SERVER_PORT [default = 40001]      port of server with db" << std::endl;
}

class database {
public:
	void add_song(
		std::string const & author,
		std::string const & song,
		std::string const & text)
	{
		std::lock_guard<std::mutex> g(m_guard);
		m_authors[author][song] = text;
	}

	std::string get_song(std::string const & author, std::string const & song)
	{
		std::lock_guard<std::mutex> g(m_guard);

		auto authorIt = m_authors.find(author);
		if (authorIt == m_authors.end())
			return "";

		auto songIt = authorIt->second.find(song);
		if (songIt == authorIt->second.end())
			return "";

		return songIt->second;
	}

	std::vector<std::string> get_song_list(std::string const & author)
	{
		std::vector<std::string> songs;

		std::lock_guard<std::mutex> g(m_guard);
		auto authorIt = m_authors.find(author);
		if (authorIt != m_authors.end())
			for (auto const & it: authorIt->second)
				songs.push_back(it.first);

		return songs;
	}

private:
	std::mutex m_guard;
	std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_authors;
};
using database_ptr = std::shared_ptr<database>;

struct client_request_visitor: public request_visitor {
	explicit client_request_visitor(database_ptr db)
		: database(db)
	{}

	void visit(get_song_list_request & request) override
	{
		message = std::make_shared<get_song_list_response>(database->get_song_list(request.get_author()));
	}

	void visit(get_song_request & request) override
	{
		message = std::make_shared<get_song_response>(
			database->get_song(request.get_author(), request.get_song()));
	}

	void visit(add_song_request & request) override
	{
		database->add_song(request.get_author(), request.get_song(), request.get_text());
		message = std::make_shared<add_song_response>("OK");
	}

	message_ptr message;
	database_ptr database;
};

int main(int argc, char * argv[])
{
	if (argc > 3 || (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))) {
		usage(argv[0]);
		return (argc == 2 ? 0 : 1);
	}

	std::string hostname = "127.0.0.1";
	uint16_t port = 40001;
	(void) port;

	if (argc >= 2) {
		hostname = argv[1];
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

	auto ssocket = make_server_socket(hostname, port);

	database_ptr database;
	std::cerr << "server started on port " << port;
	while (true) {
		auto client = ssocket->accept_one_client();
		std::cerr << "accepted connection, start handling it" << std::endl;
		std::thread t([=] () {
			auto request = recv_message(*client);
			client_request_visitor v(database);
			request->accept(v);
			send_message(*client, v.message);
		});
		t.detach();
	}

	return 0;
}
