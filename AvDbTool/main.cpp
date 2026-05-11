#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <fstream>
#include "AvDbFormat.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

// ---------------------------------------------------------------
// SHA-256 через CNG
// ---------------------------------------------------------------
static bool Sha256(const uint8_t* data, size_t len, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM,
        nullptr, 0) == 0)
    {
        DWORD hashObjSize = 0, cbData = 0;
        BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
            (PUCHAR)&hashObjSize, sizeof(DWORD), &cbData, 0);
        std::vector<uint8_t> hashObj(hashObjSize);

        if (BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjSize,
            nullptr, 0, 0) == 0)
        {
            BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
            ok = (BCryptFinishHash(hHash, out, 32, 0) == 0);
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    return ok;
}

// ---------------------------------------------------------------
// RSA-2048 Sign (SHA-256 + PKCS1)
// ---------------------------------------------------------------
static bool RsaSign(BCRYPT_KEY_HANDLE hKey,
    const uint8_t* data, size_t len,
    uint8_t sig[256])
{
    uint8_t hash[32]{};
    if (!Sha256(data, len, hash)) return false;

    BCRYPT_PKCS1_PADDING_INFO pad{};
    pad.pszAlgId = BCRYPT_SHA256_ALGORITHM;

    ULONG sigLen = 0;
    if (BCryptSignHash(hKey, &pad, hash, 32, nullptr, 0,
        &sigLen, BCRYPT_PAD_PKCS1) != 0)
        return false;

    if (sigLen != 256) return false;

    return BCryptSignHash(hKey, &pad, hash, 32, sig, 256,
        &sigLen, BCRYPT_PAD_PKCS1) == 0;
}

// ---------------------------------------------------------------
// Генерация RSA-2048 ключевой пары
// ---------------------------------------------------------------
static bool GenerateKeyPair(const wchar_t* privKeyFile,
    const wchar_t* pubKeyFile)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM,
        nullptr, 0) != 0)
        return false;

    bool ok = false;
    if (BCryptGenerateKeyPair(hAlg, &hKey, 2048, 0) == 0 &&
        BCryptFinalizeKeyPair(hKey, 0) == 0)
    {
        // Экспорт приватного ключа (PKCS#8 DER)
        ULONG privLen = 0;
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB,
            nullptr, 0, &privLen, 0);
        std::vector<uint8_t> privBlob(privLen);
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB,
            privBlob.data(), privLen, &privLen, 0);

        // Экспорт публичного ключа
        ULONG pubLen = 0;
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB,
            nullptr, 0, &pubLen, 0);
        std::vector<uint8_t> pubBlob(pubLen);
        BCryptExportKey(hKey, nullptr, BCRYPT_RSAPUBLIC_BLOB,
            pubBlob.data(), pubLen, &pubLen, 0);

        // Сохраняем в файлы
        std::ofstream fPriv(privKeyFile, std::ios::binary);
        fPriv.write((char*)privBlob.data(), privBlob.size());
        fPriv.close();

        std::ofstream fPub(pubKeyFile, std::ios::binary);
        fPub.write((char*)pubBlob.data(), pubBlob.size());
        fPub.close();

        wprintf(L"Private key: %s\n", privKeyFile);
        wprintf(L"Public key:  %s\n", pubKeyFile);
        ok = true;
    }

    if (hKey) BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

// ---------------------------------------------------------------
// Загрузить приватный ключ из файла
// ---------------------------------------------------------------
static BCRYPT_KEY_HANDLE LoadPrivateKey(const wchar_t* file)
{
    std::ifstream f(file, std::ios::binary | std::ios::ate);
    if (!f) return nullptr;
    size_t sz = (size_t)f.tellg(); f.seekg(0);
    std::vector<uint8_t> blob(sz);
    f.read((char*)blob.data(), sz);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RSA_ALGORITHM,
        nullptr, 0) != 0)
        return nullptr;

    BCryptImportKeyPair(hAlg, nullptr, BCRYPT_RSAFULLPRIVATE_BLOB,
        &hKey, blob.data(), (ULONG)sz, 0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return hKey;
}

// ---------------------------------------------------------------
// Построить запись + подписать
// ---------------------------------------------------------------
static bool BuildRecord(BCRYPT_KEY_HANDLE hKey,
    const uint8_t* sigBytes, uint32_t sigLen,
    uint64_t offBegin, uint64_t offEnd,
    uint8_t type,
    const wchar_t* threatName,
    std::vector<uint8_t>& out)
{
    AvDbFileRecord rec{};
    memcpy(&rec.ObjectSignaturePrefix, sigBytes, 8);
    rec.ObjectSignatureLength = sigLen;
    Sha256(sigBytes, sigLen, rec.ObjectSignature);
    rec.OffsetBegin = offBegin;
    rec.OffsetEnd = offEnd;
    rec.ObjectType = type;

    // Подписываем все поля кроме AvRecordSignature
    std::vector<uint8_t> toSign;
    auto app = [&](const void* p, size_t n) {
        auto b = (const uint8_t*)p;
        toSign.insert(toSign.end(), b, b + n);
        };
    app(&rec.ObjectSignaturePrefix, 8);
    app(&rec.ObjectSignatureLength, 4);
    app(rec.ObjectSignature, 32);
    app(&rec.OffsetBegin, 8);
    app(&rec.OffsetEnd, 8);
    app(&rec.ObjectType, 1);

    if (!RsaSign(hKey, toSign.data(), toSign.size(), rec.AvRecordSignature))
        return false;

    // Имя угрозы
    uint16_t nameLen = (uint16_t)(wcslen(threatName) * sizeof(wchar_t));
    rec.ThreatNameLen = nameLen;

    // Записываем в буфер
    out.insert(out.end(), (uint8_t*)&rec, (uint8_t*)&rec + sizeof(rec));
    out.insert(out.end(), (uint8_t*)threatName, (uint8_t*)threatName + nameLen);
    return true;
}

// ---------------------------------------------------------------
// Создать .avdb файл
// ---------------------------------------------------------------
static bool CreateAvDb(const wchar_t* privKeyFile,
    const wchar_t* outFile)
{
    BCRYPT_KEY_HANDLE hKey = LoadPrivateKey(privKeyFile);
    if (!hKey) { wprintf(L"Cannot load private key\n"); return false; }

    // Тестовые сигнатуры
    struct Entry {
        const char* sig;
        uint32_t       sigLen;
        uint64_t       offBegin;
        uint64_t       offEnd;
        uint8_t        type;
        const wchar_t* name;
    } entries[] = {
        { "EICAR-TEST-SIGNATURE-PE",  23, 0,  512,        0, L"Test.EICAR.PE"       },
        { "MALWARE-SCRIPT-SIG",       18, 0,  UINT64_MAX, 1, L"Test.Malware.Script" },
        { "VIRUS_BODY_MARKER",        17, 64, 1024,       0, L"Test.Virus.PE.Body"  },
    };

    // Заголовок
    AvDbFileHeader hdr{};
    memcpy(hdr.Magic, AVDB_MAGIC, 4);
    hdr.Version = AVDB_VERSION;
    hdr.ReleaseDate = 20260511ULL;
    hdr.RecordCount = (uint32_t)(sizeof(entries) / sizeof(entries[0]));

    std::vector<uint8_t> fileData;
    auto app = [&](const void* p, size_t n) {
        auto b = (const uint8_t*)p;
        fileData.insert(fileData.end(), b, b + n);
        };
    app(&hdr, sizeof(hdr));

    // Записи
    for (auto& e : entries)
    {
        std::vector<uint8_t> recData;
        if (!BuildRecord(hKey, (const uint8_t*)e.sig, e.sigLen,
            e.offBegin, e.offEnd, e.type, e.name, recData))
        {
            wprintf(L"Failed to build record for %s\n", e.name);
            BCryptDestroyKey(hKey);
            return false;
        }
        fileData.insert(fileData.end(), recData.begin(), recData.end());
    }

    // Подпись всего файла
    uint8_t dbSig[256]{};
    RsaSign(hKey, fileData.data(), fileData.size(), dbSig);
    fileData.insert(fileData.end(), dbSig, dbSig + 256);

    BCryptDestroyKey(hKey);

    // Запись в файл
    std::ofstream f(outFile, std::ios::binary);
    if (!f) { wprintf(L"Cannot write %s\n", outFile); return false; }
    f.write((char*)fileData.data(), fileData.size());
    wprintf(L"Created: %s (%zu bytes, %u records)\n",
        outFile, fileData.size(), hdr.RecordCount);
    return true;
}

// ---------------------------------------------------------------
// main
// ---------------------------------------------------------------
int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        wprintf(L"Usage:\n");
        wprintf(L"  AvDbTool.exe genkey                     -- generate key pair\n");
        wprintf(L"  AvDbTool.exe create <privkey> <out.avdb> -- create database\n");
        return 1;
    }

    if (wcscmp(argv[1], L"genkey") == 0)
    {
        return GenerateKeyPair(L"avdb_private.key", L"avdb_public.key") ? 0 : 1;
    }
    else if (wcscmp(argv[1], L"create") == 0 && argc >= 4)
    {
        return CreateAvDb(argv[2], argv[3]) ? 0 : 1;
    }

    wprintf(L"Unknown command\n");
    return 1;
}