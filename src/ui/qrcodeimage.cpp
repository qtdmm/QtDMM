// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/qrcodeimage.h"

#include "3rdparty/qrcodegen/qrcodegen.hpp"

#include <QPainter>

QImage qrCodeImage(const QString &text, int scale)
{
  try
  {
    const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    constexpr int quiet = 4;
    const int size = qr.getSize();
    const int side = (size + 2 * quiet) * scale;
    QImage image(side, side, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    for (int y = 0; y < size; ++y)
      for (int x = 0; x < size; ++x)
        if (qr.getModule(x, y))
          painter.fillRect((x + quiet) * scale, (y + quiet) * scale, scale, scale, Qt::black);
    return image;
  }
  catch (const qrcodegen::data_too_long &)
  {
    return {};
  }
}
