#include "protocol.h"

#include <algorithm>
#include <cstring>

namespace {

void serialize_one_string(std::string const & str, message_bytes & bytes)
{
	uint8_t * data = bytes.data();
	uint64_t offset = 1; // skip message type
	uint64_t size = str.length();

	// write size
	memcpy(data + offset, &size, sizeof(size));
	offset += sizeof(size);

	// write author
	memcpy(data + offset, str.data(), size);
}

std::string deserialize_one_string(message_bytes const & bytes)
{
	uint8_t const * data = bytes.data() + 1; // skip message type

	uint64_t size = 0;
	memcpy(&size, data, sizeof(size));
	data += sizeof(size);

	return std::string(data, data + size);
}

void serialize_many_strings(std::vector<std::string> const & strings, message_bytes & bytes)
{
	uint64_t stringCount = strings.size();
	uint8_t * data = bytes.data();
	uint64_t offset = 1; // skip message type
	memcpy(data + offset, &stringCount, sizeof(stringCount));
	offset += sizeof(stringCount);

	for (auto const & str: strings) {
		uint64_t size = str.size();
		memcpy(data + offset, &size, sizeof(size));
		offset += sizeof(size);

		memcpy(data + offset, str.data(), size);
		offset += size;
	}
}

std::vector<std::string> deserialize_many_strings(message_bytes const & bytes)
{
	uint8_t const * data = bytes.data() + 1; // skip message type

	uint64_t stringCount = 0;
	memcpy(&stringCount, data, sizeof(stringCount));
	data += sizeof(stringCount);

	std::vector<std::string> strings;
	for (uint64_t i = 0; i < stringCount; ++i) {
		uint64_t size = 0;
		memcpy(&size, data, sizeof(size));
		data += sizeof(size);

		strings.emplace_back(data, data + size);
		data += size;
	}

	return strings;
}

} // namespace


get_song_list_request::get_song_list_request(std::string const & author)
	: m_author(author)
{}

message_bytes get_song_list_request::serialize() const
{
	message_bytes bytes(1 + 8 + m_author.length());

	bytes[0] = uint8_t(message_type::GET_SONG_LIST_REQUEST);
	serialize_one_string(m_author, bytes);

	return bytes;
}

message_ptr get_song_list_request::deserialize(message_bytes const & bytes)
{
	if (message_type(bytes[0]) != message_type::GET_SONG_LIST_REQUEST)
		throw std::runtime_error("invalid message type");

	return std::make_shared<get_song_list_request>(deserialize_one_string(bytes));
}

void get_song_list_request::accept(request_visitor & v)
{
	v.visit(*this);
}


get_song_list_response::get_song_list_response(std::vector<std::string> const & songs)
	: m_songs(songs)
{}

message_bytes get_song_list_response::serialize() const
{
	uint64_t commonSize = 0;
	std::for_each(m_songs.begin(), m_songs.end(),
				  [&] (std::string const & str) { commonSize += str.size(); });

	uint64_t songCount = m_songs.size();
	message_bytes bytes(1 + 8 + 8 * songCount + commonSize);
	bytes[0] = uint8_t(message_type::GET_SONG_LIST_RESPONSE);

	serialize_many_strings(m_songs, bytes);

	return bytes;
}

message_ptr get_song_list_response::deserialize(message_bytes const & bytes)
{
	if (bytes[0] != uint8_t(message_type::GET_SONG_LIST_RESPONSE))
		throw std::runtime_error("invalid message type");

	return std::make_shared<get_song_list_response>(deserialize_many_strings(bytes));
}

void get_song_list_response::accept(response_visitor & v)
{
	v.visit(*this);
}


///////////////////////////////////////////////////////////////////////////////

get_song_request::get_song_request(std::string const & author, std::string const & song)
	: m_author(author)
	, m_song(song)
{}

message_bytes get_song_request::serialize() const
{
	message_bytes bytes(1 + 8 + 8 + m_author.length() + 8 + m_song.length());

	bytes[0] = uint8_t(message_type::GET_SONG_REQUEST);
	serialize_many_strings({ m_author, m_song }, bytes);

	return bytes;
}

message_ptr get_song_request::deserialize(message_bytes const & bytes)
{
	if (bytes[0] != uint8_t(message_type::GET_SONG_REQUEST))
		throw std::runtime_error("invalid message type");;

	auto strings = deserialize_many_strings(bytes);
	if (strings.size() != 2)
		throw std::runtime_error("not enough values to unpack");

	return std::make_shared<get_song_request>(strings[0], strings[1]);
}

void get_song_request::accept(request_visitor & v)
{
	v.visit(*this);
}


get_song_response::get_song_response(std::string const & text)
	: m_text(text)
{}

message_bytes get_song_response::serialize() const
{
	message_bytes bytes(1 + 8 + m_text.length());

	bytes[0] = uint8_t(message_type::GET_SONG_RESPONSE);
	serialize_one_string(m_text, bytes);

	return bytes;
}

message_ptr get_song_response::deserialize(message_bytes const & bytes)
{
	if (bytes[0] != uint8_t(message_type::GET_SONG_RESPONSE))
		throw std::runtime_error("invalid message type");;

	return std::make_shared<get_song_response>(deserialize_one_string(bytes));
}

void get_song_response::accept(response_visitor & v)
{
	v.visit(*this);
}

///////////////////////////////////////////////////////////////////////////////

add_song_request::add_song_request(
		std::string const & author,
		std::string const & song,
		std::string const & text)
	: m_author(author)
	, m_song(song)
	, m_text(text)
{}

message_bytes add_song_request::serialize() const
{
	message_bytes bytes(1 + 8 +
						8 + m_author.length() +
						8 + m_song.length() +
						8 + m_text.length());

	bytes[0] = uint8_t(message_type::ADD_SONG_REQUEST);
	serialize_many_strings({ m_author, m_song, m_text }, bytes);

	return bytes;
}

message_ptr add_song_request::deserialize(message_bytes const & bytes)
{
	if (bytes[0] != uint8_t(message_type::ADD_SONG_REQUEST))
		throw std::runtime_error("invalid message type");;

	auto strings = deserialize_many_strings(bytes);
	if (strings.size() != 3)
		throw std::runtime_error("not enough values to unpack");

	return std::make_shared<add_song_request>(strings[0], strings[1], strings[2]);
}

void add_song_request::accept(request_visitor & v)
{
	v.visit(*this);
}


add_song_response::add_song_response(std::string const & result)
	: m_result(result)
{}

message_bytes add_song_response::serialize() const
{
	message_bytes bytes(1 + 8 + m_result.length());

	bytes[0] = uint8_t(message_type::ADD_SONG_RESPONSE);
	serialize_one_string(m_result, bytes);

	return bytes;
}

message_ptr add_song_response::deserialize(message_bytes const & bytes)
{
	if (message_type(bytes[0]) != message_type::ADD_SONG_RESPONSE)
		throw std::runtime_error("invalid message type");

	return std::make_shared<get_song_list_request>(deserialize_one_string(bytes));
}

void add_song_response::accept(response_visitor & v)
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
		case message_type::GET_SONG_LIST_REQUEST:
			return get_song_list_request::deserialize(bytes);
		case message_type::GET_SONG_REQUEST:
			return get_song_request::deserialize(bytes);
		case message_type::ADD_SONG_REQUEST:
			return add_song_request::deserialize(bytes);
		case message_type::GET_SONG_LIST_RESPONSE:
			return get_song_list_response::deserialize(bytes);
		case message_type::GET_SONG_RESPONSE:
			return get_song_response::deserialize(bytes);
		case message_type::ADD_SONG_RESPONSE:
			return add_song_response::deserialize(bytes);
		default:
			throw std::runtime_error("unknown message type");
	}
}
