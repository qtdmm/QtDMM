// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "device/dmmdecoder.h"

/// The multimeter of a ZOYI ZT-703S running OpenOSC, the free firmware
/// (github.com/bve/OpenOSC, GPL-3.0), over its USB CDC serial port; the
/// baud rate does not matter. Polled with SCPI: QtDMM sends
/// `SYST:DMM:RES?\n`, the meter answers one line of 11 fields ending in LF,
/// `fn,value,status,value2,quantity2,range,mode,hold,rel,seq,time_us`:
///
/// - fn 0 V DC, 1 V AC, 2 Ohm, 3 continuity, 4 diode, 5 capacitance, 6 A DC,
///   7 A AC, 8 mV DC, 9 mV AC, 10 mA DC, 11 mA AC, 12 temperature (type K)
/// - value in SI base units ("2.176e+3"), NAN unless status is 0
/// - status 0 OK, 6 overload, 14 no number on the display, others errors
/// - value2/quantity2 a secondary value (only after FUNC2 FREQ; 5 = Hz)
/// - range 1-based into the firmware's range table (0 unknown), mode 1 auto,
///   0 manual, -1 unknown; hold and rel 1/0
///
/// The digits shown come from that range table (the firmware's ranges.cpp
/// for the ZT703S, 25000 counts). Spec and vectors in
/// docs/protocols/spec/openosc.yaml, from issue #603.
///
/// Frames are variable-length lines (getPacketLength() 0).
class DecoderOpenOSC : public DmmDecoder
{
  Q_OBJECT
public:
  DecoderOpenOSC(FrameFormat::DataFormat df) : DmmDecoder(df) { m_name = "OpenOSC"; }

  std::optional<DmmDecoder::DmmResponse> decode(const QByteArray &data, int id) override;
  QByteArray pollRequest() const override { return QByteArrayLiteral("SYST:DMM:RES?\n"); }
  bool checkFormat(const char *data, size_t idx) override;
  size_t getPacketLength() override { return 0; }
};
