# QtDMM's changes to QCustomPlot

QCustomPlot 2.1.1 in the version of Wireshark's fork. Upstream is no longer
developed; QtDMM maintains this copy. Changes keep the API; each is marked
`QtDMM:` in `qcustomplot.cpp` and checked by `tests/test_qcplocal.cpp`.

| Where | Change |
|---|---|
| `QCPAxisTickerDateTime::getSubTickCount`, `QCPAxisTickerTime::getSubTickCount` | no `qRound()` of a step beyond int's range (Qt 6.10 asserts on it); from NeoQCP `bc948f4a` |
| `QCPGraph::getOptimizedScatterData` | the pixel span in double, as `getOptimizedLineData` already has it (no int overflow) |
| `QCPColorMapData::QCPColorMapData` | no second `fill(0)`, `setSize()` fills; from NeoQCP `4c5133a2` |
| `QCPColorMapData::setSize` | rejects negative sizes and more cells than an int counts; empty after a failed allocation |
| `QCPColorMapData::fill` | `std::fill_n` instead of `memset` (which set no double but 0); nothing to do without cells; from NeoQCP `11fd5617` |
| `QCPColorMapData::fillAlpha`, `createAlpha` | the cell count in `size_t` |
| `QCPColorMapData::cellToCoord` | a single column or row lies in the middle of its range, not at 0/0; from NeoQCP `2ad8bd57` |
