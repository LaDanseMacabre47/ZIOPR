#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include "ScanEngine.h"

// ---------------------------------------------------------------
// Результат сканирования директории
// ---------------------------------------------------------------
struct DirectoryScanResult
{
    uint32_t             FilesScanned = 0;
    uint32_t             ThreatsFound = 0;
    std::vector<ScanResult> Threats;    // только вредоносные файлы
};

// ---------------------------------------------------------------
// Менеджер сканирования
// ---------------------------------------------------------------
class ScanManager
{
public:
    // Определить тип файла по расширению/заголовку
    static ObjectType DetectObjectType(const wchar_t* path,
        const uint8_t* data,
        size_t size);

    // Сканировать один файл
    static ScanResult ScanFile(const wchar_t* filePath);

    // Сканировать директорию (рекурсивно)
    static DirectoryScanResult ScanDirectory(const wchar_t* dirPath);
};