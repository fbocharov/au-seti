#include "with_descriptor.h"

#include <algorithm>

#include <unistd.h>

with_descriptor::with_descriptor(int descriptor)
	: m_descriptor(descriptor)
{}

with_descriptor::with_descriptor(with_descriptor && other)
{
	m_descriptor = other.m_descriptor;
	other.m_descriptor = -1;
}

with_descriptor & with_descriptor::operator=(with_descriptor && other)
{
	std::swap(m_descriptor, other.m_descriptor);
	return *this;
}

with_descriptor::~with_descriptor()
{
	if (is_valid())
		close(m_descriptor);
}

bool with_descriptor::is_valid() const
{
	return m_descriptor >= 0;
}

