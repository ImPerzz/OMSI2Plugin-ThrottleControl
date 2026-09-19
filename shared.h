#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>
#include <string>

#define PROJECT_VERSION "1.0"

#define myprintf(dest, size, fmt, ...) _snprintf_s(dest, size, size - 1, fmt, __VA_ARGS__)
#define mystrcpy(dest, src, count) strncpy_s(dest, count, src, count - 1)

namespace g
{
	HMODULE dll_instance = nullptr;
	bool debug = false;
	const char* version = nullptr;
}

template <size_t size>
struct UnicodeString
{
	uint16_t code_page;
	uint16_t element_size;
	uint32_t reference_count;
	uint32_t length;
	wchar_t string[size];

	UnicodeString(const wchar_t* message, ...)
	{
		va_list va;
		va_start(va, message);
		vswprintf_s(string, size, message, va);
		va_end(va);

		code_page = 1200; // CP_WINUNICODE
		element_size = 2;
		reference_count = static_cast<uint32_t>(-1);
		length = static_cast<uint32_t>(std::char_traits<wchar_t>::length(string));
	}
};

enum LogType : char
{
	LT_PRINT = 0x0,
	LT_INFO = 0x1,
	LT_WARN = 0x2,
	LT_ERROR = 0x3,
	LT_FATAL = 0x4,
};

void Log(LogType type, const char* message, ...);

#define NO_OF(id) 0x0
namespace Offsets
{
	uintptr_t AddLogEntry = NO_OF(6);

	constexpr const char* str = ":3";
	constexpr int len = 2;

	inline const char* CheckVersion()
	{
		if (!strncmp(reinterpret_cast<char*>(0x7C9780), str, len))
		{
			AddLogEntry = 0x8022C0;
			return "2.3.004 - Latest Steam version";
		}

		if (!strncmp(reinterpret_cast<char*>(0x7C9668), str, len))
		{
			AddLogEntry = 0x801FD4;
			return "2.2.032 - Tram patch";
		}

		return nullptr;
	}
}
#undef NO_OF
