# Coding style

How the QtDMM sources are written. The rules describe the code as it is (measured on 26.2, review R13); where the code is not uniform yet, the rule says which way new code goes. Existing code is not reformatted in bulk: a file follows the rules when it is changed anyway, so `git blame` stays useful.

C++20 and Qt 6. The build with `-DQTDMM_WERROR=ON` has no warnings, and stays so.

## Files

- Lower case, no spaces: `qcpgraph.cpp`, `recordingstore.h`. Decoders are named after the protocol or chip with underscores between vendor and model: `uni_t_ienv.cpp`, `cyrustek_es51922.cpp`.
- One class per header and source pair, the file named after the class in lower case.
- A class too large for one source file spreads its definitions over parts named `<class>_<part>.cpp`, each about one subject (`qcpgraph_axes.cpp`, `qcpgraph_input.cpp`); the header stays one. Helpers that several parts need go into `<class>_p.h`, which is not part of the interface.
- Headers use `#pragma once`.
- A new file starts with:

  ```cpp
  // Copyright (c) 2026 The QtDMM developers
  // SPDX-License-Identifier: GPL-3.0-or-later
  ```

  Files that carry an author's copyright notice (from QtDMM's earlier years) keep it.

## Layout

- Indent with two spaces, no tabs.
- Braces on a line of their own for classes, functions and blocks:

  ```cpp
  if (m_series.isEmpty())
  {
    m_integral = 0;
    return;
  }
  ```

  A single statement may go without braces, on the next line.
- **Line length:** there is no hard limit and no 80-column rule. Around 120 columns is the guide, and each case is weighed: one longer line that reads at a glance is better than a statement spread over three short ones. Wrap where the line would no longer be read as one thought, and then at a sensible point (after a comma, before an operator), with the continuation aligned or indented.
- Short tables and initialiser lists stay on one line when they fit: `{ "T", 1e12 }, { "G", 1e9 }, …`.
- **Exception:** the `addConfig({...})` lines that register the meters in the decoders are never wrapped, however long: one meter per line, so the models of a decoder read as a table and a meter is added or changed in one line.
- **Comments follow the same width.** Do not break a paragraph at 78 columns; let it run to about 100–120. When a comment is changed, its paragraph is re-wrapped; untouched comments stay as they are.

## Names

camelCase throughout, no snake_case.

| What | Style | Examples |
|---|---|---|
| classes, structs, enums, typedefs | PascalCase | `RecordingStore`, `PortKey`, `Quality` |
| enum values | PascalCase (`enum class` preferred) | `Quality::Overload`, `Live` |
| functions and methods | camelCase, a verb for actions | `appendExtra()`, `setStale()`, `isRunning()` |
| getters | the property's name, no `get` | `unit()`, `extraCount()`; booleans `isX()`/`hasX()` |
| parameters and local variables | camelCase | `baseUnit`, `firstPoint` |
| member variables of classes | `m_camelCase` | `m_series`, `m_lastReading` |
| widgets kept as members | `ui_camelCase` | `ui_intValue`, `ui_showInt` |
| fields of plain data structs (all public) | camelCase without prefix | `Reading::baseUnit`, `RawPoint::t` |
| constants (`constexpr`, `static const`, file-level) | `kPascalCase` | `kMaxPoints`, `kStaleMs` |
| mutable file-static variables (avoid) | `s_camelCase` in an anonymous namespace | `s_vxi11PortmapperPort` |

- Abbreviations are written as words: `HidReader`, `DmmDecoder`, `ScpiServer`, `Vxi11Server`. Older names such as `HIDSerialDevice` and `DMMInfo` stay until their code is reworked.
- Units belong in the name when a number carries one: `timeoutMs`, `kLiveMaxMs`, `sampleTimeTenths`.
- Slots are named for what they do (`startRecording()`, `clear()`), without the old `…SLOT` suffix. The existing `…SLOT` methods are kept, but new ones do not get it.
- `on_<object>_<signal>()` (Qt's automatic connection by name) only in the existing Designer forms. New code connects explicitly.

## Qt

- `Q_EMIT`, `Q_SIGNALS`, `Q_SLOTS`, not `emit`/`signals`/`slots`.
- `nullptr`, not `Q_NULLPTR` or `0`.
- Connections with member-function pointers or lambdas: `connect(m_store, &RecordingStore::appended, this, &QcpGraph::onAppended)`. No `SIGNAL()`/`SLOT()` strings and no `QMetaObject::invokeMethod(obj, "name")`: a typo there only shows at run time. `invokeMethod` with a lambda (queued call) is fine.
- `std::as_const` for range-for over Qt containers that are members.
- `QStringLiteral` for string literals that become a `QString`, `QLatin1String`/`QLatin1Char` for comparisons.
- Text the user sees goes through `tr()`; it is English, in the tone of the handbook.
- Ownership by QObject parent. A raw `new` without a parent belongs into a `std::unique_ptr`.
- Logging with a category: `Q_LOGGING_CATEGORY(lcScpi, "qtdmm.scpi", QtWarningMsg)`, then `qCDebug(lcScpi)` (a new category also goes into the `--debug` rules in `mainwindow.cpp` (`setFilterRules`), or `QT_LOGGING_RULES`). No `std::cerr` and no `qDebug()` left in, and no commented-out debug lines: what is worth keeping becomes a category message, the rest is deleted.

## Structure

- **No forwarders through layers.** A function that only calls another one with the same arguments needs a reason to exist: a stable public API, hiding a type, or a name that says more. When a setting is added, it does not get a getter in the page, one in `SettingsDialog`, and one more in `InstanceWidget`; the consumer reads the page's values (or the settings) directly.
- When a function is renamed or replaced, the old one goes in the same change. No wrapper that only keeps the old name (`bucketPoints()` → `pointsOf()`).
- One way to reach an object: either through its owner's interface or directly, not both side by side (`graph->setSampleLength()` next to `graph->store()->setPreTrigger()`).
- More than about four parameters of the same kind: pass a struct (`GraphPrefs::Values`, `DmmResponse`) instead of a positional list.
- Structs larger than a few pointers are passed as `const &` (`const DmmDecoder::DMMInfo &`).
- Code that is not used is deleted, not commented out; git keeps it.
- A helper that two files need goes into a shared place, not a second copy. Examples: the 1-2-5 step of an axis (`core/nicestep.h`), the ring-buffer access of the decoders, the receive buffer of the transports.
- Long functions are split where the parts have a name. Protocol dispatchers and decoder tables may be long; a lookup table is preferred to a long `if`/`switch` chain.

## Comments

- Doc comments are `///` before the declaration in the header. They say what something is for and what the caller must know (units, ownership, when a signal comes), not what the code says anyway.
- Comments in the code explain *why*: the meter's quirk, the protocol rule, the bug the line prevents. A reference helps: `(IEEE 488.2 §6.1)`, `(review R12-01)`, `(dmm-tools, family spec §2.3)`.
- English, plain sentences.
- `TODO`/`FIXME` only with what is missing; open work belongs in an issue or on the board.

## Decoders

- A decoder documents its protocol at the top of the source, with the source of the information, and lists what was tried with a real meter and what is UNVERIFIED.
- One `addConfig({...})` per meter, on one line (see Layout).
- Test frames come from the protocol specification under `docs/protocols/spec/` (fixtures in `tests/data/decoder/`, see the developer documentation).
- `checkFormat()` reads the ring buffer through `DmmDecoder::ringByte()` and `lastBytes()` (back 0 is the newest byte), not with its own `% FIFO_LENGTH`; the 16-bit sum of the UNI-T frames is `appendSum16()`/`sum16Valid()`.

## Tests

- One test program per subject under `tests/`, registered in `cmake/ctest.cmake`.
- `check()`, the failure count and `waitFor()` come from `tests/testutil.h`; a test does not define its own.
- A test reads a result in a statement of its own before using it in a message: `const bool ok = s.start(); check(ok, "start: " + s.error());`. The order in which function arguments are evaluated is unspecified (MSVC evaluates right to left).
- Tests use a temporary settings directory, never the user's configuration, and network ports chosen by the system (port 0), not fixed ones.

## Tools

- `clang-format` with the project's `.clang-format` (to be added: two-space indent, Allman braces, `ColumnLimit: 120`) is a help while writing, not a rule for the existing code. Do not run it over whole files that are otherwise unchanged.
- Duplicates and complexity can be checked with PMD CPD and lizard; dead and forwarding functions with CodeQL. The queries of review R13 are in the review notes.
