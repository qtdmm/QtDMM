// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

/// QCustomPlot, whichever the build draws with: the copy in src/3rdparty
/// (QCustomPlot 2.1.1, Wireshark's fork) or NeoQCP (QTDMM_WITH_NEOQCP, a
/// trial). QtDMM's sources include this header, never the library's own.
#ifdef QTDMM_NEOQCP
#  include <qcustomplot.h>
#else
#  include "3rdparty/qcustomplot/qcustomplot.h"
#endif
