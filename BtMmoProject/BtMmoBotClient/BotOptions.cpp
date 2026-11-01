#include <Pch.h>
#include "BotOptions.h"
#include <Options.h>

std::expected<BotOptions, std::string> BotOptions::Parse(int argc, wchar_t** argv)
{
	Options args(argc, argv, {"--host", "--port", "--clients", "--timeout", "--threads"}, {"--verbose", "--quiet"});
	BotOptions result;
	result.host = args.Text("--host", "127.0.0.1");
	result.port = args.Number("--port", 17777, 1, 65535);
	result.clients = args.Number("--clients", 10, 1, 10000);
	result.timeoutSeconds = args.Number("--timeout", 30, 1, 86400);
	result.threads = args.Number("--threads", 4, 1, 256);
	result.verbose = !args.Has("--quiet");
	if (!args.Error().empty())
	{
		return std::unexpected(args.Error());
	}

	return result;
}
