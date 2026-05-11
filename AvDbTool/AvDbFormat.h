#pragma once
#include <cstdint>

// ================================================================
//  Формат бинарного файла антивирусных баз (.avdb)
// ================================================================

#pragma pack(push, 1)

// Заголовок файла
struct AvDbFileHeader
{
    uint8_t  Magic[4];        // "AVDB"
    uint16_t Version;         // = 1
    uint64_t ReleaseDate;     // YYYYMMDD как uint64 (например 20260511)
    uint32_t RecordCount;     // количество записей
};

// Одна запись в файле
struct AvDbFileRecord
{
    uint64_t ObjectSignaturePrefix;   // первые 8 байт сигнатуры
    uint32_t ObjectSignatureLength;   // полная длина сигнатуры
    uint8_t  ObjectSignature[32];     // SHA-256 хеш сигнатуры
    uint64_t OffsetBegin;             // начало интервала
    uint64_t OffsetEnd;               // конец интервала (0 = любой)
    uint8_t  ObjectType;              // 0=PE, 1=Script
    uint8_t  AvRecordSignature[256];  // RSA-2048 подпись записи
    uint16_t ThreatNameLen;           // длина имени угрозы в байтах (UTF-16)
    // далее ThreatNameLen байт UTF-16 строки
};

#pragma pack(pop)

// В конце файла:
// uint8_t DatabaseSignature[256] — RSA-2048 подпись всего файла
//                                   (SHA-256 от заголовка + всех записей)

constexpr uint8_t  AVDB_MAGIC[4] = { 'A', 'V', 'D', 'B' };
constexpr uint16_t AVDB_VERSION = 1;
constexpr size_t   AVDB_RSA_SIG_LEN = 256;  // RSA-2048