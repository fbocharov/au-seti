#include "protocol.h"

#include <algorithm>
#include <cstring>

get_author_request::get_author_request(std::string const & author)
	: m_author(author)
{}

message_bytes get_author_request::serialize() const
{
	message_bytes bytes(1 + 8 + m_author.length());

	bytes[0] = uint8_t(message_type::GET_AUTHOR_REQUEST);

	uint8_t * data = bytes.data();
	uint64_t offset = 1;
	uint64_t size = m_author.length();

	// write size
	memcpy(data + offset, &size, sizeof(size));
	offset += sizeof(size);

	// write author
	memcpy(data + offset, m_author.data(), size);

	return bytes;
}

message_ptr get_author_request::deserialize(message_bytes const & bytes)
{
	if (message_type(bytes[0]) != message_type::GET_AUTHOR_REQUEST)
		throw std::runtime_error("invalid message type");

	uint8_t const * data = bytes.data() + 1;

	uint64_t size = 0;
	memcpy(&size, data, sizeof(size));
	data += sizeof(size);

	return std::make_shared<get_author_request>(std::string(data, data + size));
}

void get_author_request::accept(request_visitor & v)
{
	v.visit(*this);
}


get_author_response::get_author_response(std::vector<std::string> const & songs)
	: m_songs(songs)
{}

message_bytes get_author_response::serialize() const
{
	uint64_t commonSize = 0;
	std::for_each(m_songs.begin(), m_songs.end(),
				  [&] (std::string const & str) { commonSize += str.size(); });

	uint64_t songCount = m_songs.size();
	message_bytes bytes(1 + 8 + 8 * songCount + commonSize);
	bytes[0] = uint8_t(message_type::GET_AUTHOR_RESPONSE);

	uint8_t * data = bytes.data();
	uint64_t offset = 1;
	memcpy(data + offset, &songCount, sizeof(songCount));
	offset += sizeof(songCount);

	for (const auto& song: m_songs) {
		uint64_t size = song.size();
		memcpy(data + offset, &size, sizeof(size));
		offset += sizeof(size);

		memcpy(data + offset, song.data(), size);
		offset += size;
	}

	return bytes;
}

message_ptr get_author_response::deserialize(message_bytes const & bytes)
{
	if (bytes[0] != uint8_t(message_type::GET_AUTHOR_RESPONSE))
		throw std::runtime_error("invalid message type");;

	uint8_t const * data = bytes.data() + 1;

	uint64_t songCount = 0;
	memcpy(&songCount, data, sizeof(songCount));
	data += sizeof(songCount);

	std::vector<std::string> songs;
	for (uint64_t i = 0; i < songCount; ++i) {
		uint64_t size = 0;
		memcpy(&size, data, sizeof(size));
		data += sizeof(size);

		songs.emplace_back(data, data + size);
		data += size;
	}

	return std::make_shared<get_author_response>(songs);
}

void get_author_response::accept(response_visitor & v)
{
	v.visit(*this);
}


///////////////////////////////////////////////////////////////////////////////

get_author_song_request::get_author_song_request(std::string const & author, std::string const & song)
	: m_author(author)
	, m_song(song)
{}

message_bytes get_author_song_request::serialize() const
{
	// TODO: implement!
	return {};
}

void get_author_song_request::accept(request_visitor & v)
{
	v.visit(*this);
}


get_author_song_response::get_author_song_response(std::string const & text)
	: m_text(text)
{}

message_bytes get_author_song_response::serialize() const
{
	// TODO: implement!
	return {};
}

void get_author_song_response::accept(response_visitor & v)
{
	v.visit(*this);
}

///////////////////////////////////////////////////////////////////////////////

add_author_song_request::add_author_song_request(
		std::string const & author,
		std::string const & song,
		std::string const & text)
	: m_author(author)
	, m_song(song)
	, m_text(text)
{}

message_bytes add_author_song_request::serialize() const
{
	// TODO: implement!
	return {};
}

void add_author_song_request::accept(request_visitor & v)
{
	v.visit(*this);
}


add_author_song_response::add_author_song_response(bool result)
	: m_result(result)
{}

message_bytes add_author_song_response::serialize() const
{
	// TODO: implement!
	return {};
}

void add_author_song_response::accept(response_visitor & v)
{
	v.visit(*this);
}

///////////////////////////////////////////////////////////////////////////////

message_ptr parse_message(message_bytes const & bytes)
{
	if (bytes.empty())
		return nullptr;

	message_type type = message_type(bytes[0]);
	switch (type) {
		case message_type::GET_AUTHOR_REQUEST:
			return get_author_request::deserialize(bytes);
		case message_type::GET_AUTHOR_SONG_REQUEST:
			return get_author_song_request::deserialize(bytes);
		case message_type::ADD_AUTHOR_SONG_REQUEST:
			return add_author_song_request::deserialize(bytes);
		case message_type::GET_AUTHOR_RESPONSE:
			return get_author_response::deserialize(bytes);
		case message_type::GET_AUTHOR_SONG_RESPONSE:
			return get_author_song_response::deserialize(bytes);
		case message_type::ADD_AUTHOR_SONG_RESPONSE:
			return add_author_song_response::deserialize(bytes);
		default:
			throw std::runtime_error("unknown message type");
	}
}
