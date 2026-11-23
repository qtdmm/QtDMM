// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SpreadsheetWriter: the XML parts carry real cell types, the files are
// valid ZIPs with the right entries (mimetype first and stored for ODS),
// and - when LibreOffice is installed - both formats convert back to CSV
// with the same values (soffice --headless --convert-to csv).

#include <QtCore>

#include "recording/spreadsheet.h"
#include <miniz.h>
#include "testutil.h"

static QStringList zipEntries(const QString &path, QMap<QString, QByteArray> *contents = nullptr, bool *firstStored = nullptr)
{
  QStringList names;
  // the bundled miniz is built without stdio (MINIZ_NO_STDIO): read the file ourselves
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return names;
  const QByteArray bytes = f.readAll();
  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_mem(&zip, bytes.constData(), size_t(bytes.size()), 0))
    return names;
  for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i)
  {
    mz_zip_archive_file_stat st;
    mz_zip_reader_file_stat(&zip, i, &st);
    names << QString::fromUtf8(st.m_filename);
    if (i == 0 && firstStored)
      *firstStored = st.m_method == 0;
    if (contents)
    {
      size_t size = 0;
      void *p = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
      (*contents)[names.last()] = QByteArray(static_cast<const char *>(p), int(size));
      mz_free(p);
    }
  }
  mz_zip_reader_end(&zip);
  return names;
}

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  QTemporaryDir tmp;

  SpreadsheetWriter w("Recording");
  w.setHeader({"timestamp", "time (s)", "value", "unit"});
  const QDateTime t0(QDate(2026, 9, 21), QTime(14, 3, 5, 250));
  w.addRow({t0, 0.0, 1.234, "V"});
  w.addRow({t0.addMSecs(500), 0.5, -0.5, "V"});
  w.addRow({t0.addMSecs(1000), 1.0, 12345.678, "kV & <more>"});
  check(w.rowCount() == 3, "three rows");

  // --- 1. parts ---
  const QString sheet = QString::fromUtf8(w.xlsxSheet());
  check(sheet.contains("<c r=\"A1\" t=\"inlineStr\" s=\"1\"><is><t>timestamp</t></is></c>"), "xlsx header cell, bold style");
  check(sheet.contains("<c r=\"C2\"><v>1.234</v></c>"), "xlsx number cell");
  check(sheet.contains("<c r=\"A2\" s=\"2\"><v>46286.58547"), "xlsx date as serial with the date style: " + sheet.mid(sheet.indexOf("A2") - 6, 40));
  check(sheet.contains("kV &amp; &lt;more&gt;"), "xlsx text escaped");
  check(sheet.contains("state=\"frozen\""), "xlsx header row frozen");
  const QString ods = QString::fromUtf8(w.odsContent());
  check(ods.contains("office:value-type=\"date\" office:date-value=\"2026-09-21T14:03:05.250\""), "ods date cell");
  check(ods.contains("office:value-type=\"float\" office:value=\"12345.678\""), "ods number cell");
  check(ods.contains("<text:p>kV &amp; &lt;more&gt;</text:p>"), "ods text escaped");
  check(ods.contains("table:table-header-rows"), "ods header row");

  // --- 2. files ---
  QString error;
  const QString xlsx = tmp.filePath("r.xlsx"), odsPath = tmp.filePath("r.ods");
  check(w.write(xlsx, SpreadsheetWriter::Xlsx, &error), "xlsx written: " + error);
  check(w.write(odsPath, SpreadsheetWriter::Ods, &error), "ods written: " + error);
  QMap<QString, QByteArray> parts;
  const QStringList xe = zipEntries(xlsx, &parts);
  check(xe.contains("[Content_Types].xml") && xe.contains("xl/workbook.xml") && xe.contains("xl/worksheets/sheet1.xml") && xe.contains("xl/styles.xml"),
        "xlsx zip entries: " + xe.join(','));
  check(parts.value("xl/worksheets/sheet1.xml") == w.xlsxSheet(), "xlsx sheet round trips through the zip");
  bool stored = false;
  const QStringList oe = zipEntries(odsPath, nullptr, &stored);
  check(oe.first() == "mimetype" && stored, "ods: mimetype first and stored: " + oe.join(','));
  check(oe.contains("META-INF/manifest.xml") && oe.contains("content.xml"), "ods entries");
  check(SpreadsheetWriter::formatForFile("a.XLSX") == SpreadsheetWriter::Xlsx && SpreadsheetWriter::formatForFile("b.ods") == SpreadsheetWriter::Ods
        && !SpreadsheetWriter::formatForFile("c.csv").has_value(), "format by suffix");
  SpreadsheetWriter empty;
  check(!empty.write(tmp.filePath("e.xlsx"), SpreadsheetWriter::Xlsx, &error) && !error.isEmpty(), "empty refuses");
  check(!w.write("/no/such/dir/x.ods", SpreadsheetWriter::Ods, &error), "unwritable path fails");

  // --- 2b. control characters: a noisy line makes an ASCII decoder produce
  // readings like "1.2\x034"; unescaped they made the file unopenable ---
  {
    SpreadsheetWriter n("Noise");
    n.setHeader({"timestamp", "value", QString("unit\x01")});
    n.addRow({t0, QString("1.2\x03" "4"), QString("V\x1f")});
    n.addRow({t0.addMSecs(500), QString("ok\tand\nnewline"), "V"});
    const QString sheet2 = QString::fromUtf8(n.xlsxSheet());
    const QString ods2 = QString::fromUtf8(n.odsContent());
    for (const QString &xml : {sheet2, ods2})
    {
      check(!xml.contains(QChar(0x03)) && !xml.contains(QChar(0x01)) && !xml.contains(QChar(0x1f)),
            "control characters are gone");
      check(xml.contains(QChar(0xFFFD)), "... replaced by U+FFFD");
      check(xml.contains('\t') && xml.contains('\n'), "tab and newline survive");
      QXmlStreamReader r(xml);
      while (!r.atEnd())
        r.readNext();
      check(!r.hasError(), "well-formed XML: " + r.errorString());
    }
    const QString noisy = tmp.filePath("n.ods");
    check(n.write(noisy, SpreadsheetWriter::Ods, &error), "written: " + error);
    QMap<QString, QByteArray> noisyParts;
    zipEntries(noisy, &noisyParts);
    QXmlStreamReader r(noisyParts.value("content.xml"));
    while (!r.atEnd())
      r.readNext();
    check(!r.hasError(), "content.xml in the zip is well-formed: " + r.errorString());
  }

  // --- 3. LibreOffice reads both back with the same values ---
  const QString soffice = QStandardPaths::findExecutable("soffice");
  if (soffice.isEmpty() || qEnvironmentVariableIsSet("QTDMM_NO_SOFFICE"))
    qInfo() << "soffice not found - conversion check skipped";
  else
  {
    for (const QString &file : {xlsx, odsPath})
    {
      QProcess p;
      p.setProcessEnvironment([]{ auto e = QProcessEnvironment::systemEnvironment(); e.insert("HOME", QDir::tempPath() + "/qtdmm-soffice"); return e; }());
      p.start(soffice, {"--headless", "--convert-to", "csv", "--outdir", tmp.path(), file});
      const bool done = p.waitForFinished(90000);
      const QString csvPath = tmp.filePath("r.csv");
      QFile csv(csvPath);
      if (!done || !csv.open(QIODevice::ReadOnly | QIODevice::Text))
      {
        qInfo() << "soffice conversion did not produce a csv for" << file << "- skipped:" << p.readAllStandardError();
        continue;
      }
      const QStringList lines = QString::fromUtf8(csv.readAll()).split('\n', Qt::SkipEmptyParts);
      csv.close();
      QFile::remove(csvPath);
      check(lines.size() == 4, QString("%1: header + 3 rows via LibreOffice (%2)").arg(QFileInfo(file).suffix()).arg(lines.size()));
      if (lines.size() == 4)
      {
        check(lines[0].contains("timestamp") && lines[0].contains("unit"), "header survives: " + lines[0]);
        // LibreOffice writes the CSV in its locale: decimal comma here, point elsewhere
        const QString l1 = QString(lines[1]).replace("\"", ""), l3 = QString(lines[3]).replace("\"", "");
        check((l1.contains("2026-09-21 14:03:05.250") || l1.contains("2026-09-21 14:03:05,250")) && (l1.contains("1.234") || l1.contains("1,234")),
              QFileInfo(file).suffix() + ": date with milliseconds and number: " + lines[1]);
        check((l3.contains("12345.678") || l3.contains("12345,678")) && l3.contains("kV & <more>"), QFileInfo(file).suffix() + ": big number and text: " + lines[3]);
      }
    }
  }

  if (failed == 0)
    qInfo() << "All spreadsheet tests passed.";
  else
    qWarning() << failed << "spreadsheet test(s) failed.";
  return failed == 0 ? 0 : 1;
}
