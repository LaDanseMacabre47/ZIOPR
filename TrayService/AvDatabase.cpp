#include "AvDatabase.h"
#include "..\AvDbTool\AvDbFormat.h"
#include <bcrypt.h>
#include <fstream>
#include <vector>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

// ---------------------------------------------------------------
// SHA-256 через CNG
// ---------------------------------------------------------------
static bool Sha256Cng(const uint8_t* data, size_t len, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE  hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
        nullptr, 0) != 0)
        return false;

    DWORD objSz = 0, cb = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
        (PUCHAR)&objSz, sizeof(DWORD), &cb, 0);
    std::vector<uint8_t> obj(objSz);

    if (BCryptCreateHash(hAlg, &hHash, obj.data(), objSz,
        nullptr, 0, 0) == 0)
    {
        BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
        ok = (BCryptFinishHash(hHash, out, 32, 0) == 0);
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ---------------------------------------------------------------
// RSA-2048 Verify (SHA-256 + PKCS1)
// ---------------------------------------------------------------
bool AvDatabaseManager::VerifyRsaSignature(
    const uint8_t* pubKeyBlob, ULONG pubKeyLen,
    const uint8_t* data, size_t dataLen,
    const uint8_t  sig[256])
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM,
        nullptr, 0) != 0)
        return false;

    if (BCryptImportKeyPair(hAlg, nullptr, BCRYPT_RSAPUBLIC_BLOB,
        &hKey, (PUCHAR)pubKeyBlob, pubKeyLen, 0) == 0)
    {
        uint8_t hash[32]{};
        Sha256Cng(data, dataLen, hash);

        BCRYPT_PKCS1_PADDING_INFO pad{};
        pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;

        ok = (BCryptVerifySignature(hKey, &pad,
            hash, 32,
            (PUCHAR)sig, 256,
            BCRYPT_PAD_PKCS1) == 0);
        BCryptDestroyKey(hKey);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ---------------------------------------------------------------
AvDatabaseManager::AvDatabaseManager() : m_loaded(false) {}

void AvDatabaseManager::AddRecord(const AvRecord& rec)
{
    m_db[rec.ObjectSignaturePrefix].push_back(rec);
}

// ---------------------------------------------------------------
// Загрузить из .avdb файла с проверкой RSA подписи
// ---------------------------------------------------------------
bool AvDatabaseManager::Load(const wchar_t* avdbPath,
    const wchar_t* pubKeyPath)
{
    m_db.clear();
    m_loaded = false;

    // Читаем публичный ключ
    std::ifstream fKey(pubKeyPath, std::ios::binary | std::ios::ate);
    if (!fKey) return false;
    size_t keySize = (size_t)fKey.tellg(); fKey.seekg(0);
    std::vector<uint8_t> pubKey(keySize);
    fKey.read((char*)pubKey.data(), keySize);
    fKey.close();

    // Читаем .avdb файл
    std::ifstream fDb(avdbPath, std::ios::binary | std::ios::ate);
    if (!fDb) return false;
    size_t fileSize = (size_t)fDb.tellg(); fDb.seekg(0);
    if (fileSize < sizeof(AvDbFileHeader) + AVDB_RSA_SIG_LEN)
        return false;

    std::vector<uint8_t> fileData(fileSize);
    fDb.read((char*)fileData.data(), fileSize);
    fDb.close();

    // Проверяем подпись всего файла (последние 256 байт)
    size_t bodySize = fileSize - AVDB_RSA_SIG_LEN;
    const uint8_t* dbSig = fileData.data() + bodySize;

    if (!VerifyRsaSignature(pubKey.data(), (ULONG)pubKey.size(),
        fileData.data(), bodySize, dbSig))
        return false;  // подпись базы не прошла

    // Парсим заголовок
    size_t pos = 0;
    AvDbFileHeader hdr{};
    memcpy(&hdr, fileData.data() + pos, sizeof(hdr));
    pos += sizeof(hdr);

    if (memcmp(hdr.Magic, AVDB_MAGIC, 4) != 0) return false;
    if (hdr.Version != AVDB_VERSION)            return false;

    m_releaseDate = hdr.ReleaseDate;
    m_recordCount = hdr.RecordCount;

    // Парсим записи
    for (uint32_t i = 0; i < hdr.RecordCount; i++)
    {
        if (pos + sizeof(AvDbFileRecord) > bodySize) return false;

        AvDbFileRecord fileRec{};
        memcpy(&fileRec, fileData.data() + pos, sizeof(fileRec));
        pos += sizeof(fileRec);

        // Читаем имя угрозы
        if (pos + fileRec.ThreatNameLen > bodySize) return false;
        std::wstring threatName(
            (wchar_t*)(fileData.data() + pos),
            fileRec.ThreatNameLen / sizeof(wchar_t));
        pos += fileRec.ThreatNameLen;

        // Проверяем подпись записи
        std::vector<uint8_t> recData;
        auto app = [&](const void* p, size_t n) {
            auto b = (const uint8_t*)p;
            recData.insert(recData.end(), b, b + n);
            };
        app(&fileRec.ObjectSignaturePrefix, 8);
        app(&fileRec.ObjectSignatureLength, 4);
        app(fileRec.ObjectSignature, 32);
        app(&fileRec.OffsetBegin, 8);
        app(&fileRec.OffsetEnd, 8);
        app(&fileRec.ObjectType, 1);

        if (!VerifyRsaSignature(pubKey.data(), (ULONG)pubKey.size(),
            recData.data(), recData.size(),
            fileRec.AvRecordSignature))
            continue;  // подпись записи не прошла — пропускаем

        // Добавляем в базу
        AvRecord rec{};
        rec.ObjectSignaturePrefix = fileRec.ObjectSignaturePrefix;
        rec.ObjectSignatureLength = fileRec.ObjectSignatureLength;
        memcpy(rec.ObjectSignature, fileRec.ObjectSignature, 32);
        memcpy(rec.AvRecordSignature, fileRec.AvRecordSignature, 256);
        rec.OffsetBegin = fileRec.OffsetBegin;
        rec.OffsetEnd = fileRec.OffsetEnd;
        rec.Type = (ObjectType)fileRec.ObjectType;
        rec.ThreatName = threatName;

        AddRecord(rec);
    }

    m_loaded = true;
    return true;
}

// ---------------------------------------------------------------
// Fallback: хардкод для тестов без файла баз
// ---------------------------------------------------------------
static bool Sha256Win(const uint8_t* data, size_t len, uint8_t out[32])
{
    return Sha256Cng(data, len, out);
}

void AvDatabaseManager::LoadHardcoded()
{
    m_db.clear();
    m_releaseDate = 20260511ULL;
    m_recordCount = 3;

    struct Entry {
        const char* sig; uint32_t len;
        uint64_t ob; uint64_t oe;
        ObjectType t; const wchar_t* name;
    } entries[] = {
        {"EICAR-TEST-SIGNATURE-PE", 23, 0, 512,        ObjectType::PE,     L"Test.EICAR.PE"},
        {"MALWARE-SCRIPT-SIG",      18, 0, UINT64_MAX, ObjectType::Script, L"Test.Malware.Script"},
        {"VIRUS_BODY_MARKER",       17, 64, 1024,      ObjectType::PE,     L"Test.Virus.PE.Body"},
    };

    for (auto& e : entries)
    {
        AvRecord r{};
        memcpy(&r.ObjectSignaturePrefix, e.sig, 8);
        r.ObjectSignatureLength = e.len;
        Sha256Win((const uint8_t*)e.sig, e.len, r.ObjectSignature);
        r.OffsetBegin = e.ob;
        r.OffsetEnd = e.oe;
        r.Type = e.t;
        r.ThreatName = e.name;
        memset(r.AvRecordSignature, 0, 256);  // нет подписи в хардкоде
        AddRecord(r);
    }

    m_loaded = true;
}

// ---------------------------------------------------------------
AvDatabaseInfo AvDatabaseManager::GetInfo() const
{
    AvDatabaseInfo info;
    // Форматируем дату из YYYYMMDD
    if (m_releaseDate > 0)
    {
        wchar_t buf[32]{};
        uint64_t d = m_releaseDate;
        swprintf_s(buf, L"%04llu-%02llu-%02llu",
            d / 10000, (d % 10000) / 100, d % 100);
        info.ReleaseDate = buf;
    }
    else
        info.ReleaseDate = L"unknown";

    info.RecordCount = 0;
    for (auto& kv : m_db)
        info.RecordCount += (uint32_t)kv.second.size();
    return info;
}

// ---------------------------------------------------------------
AvDatabaseManager& GetAvDatabaseManager()
{
    static AvDatabaseManager instance;
    return instance;
}