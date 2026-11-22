// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>

#include "device/dmmdecoder.h"

/// The frames of UNI-T's newer Bluetooth LE meters of the "smartmeasure"
/// multimeter app (UT117C, UT219PV) on the ISSC service:
///
///     AB CD <length BE16> <command> <data> <16-bit BE sum of all before>
///
/// with the length counting command, data and sum (a frame is length + 4
/// bytes). Not the UT60BT/UT61+ frames, whose length is one byte.
namespace UniTAbcd16
{
/// A frame from @p p to @p end: AB CD, a length, length + 4 bytes. With
/// @p summed the 16-bit sum decides, and length + 6 counts too: UNI-T's
/// app reads a UT117C reading as length + 4 bytes, libreble (ported from
/// webspiderteam's Windows tool) as length + 6 (length 0x12 for 24 bytes).
inline bool frameAt(const unsigned char *b, int p, int end, bool summed)
{
  if (end - p < 7 || b[p] != 0xAB || b[p + 1] != 0xCD)
    return false;
  const int length = (b[p + 2] << 8) | b[p + 3];
  if (length < 3)
    return false;
  if (!summed)
    return p + 4 + length == end;
  return (p + 4 + length == end || p + 6 + length == end) && DmmDecoder::sum16Valid(b + p, end - p);
}

/// For DmmDecoder::checkFormat(): a frame ends at @p idx when one starts
/// somewhere in the bytes since the last frame and its length fits.
inline bool endsAt(const char *data, size_t idx, bool summed)
{
  const auto *b = reinterpret_cast<const unsigned char *>(data);
  const int end = int(idx) + 1;
  for (int p = 0; p + 7 <= end; ++p)
    if (frameAt(b, p, end, summed))
      return true;
  return false;
}

/// The last frame in @p data (the bytes since the previous one) and its
/// length; nullptr when there is none.
inline const unsigned char *find(const QByteArray &data, bool summed, int *length)
{
  const auto *b = reinterpret_cast<const unsigned char *>(data.constData());
  const int end = int(data.size());
  for (int p = 0; p + 7 <= end; ++p)
    if (frameAt(b, p, end, summed))
    {
      *length = end - p;
      return b + p;
    }
  return nullptr;
}

/// A frame of @p command with @p data and its sum.
inline QByteArray command(quint8 command, const QByteArray &data)
{
  const int length = int(data.size()) + 3;
  QByteArray f = QByteArray::fromHex("abcd");
  f.append(char(length >> 8)).append(char(length & 0xFF)).append(char(command)).append(data);
  DmmDecoder::appendSum16(f);
  return f;
}
}
