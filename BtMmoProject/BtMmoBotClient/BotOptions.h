#pragma once

class BotOptions
{
public:
	static std::expected<BotOptions, std::string> Parse(int argc, wchar_t** argv);
	std::string host{"127.0.0.1"};
	unsigned port{17777};
	unsigned clients{10};
	unsigned timeoutSeconds{30};
	unsigned threads{4};
	bool verbose{true};
};
