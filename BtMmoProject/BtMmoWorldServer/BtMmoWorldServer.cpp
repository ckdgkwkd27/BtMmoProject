#include <Pch.h>
#include "ServerApplication.h"
#include <Console.h>

int wmain(int argc, wchar_t** argv)
{
	Utf8Console console;
	auto options = ServerOptions::Parse(argc, argv);
	
	if (!options)
	{
		std::cerr << "Server error: " << options.error() << '\n';
		return 1;
	}
	
	return ServerApplication(std::move(*options)).Run();
}
