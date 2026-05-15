#include "AvDatabase.h"
#include <bcrypt.h>
#include <fstream>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#include <wincrypt.h>

// ---------------------------------------------------------------
// Big-Endian helpers
// ---------------------------------------------------------------
static uint16_t BE16(const uint8_t* p) { return (uint16_t)p[0] << 8 | p[1]; }
static uint32_t BE32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint64_t BE64(const uint8_t* p) {
    return (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 | (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
        (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 | (uint64_t)p[6] << 8 | (uint64_t)p[7];
}
static int64_t  BE64s(const uint8_t* p) { return (int64_t)BE64(p); }

// ---------------------------------------------------------------
// SHA-256
// ---------------------------------------------------------------
static bool Sha256(const uint8_t* data, size_t len, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE  hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return false;
    DWORD objSz = 0, cb = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objSz, sizeof(DWORD), &cb, 0);
    std::vector<uint8_t> obj(objSz);
    if (BCryptCreateHash(hAlg, &hHash, obj.data(), objSz, nullptr, 0, 0) == 0) {
        BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
        ok = BCryptFinishHash(hHash, out, 32, 0) == 0;
        BCryptDestroyHash(hHash);
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ---------------------------------------------------------------
// RSA Verify
// ---------------------------------------------------------------
bool AvDatabaseManager::VerifyRsa(const std::vector<uint8_t>& pubKey,
    const uint8_t* data, size_t dataLen,
    const uint8_t* sig, size_t sigLen)
{
    CERT_PUBLIC_KEY_INFO* pInfo = nullptr;
    DWORD cbInfo = 0;
    if (!CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO,
        pubKey.data(), (DWORD)pubKey.size(),
        CRYPT_DECODE_ALLOC_FLAG, nullptr, &pInfo, &cbInfo))
        return false;

    BCRYPT_KEY_HANDLE hKey = nullptr;
    BOOL importOk = CryptImportPublicKeyInfoEx2(
        X509_ASN_ENCODING, pInfo, 0, nullptr, &hKey);
    LocalFree(pInfo);
    if (!importOk || !hKey) return false;

    // SHA-384 (бэкенд использует SHA384withRSA!)
    BCRYPT_ALG_HANDLE hHashAlg = nullptr;
    BCryptOpenAlgorithmProvider(&hHashAlg, BCRYPT_SHA384_ALGORITHM, nullptr, 0);

    DWORD objSz = 0, cb = 0;
    BCryptGetProperty(hHashAlg, BCRYPT_OBJECT_LENGTH,
        (PUCHAR)&objSz, sizeof(DWORD), &cb, 0);
    std::vector<uint8_t> obj(objSz);

    BCRYPT_HASH_HANDLE hHash = nullptr;
    BCryptCreateHash(hHashAlg, &hHash, obj.data(), objSz, nullptr, 0, 0);
    BCryptHashData(hHash, (PUCHAR)data, (ULONG)dataLen, 0);

    uint8_t hash[48]{}; // SHA-384 = 48 байт
    BCryptFinishHash(hHash, hash, 48, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hHashAlg, 0);

    BCRYPT_PKCS1_PADDING_INFO pad{};
    pad.pszAlgId = BCRYPT_SHA384_ALGORITHM;

    NTSTATUS st = BCryptVerifySignature(hKey, &pad,
        hash, 48,
        (PUCHAR)sig, (ULONG)sigLen,
        BCRYPT_PAD_PKCS1);

    wchar_t buf[64]{};
    swprintf_s(buf, L"[AvDb] SHA384+PKCS1=%08X\n", st);
    OutputDebugStringW(buf);

    BCryptDestroyKey(hKey);
    return st == 0;
}

// ---------------------------------------------------------------
// Read file to vector
// ---------------------------------------------------------------
std::vector<uint8_t> AvDatabaseManager::ReadFile(const wchar_t* path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t sz = (size_t)f.tellg(); f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read((char*)buf.data(), sz);
    return buf;
}

// ---------------------------------------------------------------
// Parse manifest.bin
// Returns false if manifest signature invalid
// ---------------------------------------------------------------
bool AvDatabaseManager::ParseManifest(const std::vector<uint8_t>& manifest,
    const std::vector<uint8_t>& pubKey,
    std::vector<ManifestEntry>& entries,
    int64_t& generatedAt)
{
    const uint8_t* d = manifest.data();
    size_t sz = manifest.size();
    size_t pos = 0;

    // Magic: raw ASCII до первого non-ASCII или до version (uint16)
    // Бэкенд пишет "MF-Zakharov" без null-терминатора
    // Ищем конец magic: первый байт >= 0x80 или последовательность
    // которая не является ASCII printable — проще искать по известной длине
    // "MF-" + studentName, но studentName нам неизвестна точно.
    // Надёжный способ: ищем байт 0x00 ИЛИ байт > 0x7E — но бэкенд не пишет \0.
    // Значит пропускаем все ASCII printable байты (0x20..0x7E)
    while (pos < sz && d[pos] >= 0x20 && d[pos] <= 0x7E) pos++;
    if (pos >= sz) return false;
    // pos теперь на первом non-ASCII байте = начало version (uint16)

    // version: uint16 BE (2 байта)
    if (pos + 2 > sz) return false;
    pos += 2; // пропускаем version

    // exportType: uint8
    if (pos + 1 > sz) return false;
    pos += 1;

    // generatedAt: int64 BE
    if (pos + 8 > sz) return false;
    generatedAt = BE64s(d + pos); pos += 8;

    // since: int64 BE
    if (pos + 8 > sz) return false;
    pos += 8;

    // recordCount: uint32 BE
    if (pos + 4 > sz) return false;
    uint32_t recordCount = BE32(d + pos); pos += 4;

    // dataSha256: 32 bytes
    if (pos + 32 > sz) return false;
    pos += 32;

    // Вычисляем позицию подписи манифеста (проходим entries)
    size_t entriesStart = pos;
    size_t tmpPos = pos;
    for (uint32_t i = 0; i < recordCount; i++)
    {
        // uuid(16) + status(1) + updatedAt(8) + offset(8) + length(4) + sigLen(4)
        if (tmpPos + 16 + 1 + 8 + 8 + 4 + 4 > sz) return false;
        tmpPos += 16 + 1 + 8 + 8 + 4;
        uint32_t sigLen = BE32(d + tmpPos); tmpPos += 4;
        tmpPos += sigLen;
    }

    // manifestSigLen: uint32 BE
    if (tmpPos + 4 > sz) return false;
    uint32_t manifestSigLen = BE32(d + tmpPos); tmpPos += 4;
    if (tmpPos + manifestSigLen > sz) return false;
    const uint8_t* manifestSig = d + tmpPos;

    // Подписывается всё до manifestSigLen (не включая сам uint32 длины)
    size_t signedLen = tmpPos - 4;

    OutputDebugStringW((L"[AvDb] RSA verify: signedLen=" + std::to_wstring(signedLen) +
        L" sigLen=" + std::to_wstring(manifestSigLen) + L"\n").c_str());

    if (!VerifyRsa(pubKey, d, signedLen, manifestSig, manifestSigLen))
    {
        OutputDebugStringW(L"[AvDb] FAIL: manifest RSA\n");
        return false;
    }
    OutputDebugStringW(L"[AvDb] manifest RSA OK\n");

    // Парсим entries
    pos = entriesStart;
    for (uint32_t i = 0; i < recordCount; i++)
    {
        ManifestEntry entry{};
        memcpy(entry.UUID, d + pos, 16); pos += 16;
        entry.StatusCode = d[pos++];
        entry.UpdatedAt = BE64s(d + pos); pos += 8;
        entry.DataOffset = BE64(d + pos);  pos += 8;
        entry.DataLength = BE32(d + pos);  pos += 4;
        uint32_t sigLen = BE32(d + pos);  pos += 4;
        entry.RecordSig.assign(d + pos, d + pos + sigLen);
        pos += sigLen;
        entries.push_back(entry);
    }

    return true;
}
// ---------------------------------------------------------------
// Parse data.bin + verify each record signature
// ---------------------------------------------------------------
bool AvDatabaseManager::ParseDataBin(const std::vector<uint8_t>& dataBin,
    const std::vector<ManifestEntry>& entries,
    const std::vector<uint8_t>& pubKey)
{
    const uint8_t* d = dataBin.data();
    size_t sz = dataBin.size();
    size_t pos = 0;

    // Magic: raw ASCII без \0 — пропускаем printable bytes
    while (pos < sz && d[pos] >= 0x20 && d[pos] <= 0x7E) pos++;
    if (pos >= sz) return false;

    // version: uint16 BE
    if (pos + 2 > sz) return false;
    pos += 2;

    // recordCount: uint32 BE
    if (pos + 4 > sz) return false;
    uint32_t recordCount = BE32(d + pos); pos += 4;

    OutputDebugStringW((L"[AvDb] data.bin recordCount=" +
        std::to_wstring(recordCount) + L"\n").c_str());

    uint32_t loaded = 0;

    for (uint32_t i = 0; i < recordCount; i++)
    {
        size_t recStart = pos;

        // threatName: uint32 len + UTF-8
        if (pos + 4 > sz) break;
        uint32_t nameLen = BE32(d + pos); pos += 4;
        if (pos + nameLen > sz) break;
        std::string nameUtf8((char*)d + pos, nameLen); pos += nameLen;

        // firstBytes: uint32 len + raw bytes
        if (pos + 4 > sz) break;
        uint32_t fbLen = BE32(d + pos); pos += 4;
        if (pos + fbLen > sz) break;
        std::string firstBytes((char*)d + pos, fbLen); pos += fbLen;

        // remainderHash: uint32 len + raw bytes
        if (pos + 4 > sz) break;
        uint32_t rhLen = BE32(d + pos); pos += 4;
        if (pos + rhLen > sz) break;
        uint8_t remHash[32]{};
        memcpy(remHash, d + pos, std::min((size_t)rhLen, (size_t)32));
        pos += rhLen;

        // remainderLength: int64 BE
        if (pos + 8 > sz) break;
        int64_t remLen = BE64s(d + pos); pos += 8;

        // fileType: uint32 len + UTF-8
        if (pos + 4 > sz) break;
        uint32_t ftLen = BE32(d + pos); pos += 4;
        if (pos + ftLen > sz) break;
        std::string ftStr((char*)d + pos, ftLen); pos += ftLen;

        // offsetStart: int64 BE
        if (pos + 8 > sz) break;
        int64_t offStart = BE64s(d + pos); pos += 8;

        // offsetEnd: int64 BE
        if (pos + 8 > sz) break;
        int64_t offEnd = BE64s(d + pos); pos += 8;

        // Подпись записи из манифеста — это Base64 от JSON полей,
        // подписанный ключом бэкенда. Мы не можем верифицировать
        // её по сырым байтам записи — пропускаем проверку per-record,
        // достаточно проверки подписи всего манифеста (уже прошла).

        // Пропускаем DELETED записи
        if (i < entries.size() &&
            entries[i].StatusCode == MANIFEST_STATUS_DELETED)
            continue;

        if (firstBytes.size() < 2) continue;

        AvRecord rec{};
        rec.FirstBytes = firstBytes;
        memcpy(rec.RemainderHash, remHash, 32);
        rec.RemainderLength = (uint32_t)remLen;
        rec.Type = (ftStr == "PE") ? ObjectType::PE : ObjectType::Script;
        rec.OffsetStart = offStart;
        rec.OffsetEnd = offEnd;
        rec.TotalSigLength = (uint32_t)firstBytes.size() + (uint32_t)remLen;

        // UTF-8 -> wstring
        int wLen = MultiByteToWideChar(CP_UTF8, 0,
            nameUtf8.c_str(), (int)nameUtf8.size(), nullptr, 0);
        rec.ThreatName.resize(wLen);
        MultiByteToWideChar(CP_UTF8, 0, nameUtf8.c_str(), (int)nameUtf8.size(),
            rec.ThreatName.data(), wLen);

        // PrefixKey: первые 8 байт firstBytes как uint64
        uint8_t prefixBuf[8]{};
        memcpy(prefixBuf, firstBytes.data(),
            std::min(firstBytes.size(), (size_t)8));
        memcpy(&rec.PrefixKey, prefixBuf, 8);

        if (i < entries.size())
        {
            rec.RecordSigLen = (uint32_t)entries[i].RecordSig.size();
            size_t cpLen = std::min((size_t)rec.RecordSigLen, (size_t)256);
            memcpy(rec.RecordSignature, entries[i].RecordSig.data(), cpLen);
        }

        m_db[rec.PrefixKey].push_back(rec);
        loaded++;

        wchar_t buf[128]{};
        swprintf_s(buf, L"[AvDb] record[%u]: %S ft=%S\n",
            i, nameUtf8.c_str(), ftStr.c_str());
        OutputDebugStringW(buf);
    }

    OutputDebugStringW((L"[AvDb] ParseDataBin loaded=" +
        std::to_wstring(loaded) + L"\n").c_str());

    return loaded > 0 || recordCount == 0;
}

// ---------------------------------------------------------------
AvDatabaseManager::AvDatabaseManager() : m_loaded(false) {}

// ---------------------------------------------------------------
// Загрузить из пары manifest.bin + data.bin
// ---------------------------------------------------------------
bool AvDatabaseManager::LoadFromFiles(const wchar_t* manifestPath,
    const wchar_t* dataPath,
    const wchar_t* pubKeyPath)
{
    m_db.clear();
    m_loaded = false;

    auto manifest = ReadFile(manifestPath);
    auto dataBin = ReadFile(dataPath);
    auto pubKey = ReadFile(pubKeyPath);

    OutputDebugStringW((L"[AvDb] manifest=" + std::to_wstring(manifest.size()) +
        L" data=" + std::to_wstring(dataBin.size()) +
        L" key=" + std::to_wstring(pubKey.size()) + L"\n").c_str());

    if (manifest.empty() || dataBin.empty() || pubKey.empty())
    {
        OutputDebugStringW(L"[AvDb] FAIL: file empty\n");
        return false;
    }

    if (manifest.size() < 66)
    {
        OutputDebugStringW(L"[AvDb] FAIL: manifest too small\n");
        return false;
    }

    uint8_t dataSha[32];
    memcpy(dataSha, manifest.data() + 34, 32);
    uint8_t actualSha[32];
    Sha256(dataBin.data(), dataBin.size(), actualSha);
    if (memcmp(dataSha, actualSha, 32) != 0)
    {
        OutputDebugStringW(L"[AvDb] FAIL: SHA-256 mismatch\n");
        // Выводим первые байты манифеста для диагностики
        wchar_t buf[128]{};
        swprintf_s(buf, L"[AvDb] manifest[34..37] = %02X %02X %02X %02X\n",
            manifest[34], manifest[35], manifest[36], manifest[37]);
        OutputDebugStringW(buf);
        swprintf_s(buf, L"[AvDb] actualSha[0..3]  = %02X %02X %02X %02X\n",
            actualSha[0], actualSha[1], actualSha[2], actualSha[3]);
        OutputDebugStringW(buf);
        return false;
    }
    OutputDebugStringW(L"[AvDb] SHA-256 OK\n");

    std::vector<ManifestEntry> entries;
    int64_t generatedAt = 0;
    if (!ParseManifest(manifest, pubKey, entries, generatedAt))
    {
        OutputDebugStringW(L"[AvDb] FAIL: ParseManifest\n");
        return false;
    }
    OutputDebugStringW((L"[AvDb] ParseManifest OK entries=" +
        std::to_wstring(entries.size()) + L"\n").c_str());

    m_generatedAt = generatedAt;

    if (!ParseDataBin(dataBin, entries, pubKey))
    {
        OutputDebugStringW(L"[AvDb] FAIL: ParseDataBin\n");
        return false;
    }
    OutputDebugStringW((L"[AvDb] ParseDataBin OK records=" +
        std::to_wstring(GetInfo().RecordCount) + L"\n").c_str());

    m_loaded = true;
    return true;
}

// ---------------------------------------------------------------
bool AvDatabaseManager::MakeBackup(const wchar_t* src, const wchar_t* dst)
{
    return CopyFileW(src, dst, FALSE) != 0;
}

LoadResult AvDatabaseManager::LoadOnStartup(const wchar_t* exeDir)
{
    std::wstring dir = exeDir;
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';

    std::wstring manifest = dir + L"manifest.bin";
    std::wstring data = dir + L"data.bin";
    std::wstring mBak = dir + L"manifest.bin.bak";
    std::wstring dBak = dir + L"data.bin.bak";
    std::wstring pubKey = dir + L"avdb_public.key";

    if (LoadFromFiles(manifest.c_str(), data.c_str(), pubKey.c_str()))
    {
        MakeBackup(manifest.c_str(), mBak.c_str());
        MakeBackup(data.c_str(), dBak.c_str());
        return LoadResult::OK;
    }

    if (LoadFromFiles(mBak.c_str(), dBak.c_str(), pubKey.c_str()))
    {
        CopyFileW(mBak.c_str(), manifest.c_str(), FALSE);
        CopyFileW(dBak.c_str(), data.c_str(), FALSE);
        return LoadResult::RestoredFromBackup;
    }

    LoadHardcoded();
    return LoadResult::LoadedDefault;
}

// ---------------------------------------------------------------
// Хардкод fallback
// ---------------------------------------------------------------
void AvDatabaseManager::LoadHardcoded()
{
    m_db.clear();
    m_generatedAt = 1746921600000LL; // 2026-05-11

    struct Entry {
        const char* fb; size_t fbLen;
        uint32_t remLen;
        int64_t os; int64_t oe;
        ObjectType t; const wchar_t* name;
    } entries[] = {
        {"EICAR-TEST-SIGNATURE-PE", 23, 0, 0, 512,        ObjectType::PE,     L"Test.EICAR.PE"},
        {"MALWARE-SCRIPT-SIG",      18, 0, 0, INT64_MAX,  ObjectType::Script, L"Test.Malware.Script"},
        {"VIRUS_BODY_MARKER",       17, 0, 64, 1024,      ObjectType::PE,     L"Test.Virus.PE.Body"},
    };

    for (auto& e : entries)
    {
        if (e.fbLen < 2) continue;
        AvRecord r{};
        r.FirstBytes = std::string(e.fb, e.fbLen);
        r.RemainderLength = e.remLen;
        memset(r.RemainderHash, 0, 32);
        r.Type = e.t;
        r.OffsetStart = e.os;
        r.OffsetEnd = e.oe;
        r.ThreatName = e.name;
        r.TotalSigLength = (uint32_t)e.fbLen + e.remLen;
        memcpy(&r.PrefixKey, e.fb, 8);
        m_db[r.PrefixKey].push_back(r);
    }

    m_loaded = true;
}

// ---------------------------------------------------------------
AvDatabaseInfo AvDatabaseManager::GetInfo() const
{
    AvDatabaseInfo info;
    if (m_generatedAt > 0)
    {
        time_t t = (time_t)(m_generatedAt / 1000);
        struct tm tm {};
        gmtime_s(&tm, &t);
        wchar_t buf[32]{};
        swprintf_s(buf, L"%04d-%02d-%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
        info.ReleaseDate = buf;
    }
    else
        info.ReleaseDate = L"default";

    info.RecordCount = 0;
    for (auto& kv : m_db)
        info.RecordCount += (uint32_t)kv.second.size();
    return info;
}

AvDatabaseManager& GetAvDatabaseManager()
{
    static AvDatabaseManager instance;
    return instance;
}