#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace clock_util {

inline long long now_ms()
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	const ULONGLONG ticks = (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
	constexpr ULONGLONG kEpochDiff = 116444736000000000ULL;
	return static_cast<long long>((ticks - kEpochDiff) / 10000ULL);
}

} // namespace clock_util
