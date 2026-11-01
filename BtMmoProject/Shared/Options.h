#pragma once
#include "Types.h"
#include <NetworkCore/Platform.h>

class Options
{
public:
	template <typename Char>
	Options(int argc, Char** argv, std::initializer_list<std::string_view> values,
	        std::initializer_list<std::string_view> flags = {})
	{
		for (int i = 1; i < argc; ++i)
		{
			const std::string key = Utf8(argv[i]);
			if (!error.empty())
			{
				return;
			}

			if (options.contains(key))
			{
				SetError("Duplicate option: " + key);
				return;
			}

			bool found = false;
			for (auto flag : flags)
			{
				if (key == flag)
				{
					options.emplace(key, "true");
					found = true;
					break;
				}
			}

			if (found)
			{
				continue;
			}

			for (auto value : values)
			{
				if (key == value)
				{
					if (++i == argc)
					{
						SetError("Missing value: " + key);
						return;
					}

					options.emplace(key, Utf8(argv[i]));
					found = true;
					break;
				}
			}

			if (!found)
			{
				SetError("Unknown option: " + key);
				return;
			}

			if (!error.empty())
			{
				return;
			}
		}
	}

	bool Has(const std::string& key) const
	{
		return options.contains(key);
	}

	const std::string& Error() const
	{
		return error;
	}

	std::string Text(const std::string& key, std::string fallback) const
	{
		auto found = options.find(key);
		return found == options.end() ? fallback : found->second;
	}

	template <std::unsigned_integral T = uint32>
	T Number(const std::string& key, std::type_identity_t<T> fallback, uint32 minimum, uint32 maximum)
	{
		auto found = options.find(key);
		if (found == options.end())
		{
			return fallback;
		}

		T result{};
		const auto& value = found->second;
		if (auto [end, _error] = std::from_chars(value.data(), value.data() + value.size(), result); _error != std::errc{} || end != value.data() + value.size() || result < minimum || result > maximum)
		{
			SetError(key + " must be " + std::to_string(minimum) + ".." + std::to_string(maximum));
			return fallback;
		}

		return result;
	}
private:
	void SetError(std::string _error)
	{
		if (error.empty())
		{
			error = std::move(_error);
		}
	}

	static std::string Utf8(const char* value)
	{
		return value;
	}

	std::string Utf8(const wchar_t* value)
	{
		const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
		if (size == 0)
		{
			SetError("Invalid Unicode argument");
			return {};
		}

		std::string result(static_cast<std::size_t>(size), '\0');
		if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, result.data(), size, nullptr, nullptr))
		{
			SetError("Invalid Unicode argument");
			return {};
		}

		result.pop_back();
		return result;
	}

	std::unordered_map<std::string, std::string> options;
	std::string error;
};
