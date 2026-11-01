#pragma once
#include "Types.h"

class RecvBuffer
{
public:
	explicit RecvBuffer(int32 _bufferSize);
	bool IsValid() const
	{
		return capacity != 0;
	}

	void Clean();
	bool OnRead(int32 bytes);
	bool OnWrite(int32 bytes);
	BYTE* ReadPos()
	{
		return IsValid() ? buffer.data() + readPos : nullptr;
	}

	BYTE* WritePos()
	{
		return IsValid() ? buffer.data() + writePos : nullptr;
	}

	int32 DataSize() const
	{
		return writePos - readPos;
	}

	int32 FreeSize() const
	{
		return capacity - writePos;
	}
private:
	int32 capacity;
	int32 bufferSize;
	int32 readPos{0};
	int32 writePos{0};
	std::vector<BYTE> buffer;
};
