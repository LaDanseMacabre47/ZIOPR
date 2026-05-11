#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

enum class ObjectType : uint8_t { PE = 0, Script = 1 };

struct AvRecord
{
    uint64_t   ObjectSignaturePrefix;
    uint32_t   ObjectSignatureLength;
    uint8_t    ObjectSignature[32];     // SHA-256
    uint64_t   OffsetBegin;
    uint64_t   OffsetEnd;
    ObjectType Type;
    uint8_t    AvRecordSignature[256];  // RSA-2048
    std::wstring ThreatName;
};

using AvDatabase = std::map<uint64_t, std::vector<AvRecord>>;

struct AvDatabaseInfo
{
    std::wstring ReleaseDate;
    uint32_t     RecordCount = 0;
};

class AvDatabaseManager
{
public:
    AvDatabaseManager();
    ~AvDatabaseManager() = default;

    // Загрузить из .avdb файла рядом с exe
    // pubKeyFile — путь к файлу публичного ключа
    bool Load(const wchar_t* avdbPath, const wchar_t* pubKeyPath);

    // Загрузить хардкод (fallback для тестов без файла)
    void LoadHardcoded();

    const AvDatabase& GetDatabase() const { return m_db; }
    AvDatabaseInfo    GetInfo()     const;
    bool              IsLoaded()    const { return m_loaded; }

private:
    bool VerifyRsaSignature(const uint8_t* pubKeyBlob, ULONG pubKeyLen,
        const uint8_t* data, size_t dataLen,
        const uint8_t  sig[256]);

    void AddRecord(const AvRecord& rec);

    AvDatabase   m_db;
    bool         m_loaded = false;
    uint64_t     m_releaseDate = 0;  // YYYYMMDD
    uint32_t     m_recordCount = 0;
};

AvDatabaseManager& GetAvDatabaseManager();