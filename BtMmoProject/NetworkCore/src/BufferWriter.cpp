#include <Pch.h>
#include <NetworkCore/BufferWriter.h>

bool BufferWriter::Write(const void* source, std::size_t length)
{
	if (length > FreeSize() || (length != 0 && source == nullptr))
	{
		return false;
	}

	if (length != 0)
	{
		std::memcpy(buffer.data() + pos, source, length);
	}

	pos += length;
	return true;
}

bool BufferWriter::U16(uint16 value)
{
	const std::array<BYTE, 2> bytes{static_cast<BYTE>(value >> 8), static_cast<BYTE>(value)};
	return Write(bytes.data(), bytes.size());
}

bool BufferWriter::U64(uint64 value)
{
	std::array<BYTE, 8> bytes{};
	for (std::size_t i = 0; i < bytes.size(); ++i)
	{
		bytes[i] = static_cast<BYTE>(value >> ((7 - i) * 8));
	}

	return Write(bytes.data(), bytes.size());
}

bool BufferWriter::String(const std::string& value)
{
	if (value.size() > std::numeric_limits<uint16>::max() || value.size() + 2 > FreeSize())
	{
		return false;
	}

	return U16(static_cast<uint16>(value.size())) && Write(value.data(), value.size());
}
