//======================================================================
// File:		readevent.h
// Author:	Matthias Toussaint
// Created:	Sat Apr 14 13:01:28 CEST 2001
//----------------------------------------------------------------------
// This file is part of QtDMM.
//
// QtDMM is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
// as published by the Free Software Foundation.
//
// QtDMM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------
// Copyright (c) 2001 Matthias Toussaint
//======================================================================

#pragma once

#include <QtCore>
/// Size of the byte ring buffer FrameReader collects frames in. Lives here
/// (not in readerthread.h) because DmmDecoder needs it too.
#define FIFO_LENGTH 100

/// Names the wire protocols QtDMM can decode.
///
/// The enum is the key that ties a DmmDecoder::DMMInfo entry, the decoder
/// class registered for it (DmmDecoder::getInstance), the protocol combo and
/// the settings file together; everything else about a protocol is one row
/// in protocols.cpp. Settings persist the name (toString/fromString); older
/// files hold the number, which is why the values are still stable.
class FrameFormat
{
public:
  /// One entry per decoder; see docs/protocols for the frame formats.
  enum DataFormat
  {
    Invalid = -1,
    Metex14 = 0,
    PeakTech10,
    Voltcraft14Continuous,
    Voltcraft15Continuous,
    M9803RContinuous,
    VC820Continuous,
    CyrustekES51986,
    VC940Continuous,
    QM1537Continuous,
    RS22812Continuous,
    VC870Continuous,
    DO3122Continuous,
    CyrustekES51922,
    DTM0660,
    CyrustekES51962,
    Sigrok,
    GDM703Continuous,
    BrymenBM25x,
    BrymenBM86x,
    BrymenBM52x,
    BrymenBM82x,
    FlukeQM,
    Fluke45,
    VictronBLE,
    UniTiDMM,
    UniTUT61Plus,
    TfaAirControl,
    EndOfList              // new stuff always before!
  };

  /// Name of a format as used in the settings file and in DMMInfo::protocol
  /// (from the table in protocols.cpp); "Invalid" for unknown values.
  static QString toString(DataFormat format);

  /// Inverse of toString(); unknown names give Invalid.
  static DataFormat fromString(const QString &str);
};
