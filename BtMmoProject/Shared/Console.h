#pragma once
#include <Windows.h>

class Utf8Console
{
public:
	Utf8Console() : previous(GetConsoleOutputCP())
	{
		if (previous)
		{
			SetConsoleOutputCP(CP_UTF8);
		}
	}

	~Utf8Console()
	{
		std::cout.flush();
		std::cerr.flush();
		if (previous)
		{
			SetConsoleOutputCP(previous);
		}
	}

	Utf8Console(const Utf8Console&) = delete;
	Utf8Console& operator=(const Utf8Console&) = delete;
private:
	UINT previous;
};
