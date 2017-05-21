#pragma once

class with_descriptor {
protected:
	with_descriptor(int descriptor);

	with_descriptor(with_descriptor const &) = delete;
	with_descriptor & operator=(with_descriptor const &) = delete;

	with_descriptor(with_descriptor && other);
	with_descriptor & operator=(with_descriptor && other);

	~with_descriptor();

	bool is_valid() const;

protected:
	int m_descriptor;
};
