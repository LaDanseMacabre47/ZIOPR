#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include "AvDatabase.h"

struct ScanResult
{
    bool         IsMalicious = false;
    std::wstring ThreatName;
    std::wstring FilePath;
};

class ScanEngine
{
public:
    static bool Scan(const uint8_t* data,
        size_t         size,
        ObjectType     type,
        const AvDatabase& db,
        ScanResult& result);
};