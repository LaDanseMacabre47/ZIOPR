// AvDbTool - утилита генерации ключей и тестовых баз
// Формат: manifest.bin + data.bin (Big-Endian, как на бэкенде)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>

#pragma comment(lib, "bcrypt.lib")

// ---------------------------------------------------------------
// Big-Endian write helpers
// ---------------------------------------------------------------
static void W8(std::vector<uint8_t>& v, uint8_t  x) { v.push_back(x); }
static void W16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x >> 8); v.push_back(x & 0xff); }
static void W32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((x >> 24) & 0xff); v.push_back((x >> 16) & 0xff);
    v.push_back((x >> 8) & 0xff);  v.push_back(x & 0xff);
}
static void W64(std::vector<uint8_t>& v, uint64_t x) {
    for (int i = 7; i >= 0; i--) v.push_back((x >> (i * 8)) & 0xff);
}
static void W64s(std::vector<uint8_t>& v, int64_t x) { W64(v, (uint64_t)x); }
static void WBytes(std::vector<uint8_t>& v, const void* p, size_t n) {
    auto b = (const uint8_t*)p; v.insert(v.end(), b, b + n);
}
static void WStr(std::vector<uint8_t>& v, const char* s) {
    while (*s) v.push_back((uint8_t)*s++);
    v.push_back(0); // null terminator
}

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
// RSA Sign
// ---------------------------------------------------------------
static bool RsaSign(BCRYPT_KEY_HANDLE hKey,
    const uint8_t* data, size_t len,
    std::vector<uint8_t>& sig)
{
    uint8_t hash[32]{};
    if (!Sha256(data, len, hash)) return false;
    BCRYPT_PKCS1_PADDING_INFO pad{}; pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;
    ULONG sigLen = 0;
    if (BCryptSignHash(hKey, &pad, hash, 32, nullptr, 0, &sigLen, BCRYPT_PAD_PKCS1) != 0)
        return false;
    sig.resize(sigLen);
    return BCryptSignHash(hKey, &pad, hash, 32, sig.data(), sigLen, &sigLen,
        BCRYPT_PAD_PKCS1) == 0;
}

// ---------------------------------------------------------------
// Generate RSA-2048 key pair
// ---------------------------------------------------------------
static bool GenerateKeyPair(const wchar_t* privFile, const wchar_t* pubFile)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM, nullptr, 0) != 0)
        return false;

    if (BCryptGenerateKeyPair(hAlg, &hKey, 2048, 0) == 0 &&
        BCryptFinalizeKeyPair(hKey, 0) == 0)
    {
        ULONG n = 0;
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, nullptr, 0, &n, 0);
        std::vector<uint8_t> priv(n);
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB, priv.data(), n, &n, 0);

        ULONG m = 0;
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB, nullptr, 0, &m, 0);
        std::vector<uint8_t> pub(m);
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB, pub.data(), m, &m, 0);

        { std::ofstream f(privFile, std::ios::binary); f.write((char*)priv.data(), priv.size()); }
        { std::ofstream f(pubFile, std::ios::binary); f.write((char*)pub.data(), pub.size()); }

        wprintf(L"Private key: %s\n", privFile);
        wprintf(L"Public key:  %s\n", pubFile);
        ok = true;
    }
    if (hKey) BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ---------------------------------------------------------------
// Load private key
// ---------------------------------------------------------------
static BCRYPT_KEY_HANDLE LoadPrivKey(const wchar_t* file)
{
    std::ifstream f(file, std::ios::binary | std::ios::ate);
    if (!f) return nullptr;
    size_t sz = (size_t)f.tellg(); f.seekg(0);
    std::vector<uint8_t> blob(sz);
    f.read((char*)blob.data(), sz);
    BCRYPT_ALG_HANDLE hAlg = nullptr; BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM, nullptr, 0) != 0)
        return nullptr;
    BCryptImportKeyPair(hAlg, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB,
        &hKey, blob.data(), (ULONG)sz, 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hKey;
}

// ---------------------------------------------------------------
// Create manifest.bin + data.bin
// ---------------------------------------------------------------
struct TestRecord {
    const char* threatName;
    const char* firstBytes;    // raw signature bytes
    uint32_t    fbLen;
    uint32_t    remLen;        // remainder length (0 = no remainder)
    uint8_t     fileType;      // 0=PE, 1=Script
    int64_t     offsetStart;
    int64_t     offsetEnd;
};

static bool CreateDatabase(const wchar_t* privKeyFile,
    const wchar_t* manifestFile,
    const wchar_t* dataFile)
{
    BCRYPT_KEY_HANDLE hKey = LoadPrivKey(privKeyFile);
    if (!hKey) { wprintf(L"Cannot load private key\n"); return false; }

    // Test records
    TestRecord records[] = {
        {"Test.EICAR.PE",        "EICAR-TEST-SIGNATURE-PE", 23, 0, 0, 0, 512},
        {"Test.Malware.Script",  "MALWARE-SCRIPT-SIG",      18, 0, 1, 0, INT64_MAX},
        {"Test.Virus.PE.Body",   "VIRUS_BODY_MARKER",       17, 0, 0, 64, 1024},
    };
    uint32_t recCount = (uint32_t)(sizeof(records) / sizeof(records[0]));

    // -------------------------------------------------------
    // Build data.bin
    // -------------------------------------------------------
    std::vector<uint8_t> dataBin;
    WStr(dataBin, "DB-Zakharov");
    W8(dataBin, 1);          // version
    W32(dataBin, recCount);   // recordCount

    // Track each record's offset and raw bytes for manifest
    struct RecordInfo { uint64_t offset; uint32_t length; };
    std::vector<RecordInfo> recInfos;

    for (auto& r : records)
    {
        uint64_t recOffset = (uint64_t)dataBin.size() -
            (uint64_t)(12 + 1 + 4); // relative to payload start
        size_t startPos = dataBin.size();

        // threatName
        uint32_t nameLen = (uint32_t)strlen(r.threatName);
        W32(dataBin, nameLen);
        WBytes(dataBin, r.threatName, nameLen);

        // firstBytes
        W32(dataBin, r.fbLen);
        WBytes(dataBin, r.firstBytes, r.fbLen);

        // remainderHash (32 bytes, zeros for test)
        uint8_t remHash[32]{};
        WBytes(dataBin, remHash, 32);

        // remainderLength
        W32(dataBin, r.remLen);

        // fileType
        W8(dataBin, r.fileType);

        // offsetStart, offsetEnd
        W64s(dataBin, r.offsetStart);
        W64s(dataBin, r.offsetEnd);

        size_t endPos = dataBin.size();
        recInfos.push_back({ (uint64_t)(startPos - (12 + 1 + 4)),
                              (uint32_t)(endPos - startPos) });
    }

    // SHA-256 of data.bin
    uint8_t dataSha[32]{};
    Sha256(dataBin.data(), dataBin.size(), dataSha);

    // -------------------------------------------------------
    // Build manifest.bin (unsigned part first)
    // -------------------------------------------------------
    std::vector<uint8_t> manifest;
    WStr(manifest, "MF-Zakharov");
    W8(manifest, 1);           // version
    W8(manifest, 1);           // exportType = full
    W64s(manifest, (int64_t)GetTickCount64() + 1746921600000LL); // generatedAt
    W64s(manifest, -1LL);       // since = -1 (full dump)
    W32(manifest, recCount);    // recordCount
    WBytes(manifest, dataSha, 32); // dataSha256

    // entries
    for (uint32_t i = 0; i < recCount; i++)
    {
        // UUID: 16 zero bytes (test)
        uint8_t uuid[16]{};
        uuid[15] = (uint8_t)(i + 1);
        WBytes(manifest, uuid, 16);

        W8(manifest, 1); // statusCode = ACTUAL
        W64s(manifest, 1746921600000LL); // updatedAt
        W64(manifest, recInfos[i].offset);  // dataOffset
        W32(manifest, recInfos[i].length);  // dataLength

        // Sign the record bytes from data.bin
        const uint8_t* recData = dataBin.data() +
            (12 + 1 + 4) + recInfos[i].offset; // header + offset
        std::vector<uint8_t> recSig;
        RsaSign(hKey, recData, recInfos[i].length, recSig);

        W32(manifest, (uint32_t)recSig.size());
        WBytes(manifest, recSig.data(), recSig.size());
    }

    // Sign the entire manifest (unsigned part)
    std::vector<uint8_t> mSig;
    RsaSign(hKey, manifest.data(), manifest.size(), mSig);
    W32(manifest, (uint32_t)mSig.size());
    WBytes(manifest, mSig.data(), mSig.size());

    BCryptDestroyKey(hKey);

    // Write files
    {
        std::ofstream f(manifestFile, std::ios::binary);
        f.write((char*)manifest.data(), manifest.size());
    }
    {
        std::ofstream f(dataFile, std::ios::binary);
        f.write((char*)dataBin.data(), dataBin.size());
    }

    wprintf(L"manifest.bin: %zu bytes\n", manifest.size());
    wprintf(L"data.bin:     %zu bytes, %u records\n", dataBin.size(), recCount);
    return true;
}

// ---------------------------------------------------------------
int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        wprintf(L"Usage:\n");
        wprintf(L"  AvDbTool.exe genkey                               -- generate key pair\n");
        wprintf(L"  AvDbTool.exe create <privkey> <manifest> <data>   -- create database\n");
        return 1;
    }

    if (wcscmp(argv[1], L"genkey") == 0)
        return GenerateKeyPair(L"avdb_private.key", L"avdb_public.key") ? 0 : 1;

    if (wcscmp(argv[1], L"create") == 0 && argc >= 5)
        return CreateDatabase(argv[2], argv[3], argv[4]) ? 0 : 1;

    wprintf(L"Unknown command\n");
    return 1;
}