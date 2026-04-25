#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iphlpapi.h>
#include <vector>
#include <string>

#pragma comment(lib, "iphlpapi.lib")

inline std::wstring GetDeviceIdentifier()
{
    ULONG bufLen = 15000;
    std::vector<BYTE> buf(bufLen);
    IP_ADAPTER_INFO* pAdapters = reinterpret_cast<IP_ADAPTER_INFO*>(buf.data());

    if (GetAdaptersInfo(pAdapters, &bufLen) == ERROR_BUFFER_OVERFLOW)
    {
        buf.resize(bufLen);
        pAdapters = reinterpret_cast<IP_ADAPTER_INFO*>(buf.data());
    }

    if (GetAdaptersInfo(pAdapters, &bufLen) != ERROR_SUCCESS)
        return L"00:00:00:00:00:00";

    for (IP_ADAPTER_INFO* p = pAdapters; p != nullptr; p = p->Next)
    {
        if (p->AddressLength == 6 &&
            !(p->Address[0] == 0 && p->Address[1] == 0 &&
                p->Address[2] == 0 && p->Address[3] == 0 &&
                p->Address[4] == 0 && p->Address[5] == 0))
        {
            wchar_t mac[32]{};
            swprintf_s(mac, 32,
                L"%02X:%02X:%02X:%02X:%02X:%02X",
                (unsigned)p->Address[0], (unsigned)p->Address[1],
                (unsigned)p->Address[2], (unsigned)p->Address[3],
                (unsigned)p->Address[4], (unsigned)p->Address[5]);
            return mac;
        }
    }
    return L"00:00:00:00:00:00";
}