#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

enum class message_type: uint8_t {
	// client messages
	GET_AUTHOR_SONG_REQUEST = 0,
	GET_AUTHOR_REQUEST = 1,
	ADD_AUTHOR_SONG_REQUEST = 2,

	// server messages
	GET_AUTHOR_SONG_RESPONSE = 64,
	GET_AUTHOR_RESPONSE = 65,
	ADD_AUTHOR_SONG_RESPONSE = 66
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

class get_author_request: public message {
public:
	get_author_request(std::string const & author);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(request_visitor & v) override;

private:
	std::string m_author;
};

class get_author_response: public message {
public:
	get_author_response(std::vector<std::string> const & songs);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(response_visitor & v) override;

private:
	std::vector<std::string> const & m_songs;
};

///////////////////////////////////////////////////////////////////////////////

class get_author_song_request: public message {
public:
	get_author_song_request(std::string const & author, std::string const & song);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(request_visitor & v) override;

private:
	std::string m_author;
	std::string m_song;
};

class get_author_song_response: public message {
public:
	get_author_song_response(std::string const & text);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(response_visitor & v) override;

private:
	std::string m_text;
};

///////////////////////////////////////////////////////////////////////////////

class add_author_song_request: public message {
public:
	add_author_song_request(
		std::string const & author,
		std::string const & song,
		std::string const & text);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(request_visitor & v) override;

private:
	std::string m_author;
	std::string m_song;
	std::string m_text;
};

class add_author_song_response: public message {
public:
	add_author_song_response(bool result);

	message_bytes serialize() const override;
	static message_ptr deserialize(message_bytes const & bytes);

	void accept(response_visitor & v) override;

private:
	bool m_result;
};

///////////////////////////////////////////////////////////////////////////////

struct request_visitor {
	virtual ~request_visitor() = default;
	virtual void visit(get_author_request & request) = 0;
	virtual void visit(get_author_song_request & request) = 0;
	virtual void visit(add_author_song_request & request) = 0;
};

struct response_visitor {
	virtual ~response_visitor() = default;
	virtual void visit(get_author_response & request) = 0;
	virtual void visit(get_author_song_response & request) = 0;
	virtual void visit(add_author_song_response & request) = 0;
};

///////////////////////////////////////////////////////////////////////////////

message_ptr parse_message(message_bytes const & bytes);
