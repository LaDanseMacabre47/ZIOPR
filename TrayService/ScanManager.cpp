#include "ScanManager.h"
#include "AvDatabase.h"
#include <cstring>

// ---------------------------------------------------------------
// Определить тип файла
// ---------------------------------------------------------------
ObjectType ScanManager::DetectObjectType(const wchar_t* path,
    const uint8_t* data,
    size_t         size)
{
    // PE: Magic MZ = 0x4D5A
    if (size >= 2 && data[0] == 0x4D && data[1] == 0x5A)
        return ObjectType::PE;

    // По расширению
    if (path)
    {
        const wchar_t* ext = wcsrchr(path, L'.');
        if (ext)
        {
            if (_wcsicmp(ext, L".ps1") == 0 ||
                _wcsicmp(ext, L".js") == 0 ||
                _wcsicmp(ext, L".py") == 0 ||
                _wcsicmp(ext, L".vbs") == 0 ||
                _wcsicmp(ext, L".bat") == 0 ||
                _wcsicmp(ext, L".cmd") == 0)
                return ObjectType::Script;
        }
    }

    return ObjectType::PE;  // по умолчанию
}

// ---------------------------------------------------------------
// Сканировать один файл
// ---------------------------------------------------------------
ScanResult ScanManager::ScanFile(const wchar_t* filePath)
{
    ScanResult result;
    result.FilePath = filePath;
    result.IsMalicious = false;

    if (!GetAvDatabaseManager().IsLoaded())
    {
        result.ThreatName = L"AV database not loaded";
        return result;
    }

    // Открываем файл
    HANDLE hFile = CreateFileW(filePath, GENERIC_READ,
        FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return result;

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0)
    {
        CloseHandle(hFile);
        return result;
    }

    // Читаем файл в память
    size_t size = (size_t)fileSize.QuadPart;
    std::vector<uint8_t> buf(size);
    DWORD bytesRead = 0;
    ReadFile(hFile, buf.data(), (DWORD)size, &bytesRead, nullptr);
    CloseHandle(hFile);

    if (bytesRead == 0) return result;

    // Определяем тип
    ObjectType type = DetectObjectType(filePath, buf.data(), bytesRead);

    // Сканируем
    ScanEngine::Scan(buf.data(), bytesRead, type,
        GetAvDatabaseManager().GetDatabase(), result);
    return result;
}

// ---------------------------------------------------------------
// Сканировать директорию рекурсивно
// ---------------------------------------------------------------
DirectoryScanResult ScanManager::ScanDirectory(const wchar_t* dirPath)
{
    DirectoryScanResult dirResult;

    std::wstring pattern = dirPath;
    if (!pattern.empty() && pattern.back() != L'\\')
        pattern += L'\\';
    pattern += L'*';

    WIN32_FIND_DATAW fd{};
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE)
        return dirResult;

    std::wstring base = dirPath;
    if (!base.empty() && base.back() != L'\\')
        base += L'\\';

    do {
        if (wcscmp(fd.cFileName, L".") == 0 ||
            wcscmp(fd.cFileName, L"..") == 0)
            continue;

        std::wstring fullPath = base + fd.cFileName;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Рекурсия
            auto sub = ScanDirectory(fullPath.c_str());
            dirResult.FilesScanned += sub.FilesScanned;
            dirResult.ThreatsFound += sub.ThreatsFound;
            for (auto& t : sub.Threats)
                dirResult.Threats.push_back(t);
        }
        else
        {
            dirResult.FilesScanned++;
            auto r = ScanFile(fullPath.c_str());
            if (r.IsMalicious)
            {
                dirResult.ThreatsFound++;
                dirResult.Threats.push_back(r);
            }
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return dirResult;
}