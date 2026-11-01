#include <Pch.h>
#include <NetworkCore/BufferReader.h>

bool BufferReader::Peek(void* destination, std::size_t length) const
{
	if (length > FreeSize() || (length != 0 && destination == nullptr))
	{
		return false;
	}

	if (length != 0)
	{
		std::memcpy(destination, buffer.data() + pos, length);
	}

	return true;
}

bool BufferReader::Read(void* destination, std::size_t length)
{
	if (!Peek(destination, length))
	{
		return false;
	}

	pos += length;
	return true;
}

bool BufferReader::U16(uint16& value)
{
	std::array<BYTE, 2> bytes{};
	if (!Read(bytes.data(), bytes.size()))
	{
		return false;
	}

	value = static_cast<uint16>((bytes[0] << 8) | bytes[1]);
	return true;
}

bool BufferReader::U64(uint64& value)
{
	std::array<BYTE, 8> bytes{};
	if (!Read(bytes.data(), bytes.size()))
	{
		return false;
	}

	value = 0;
	for (auto byte : bytes)
	{
		value = (value << 8) | byte;
	}

	return true;
}

bool BufferReader::String(std::string& value, std::size_t maxBytes)
{
	uint16 size{};
	if (!U16(size) || size > maxBytes || size > FreeSize())
	{
		return false;
	}

	value.resize(size);
	return Read(value.data(), size);
}
