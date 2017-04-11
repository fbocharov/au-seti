#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

enum class message_type: uint8_t {
	// client messages
	GET_SONG_REQUEST = 0,
	GET_SONG_LIST_REQUEST = 1,
	ADD_SONG_REQUEST = 2,

	// server messages
	GET_SONG_RESPONSE = 64,
	GET_SONG_LIST_RESPONSE = 65,
	ADD_SONG_RESPONSE = 66
};

///////////////////////////////////////////////////////////////////////////////

struct request_visitor;
struct response_visitor;

using message_bytes = std::vector<uint8_t>;

struct message {
	virtual ~message() = default;
	virtual message_bytes serialize() const = 0;

	virtual void accept(request_visitor &)
	{
		throw std::runtime_error("message type don't support request visitor");
	}

	virtual void accept(response_visitor &)
	{
		throw std::runtime_error("message type don't support response visitor");
	}
};
using message_ptr = std::shared_ptr<message>;

///////////////////////////////////////////////////////////////////////////////

class get_song_list_request: public message {
public:
	get_song_list_request(std::string const & author);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(request_visitor & v) override;

	std::string const & get_author() const { return m_author; }

private:
	std::string m_author;
};

class get_song_list_response: public message {
public:
	explicit get_song_list_response(std::vector<std::string> const & songs);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(response_visitor & v) override;

	std::vector<std::string> const & get_songs() { return m_songs; }

private:
	std::vector<std::string> m_songs;
};

///////////////////////////////////////////////////////////////////////////////

class get_song_request: public message {
public:
	get_song_request(std::string const & author, std::string const & song);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(request_visitor & v) override;

	std::string const & get_author() const { return m_author; }
	std::string const & get_song() const { return m_song; }

private:
	std::string m_author;
	std::string m_song;
};

class get_song_response: public message {
public:
	get_song_response(std::string const & text);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(response_visitor & v) override;

	std::string const & get_text() const { return m_text; }

private:
	std::string m_text;
};

///////////////////////////////////////////////////////////////////////////////

class add_song_request: public message {
public:
	add_song_request(
		std::string const & author,
		std::string const & song,
		std::string const & text);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(request_visitor & v) override;

	std::string const & get_author() const { return m_author; }
	std::string const & get_song() const { return m_song; }
	std::string const & get_text() const { return m_text; }

private:
	std::string m_author;
	std::string m_song;
	std::string m_text;
};

class add_song_response: public message {
public:
	add_song_response(const std::__cxx11::string &result);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(response_visitor & v) override;

	std::string get_result() const { return m_result; }

private:
	std::string m_result;
};

///////////////////////////////////////////////////////////////////////////////

struct request_visitor {
	virtual ~request_visitor() = default;
	virtual void visit(get_song_list_request & request) = 0;
	virtual void visit(get_song_request & request) = 0;
	virtual void visit(add_song_request & request) = 0;
};

struct response_visitor {
	virtual ~response_visitor() = default;
	virtual void visit(get_song_list_response & request) = 0;
	virtual void visit(get_song_response & request) = 0;
	virtual void visit(add_song_response & request) = 0;
};

///////////////////////////////////////////////////////////////////////////////

message_ptr parse_message(message_bytes const & bytes);
