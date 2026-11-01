#include <Pch.h>
#include <NetworkCore/SendBuffer.h>

SendBuffer::SendBuffer(int32 bufferSize)
{
	if (bufferSize <= 0 || bufferSize > 65536)
	{
		return;
	}

	buffer.resize(bufferSize);
}

bool SendBuffer::CopyData(const void* data, int32 length)
{
	if (!IsValid() || length < 0 || length > Capacity() || (length != 0 && data == nullptr))
	{
		return false;
	}

	if (length != 0)
	{
		std::memcpy(buffer.data(), data, length);
	}

	writeSize = length;
	return true;
}

bool SendBuffer::Close(uint32 _writeSize)
{
	if (!IsValid() || _writeSize > static_cast<uint32>(Capacity()))
	{
		return false;
	}

	writeSize = static_cast<int32>(_writeSize);
	return true;
}
