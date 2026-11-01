#pragma once
#include "IocpObject.h"

class IocpCore
{
public:
	IocpCore();
	~IocpCore();
	bool Register(const IocpObjectPtr& object);
	bool Dispatch(uint32 timeoutMs = INFINITE);
	bool IsValid() const
	{
		return iocpHandle != nullptr;
	}

	DWORD GetError() const
	{
		return error.load();
	}

	void Wake();
	void OperationStarted()
	{
		WorkAdded();
		pending.fetch_add(1);
	}

	void OperationCompleted()
	{
		pending.fetch_sub(1);
		WorkCompleted();
	}

	std::size_t PendingOperations() const
	{
		return pending.load();
	}

	void WorkAdded()
	{
		work.fetch_add(1);
	}

	void WorkCompleted()
	{
		work.fetch_sub(1);
	}

	std::size_t PendingWork() const
	{
		return work.load();
	}
private:
	HANDLE iocpHandle;
	std::atomic<DWORD> error{ERROR_SUCCESS};
	std::atomic<std::size_t> pending{0};
	std::atomic<std::size_t> work{0};
};
