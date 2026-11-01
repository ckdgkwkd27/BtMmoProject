#include <Pch.h>
#include <NetworkCore/IocpCore.h>
#include <NetworkCore/IocpEvent.h>

IocpCore::IocpCore()
    : iocpHandle(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0))
{
	if (iocpHandle == nullptr)
	{
		error = ::GetLastError();
	}
}

IocpCore::~IocpCore()
{
	if (iocpHandle)
	{
		::CloseHandle(iocpHandle);
	}
}

bool IocpCore::Register(const IocpObjectPtr& object)
{
	return IsValid() && object && ::CreateIoCompletionPort(object->GetHandle(), iocpHandle, 0, 0) != nullptr;
}

bool IocpCore::Dispatch(uint32 timeoutMs)
{
	DWORD bytes = 0;
	ULONG_PTR key = 0;
	OVERLAPPED* overlapped = nullptr;
	const BOOL success = ::GetQueuedCompletionStatus(iocpHandle, &bytes, &key, &overlapped, timeoutMs);
	const DWORD _error = success ? ERROR_SUCCESS : ::GetLastError();
	if (overlapped == nullptr)
	{
		if (!success && _error != WAIT_TIMEOUT)
		{
			error = _error;
		}

		return false;
	}

	auto* event = static_cast<IocpEvent*>(overlapped);
	auto owner = std::move(event->owner);
	if (owner)
	{
		owner->Dispatch(event, bytes, _error);
	}
	else
	{
		error = ERROR_INVALID_DATA;
	}

	OperationCompleted();
	return true;
}

void IocpCore::Wake()
{
	::PostQueuedCompletionStatus(iocpHandle, 0, 0, nullptr);
}
