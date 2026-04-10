#pragma once

// Compatibility shims for MSVC-specific functions when building with MinGW/GCC
#if !defined(_MSC_VER)
#include <wchar.h>
#include <cstdio>

#ifndef _wcsicmp
#define _wcsicmp wcscasecmp
#endif

inline int wcscpy_s(wchar_t* dest, size_t destsz, const wchar_t* src)
{
    if (!dest || !src || destsz == 0) return -1;
    wcsncpy(dest, src, destsz);
    dest[destsz - 1] = L'\0';
    return 0;
}

// Overload matching MSVC template: wcscpy_s(wchar_t (&dest)[N], src)
template<size_t N>
inline int wcscpy_s(wchar_t (&dest)[N], const wchar_t* src)
{
    return wcscpy_s(dest, N, src);
}

inline int wcscat_s(wchar_t* dest, size_t destsz, const wchar_t* src)
{
    if (!dest || !src || destsz == 0) return -1;
    wcsncat(dest, src, destsz - wcslen(dest) - 1);
    return 0;
}

template<size_t N>
inline int wcscat_s(wchar_t (&dest)[N], const wchar_t* src)
{
    return wcscat_s(dest, N, src);
}

template<size_t N, typename... Args>
inline int swprintf_s(wchar_t (&dest)[N], const wchar_t* fmt, Args... args)
{
    return swprintf(dest, N, fmt, args...);
}

#endif // !_MSC_VER
