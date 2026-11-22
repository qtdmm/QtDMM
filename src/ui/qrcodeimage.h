// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QImage>
#include <QString>

/// @p text as a QR code (qrcodegen, error correction M), black on white with
/// the quiet zone of four modules, each module @p scale pixels; null when
/// @p text does not fit.
QImage qrCodeImage(const QString &text, int scale = 6);
