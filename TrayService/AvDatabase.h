#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// Тип файла (fileType из data.bin)
enum class ObjectType : uint8_t { PE = 0, Script = 1 };
constexpr uint8_t MANIFEST_STATUS_DELETED = 0;
constexpr uint8_t MANIFEST_STATUS_ACTUAL = 1;
// Одна запись антивирусной базы в памяти
struct AvRecord
{
    // Из data.bin
    std::string  FirstBytes;           // raw bytes (первые байты сигнатуры)
    uint8_t      RemainderHash[32];    // SHA-256 хвоста
    uint32_t     RemainderLength;      // длина хвоста
    ObjectType   Type;
    uint32_t     RemainderHashLen = 0;
    int64_t      OffsetStart;
    int64_t      OffsetEnd;
    std::wstring ThreatName;

    // Из manifest.bin
    uint8_t      RecordSignature[256]; // RSA подпись записи
    uint32_t     RecordSigLen;

    // Для движка сканирования (производные поля)
    uint64_t     PrefixKey;            // первые 8 байт FirstBytes как uint64
    uint32_t     TotalSigLength;       // len(FirstBytes) + RemainderLength
};

using AvDatabase = std::map<uint64_t, std::vector<AvRecord>>;

struct AvDatabaseInfo
{
    std::wstring ReleaseDate;    // из generatedAtEpochMs
    uint32_t     RecordCount = 0;
};

enum class LoadResult
{
    OK,
    RestoredFromBackup,
    LoadedDefault,
    Failed,
    NeedsUpdate  // <-- новый: ЭЦП плохая, нужно обновление с сети
};

class AvDatabaseManager
{
public:
    AvDatabaseManager();
    ~AvDatabaseManager() = default;

    // Загрузить при запуске: файл -> резервная копия -> хардкод
    LoadResult LoadOnStartup(const wchar_t* exeDir);

    // Загрузить из пары файлов manifest.bin + data.bin
    bool LoadFromFiles(const wchar_t* manifestPath,
        const wchar_t* dataPath,
        const wchar_t* pubKeyPath);

    // Загрузить хардкод (fallback)
    void LoadHardcoded();

    const AvDatabase& GetDatabase() const { return m_db; }
    AvDatabaseInfo    GetInfo()     const;
    bool              IsLoaded()    const { return m_loaded; }

private:
    bool VerifyRsa(const std::vector<uint8_t>& pubKey,
        const uint8_t* data, size_t dataLen,
        const uint8_t* sig, size_t sigLen);

    bool ParseManifest(const std::vector<uint8_t>& manifest,
        const std::vector<uint8_t>& pubKey,
        std::vector<struct ManifestEntry>& entries,
        int64_t& generatedAt);

    bool ParseDataBin(const std::vector<uint8_t>& dataBin,
        const std::vector<struct ManifestEntry>& entries,
        const std::vector<uint8_t>& pubKey);

    static bool MakeBackup(const wchar_t* src, const wchar_t* dst);
    static std::vector<uint8_t> ReadFile(const wchar_t* path);

    AvDatabase   m_db;
    bool         m_loaded = false;
    int64_t      m_generatedAt = 0;   // Unix ms
};

// Запись манифеста (внутренняя структура)
struct ManifestEntry
{
    uint8_t  UUID[16];
    uint8_t  StatusCode;
    int64_t  UpdatedAt;
    uint64_t DataOffset;
    uint32_t DataLength;
    std::vector<uint8_t> RecordSig;
};

AvDatabaseManager& GetAvDatabaseManager();