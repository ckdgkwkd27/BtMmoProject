#pragma once
#include "Types.h"

class BufferWriter
{
public:
	explicit BufferWriter(std::size_t capacity = 8192) : buffer(capacity)
	{
	}

	[[nodiscard]] bool Write(const void* source, std::size_t length);
	std::size_t FreeSize() const
	{
		return buffer.size() - pos;
	}

	std::span<const BYTE> Data() const
	{
		return {buffer.data(), pos};
	}

	[[nodiscard]] bool U16(uint16 value);
	[[nodiscard]] bool U64(uint64 value);
	[[nodiscard]] bool String(const std::string& value);
private:
	std::vector<BYTE> buffer;
	std::size_t pos{0};
};
