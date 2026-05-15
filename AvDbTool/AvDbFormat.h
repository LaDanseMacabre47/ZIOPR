#pragma once
#include <cstdint>
#include <string>

// ================================================================
//  Бинарный формат пакета антивирусных баз (Big-Endian)
//  Источник: бэкенд /api/binary/signatures/full
//  Формат: multipart/mixed с двумя частями: manifest.bin + data.bin
// ================================================================

// ---------------------------------------------------------------
//  manifest.bin
// ---------------------------------------------------------------
// Заголовок (все числа Big-Endian):
//   magic:                null-terminated "MF-<Фамилия>"
//   version:              uint8
//   exportType:           uint8   (1=full, 2=increment, 3=by-ids)
//   generatedAtEpochMs:   int64
//   sinceEpochMs:         int64   (-1 для full dump)
//   recordCount:          uint32
//   dataSha256:           32 bytes
//
// entries[recordCount]:
//   uuid:                 16 bytes
//   statusCode:           uint8   (0=DELETED, 1=ACTUAL)
//   updatedAtEpochMs:     int64
//   dataOffset:           uint64
//   dataLength:           uint32
//   recordSigLen:         uint32
//   recordSigBytes:       recordSigLen bytes  (RSA подпись записи из data.bin)
//
// manifestSigLen:         uint32
// manifestSigBytes:       manifestSigLen bytes (RSA подпись всего манифеста)

// ---------------------------------------------------------------
//  data.bin
// ---------------------------------------------------------------
// Заголовок:
//   magic:                null-terminated "DB-<Фамилия>"
//   version:              uint8
//   recordCount:          uint32
//
// records[recordCount]:
//   threatNameLen:        uint32
//   threatName:           threatNameLen bytes (UTF-8)
//   firstBytesLen:        uint32
//   firstBytes:           firstBytesLen bytes (raw, декодированный hex)
//   remainderHash:        32 bytes (SHA-256 raw)
//   remainderLength:      uint32
//   fileType:             uint8   (0=PE, 1=Script, ...)
//   offsetStart:          int64
//   offsetEnd:            int64

// Статус записи манифеста
constexpr uint8_t MANIFEST_STATUS_DELETED = 0;
constexpr uint8_t MANIFEST_STATUS_ACTUAL = 1;