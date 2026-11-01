#pragma once
#include <NetworkCore/Platform.h>

class ConsoleStopHandler
{
public:
	ConsoleStopHandler();
	~ConsoleStopHandler();
	ConsoleStopHandler(const ConsoleStopHandler&) = delete;
	ConsoleStopHandler& operator=(const ConsoleStopHandler&) = delete;
	bool IsStopRequested() const;
	bool IsValid() const
	{
		return registered;
	}
private:
	bool registered{false};
	static BOOL WINAPI Handle(DWORD event);
	static std::atomic<bool> requested;
};
