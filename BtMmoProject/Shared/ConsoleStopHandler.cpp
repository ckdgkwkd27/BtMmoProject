#include <Pch.h>
#include "ConsoleStopHandler.h"

std::atomic<bool> ConsoleStopHandler::requested{false};

ConsoleStopHandler::ConsoleStopHandler()
{
	requested = false;
	registered = ::SetConsoleCtrlHandler(&Handle, TRUE) != FALSE;
}

ConsoleStopHandler::~ConsoleStopHandler()
{
	if (registered)
	{
		::SetConsoleCtrlHandler(&Handle, FALSE);
	}
}

BOOL WINAPI ConsoleStopHandler::Handle(DWORD event)
{
	if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT)
	{
		return FALSE;
	}

	requested.store(true);
	return TRUE;
}

bool ConsoleStopHandler::IsStopRequested() const
{
	return requested.load();
}
