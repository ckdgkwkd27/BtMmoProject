#include <Pch.h>
#include <NetworkCore/SendBufferQueue.h>

void SendBufferQueue::Push(SendBufferPtr buffer)
{
	pending.push(std::move(buffer));
}

void SendBufferQueue::BeginBatch()
{
	if (HasActive())
	{
		return;
	}

	while (!pending.empty() && active.size() < 16)
	{
		active.push_back(std::move(pending.front()));
		pending.pop();
	}
}

std::vector<WSABUF> SendBufferQueue::GetBuffers() const
{
	std::vector<WSABUF> buffers;
	buffers.reserve(active.size());
	std::size_t _offset = offset;
	for (const auto& buffer : active)
	{
		WSABUF native{};
		native.buf = reinterpret_cast<char*>(const_cast<BYTE*>(buffer->Buffer() + _offset));
		native.len = static_cast<ULONG>(buffer->WriteSize() - _offset);
		buffers.push_back(native);
		_offset = 0;
	}

	return buffers;
}

bool SendBufferQueue::Consume(std::size_t bytes)
{
	std::size_t available = 0;
	for (const auto& buffer : active)
	{
		available += buffer->WriteSize();
	}

	available -= offset;
	if (bytes > available)
	{
		return false;
	}

	while (bytes != 0)
	{
		const auto remaining = static_cast<std::size_t>(active.front()->WriteSize()) - offset;
		const auto consumed = std::min(bytes, remaining);
		bytes -= consumed;
		offset += consumed;
		if (offset == static_cast<std::size_t>(active.front()->WriteSize()))
		{
			active.pop_front();
			offset = 0;
		}
	}

	return true;
}

std::size_t SendBufferQueue::ClearPending()
{
	std::size_t bytes = 0;
	while (!pending.empty())
	{
		bytes += pending.front()->WriteSize();
		pending.pop();
	}

	return bytes;
}

std::size_t SendBufferQueue::ClearActive()
{
	std::size_t bytes = 0;
	for (const auto& buffer : active)
	{
		bytes += buffer->WriteSize();
	}

	bytes -= offset;
	active.clear();
	offset = 0;
	return bytes;
}
