#pragma once
#include "Platform.h"
#include "SendBuffer.h"

class SendBufferQueue
{
public:
	void Push(SendBufferPtr buffer);
	void BeginBatch();
	std::vector<WSABUF> GetBuffers() const;
	bool Consume(std::size_t bytes);
	bool HasActive() const
	{
		return !active.empty();
	}

	std::size_t ClearPending();
	std::size_t ClearActive();
private:
	std::queue<SendBufferPtr> pending;
	std::deque<SendBufferPtr> active;
	std::size_t offset{0};
};
