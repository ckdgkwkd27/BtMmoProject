#include <Pch.h>
#include <NetworkCore/Utf8.h>

bool IsValidText(const std::string& text, std::size_t maxBytes)
{
	if (text.empty() || text.size() > maxBytes)
	{
		return false;
	}

	for (std::size_t i = 0; i < text.size();)
	{
		const auto first = static_cast<unsigned char>(text[i++]);
		uint32 code = first;
		uint32 minimum = 0;
		int trailing = 0;
		if (first >= 0xc2 && first <= 0xdf)
		{
			trailing = 1;
			code &= 0x1f;
			minimum = 0x80;
		}
		else if (first >= 0xe0 && first <= 0xef)
		{
			trailing = 2;
			code &= 0x0f;
			minimum = 0x800;
		}
		else if (first >= 0xf0 && first <= 0xf4)
		{
			trailing = 3;
			code &= 0x07;
			minimum = 0x10000;
		}
		else if (first >= 0x80)
		{
			return false;
		}

		for (int j = 0; j < trailing; ++j)
		{
			if (i == text.size())
			{
				return false;
			}

			const auto next = static_cast<unsigned char>(text[i++]);
			if ((next & 0xc0) != 0x80)
			{
				return false;
			}

			code = (code << 6) | (next & 0x3f);
		}

		if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ||
		    code < 0x20 || (code >= 0x7f && code <= 0x9f))
		{
			return false;
		}
	}

	return true;
}
