#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include "AvDatabase.h"

// ---------------------------------------------------------------
// –езультат сканировани€ одного объекта
// ---------------------------------------------------------------
struct ScanResult
{
    bool         IsMalicious = false;
    std::wstring ThreatName;       // название угрозы если найдена
    std::wstring FilePath;         // путь к файлу
};

// ---------------------------------------------------------------
// ƒвижок сканировани€
// –еализует алгоритм из п.3 требований
// ---------------------------------------------------------------
class ScanEngine
{
public:
    // —канировать поток байтов
    // data     Ч указатель на данные
    // size     Ч размер данных
    // type     Ч тип объекта
    // result   Ч результат сканировани€
    static bool Scan(const uint8_t* data,
        size_t         size,
        ObjectType     type,
        const AvDatabase& db,
        ScanResult& result);
};