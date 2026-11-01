#pragma once
#include "Platform.h"
#include "Types.h"

class IocpObject : public std::enable_shared_from_this<IocpObject>
{
public:
	virtual ~IocpObject() = default;
	virtual HANDLE GetHandle() const = 0;
	virtual void Dispatch(class IocpEvent* event, uint32 bytes, DWORD error) = 0;
};
