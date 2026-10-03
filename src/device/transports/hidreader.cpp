// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/transports/hidreader.h"

Q_LOGGING_CATEGORY(lcHid, "qtdmm.hid", QtWarningMsg)

HidReader::HidReader(hid_device *handle)
  : m_handle(handle)
{
}

void HidReader::run()
{
  unsigned char buf[64];
  while (!m_stop.load())
  {
    // the timeout keeps stop() effective even when the cable sends nothing
    // (CP2110/CH9329 send no idle reports)
    const int res = hid_read_timeout(m_handle, buf, sizeof(buf), 100);
    if (res < 0)
    {
      Q_EMIT readError(QString::fromWCharArray(hid_error(m_handle)));
      break;
    }
    if (res > 0)
      Q_EMIT report(QByteArray(reinterpret_cast<const char *>(buf), res));
  }
  hid_close(m_handle);
  m_handle = Q_NULLPTR;
  Q_EMIT finished();
}
