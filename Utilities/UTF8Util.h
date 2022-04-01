#pragma once

#include <fstream>

namespace utf8 {
	class utf8
	{
	public:
		static std::wstring decode(const std::string &str);
		static std::string encode(const std::wstring &wstr);
		static std::string encode(const std::u16string &wstr);
	};
		
	using std::ifstream;
	using std::ofstream;
}
