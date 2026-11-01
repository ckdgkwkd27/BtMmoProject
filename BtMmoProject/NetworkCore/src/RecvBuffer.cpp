#include <Pch.h>
#include <NetworkCore/RecvBuffer.h>

RecvBuffer::RecvBuffer(int32 _bufferSize) : capacity(0), bufferSize(_bufferSize)
{
	if (_bufferSize <= 0 || _bufferSize > 65536)
	{
		return;
	}

	capacity = _bufferSize * 2;
	buffer.resize(capacity);
}

void RecvBuffer::Clean()
{
	const int32 dataSize = DataSize();
	if (dataSize == 0)
	{
		readPos = writePos = 0;
	}
	else if (FreeSize() < bufferSize)
	{
		std::memmove(buffer.data(), ReadPos(), dataSize);
		readPos = 0;
		writePos = dataSize;
	}
}

bool RecvBuffer::OnRead(int32 bytes)
{
	if (!IsValid() || bytes < 0 || bytes > DataSize())
	{
		return false;
	}

	readPos += bytes;
	return true;
}

bool RecvBuffer::OnWrite(int32 bytes)
{
	if (!IsValid() || bytes < 0 || bytes > FreeSize())
	{
		return false;
	}

	writePos += bytes;
	return true;
}
