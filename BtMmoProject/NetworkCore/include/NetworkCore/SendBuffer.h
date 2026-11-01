#pragma once
#include "Types.h"

class SendBuffer
{
public:
	explicit SendBuffer(int32 bufferSize);
	bool IsValid() const
	{
		return !buffer.empty();
	}

	const BYTE* Buffer() const
	{
		return buffer.data();
	}

	int32 WriteSize() const
	{
		return writeSize;
	}

	int32 Capacity() const
	{
		return static_cast<int32>(buffer.size());
	}

	[[nodiscard]] bool CopyData(const void* data, int32 length);
	[[nodiscard]] bool Close(uint32 _writeSize);
private:
	std::vector<BYTE> buffer;
	int32 writeSize{0};
};
