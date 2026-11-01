#include <Pch.h>
#include "BotApplication.h"
#include <Console.h>

int wmain(int argc, wchar_t** argv)
{
	Utf8Console console;
	auto options = BotOptions::Parse(argc, argv);
	if (!options)
	{
		std::cerr << "Bot error: " << options.error() << '\n';
		return 1;
	}

	return BotApplication(std::move(*options)).Run();
}
