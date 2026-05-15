#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------
// Скачивает антивирусные базы с бэкенда и сохраняет на диск.
// ---------------------------------------------------------------
class AvDbUpdater
{
public:
    // Скачать публичный ключ (открытый endpoint).
    // Сохраняет в exeDir\avdb_public.key
    static bool DownloadPublicKey(const wchar_t* exeDir);

    // Скачать полную базу (требует Bearer токен).
    // Сохраняет manifest.bin и data.bin в exeDir.
    // Перед сохранением делает резервную копию существующих файлов.
    static bool DownloadFullDatabase(const wchar_t* accessToken,
        const wchar_t* exeDir);

private:
    // Разобрать multipart/mixed ответ -> manifest и data
    static bool ParseMultipart(const std::vector<uint8_t>& body,
        const std::string& boundary,
        std::vector<uint8_t>& manifest,
        std::vector<uint8_t>& data);

    static bool SaveFile(const wchar_t* path,
        const std::vector<uint8_t>& data);
};