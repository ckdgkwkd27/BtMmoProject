#pragma once
#include "Types.h"

class BufferReader
{
public:
	explicit BufferReader(std::span<const BYTE> _buffer) : buffer(_buffer)
	{
	}

	bool Peek(void* destination, std::size_t length) const;
	bool Read(void* destination, std::size_t length);
	std::size_t FreeSize() const
	{
		return buffer.size() - pos;
	}

	bool Empty() const
	{
		return FreeSize() == 0;
	}

	bool U16(uint16& value);
	bool U64(uint64& value);
	bool String(std::string& value, std::size_t maxBytes);
private:
	std::span<const BYTE> buffer;
	std::size_t pos{0};
};
