// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>

#include "core/valueref.h"
#include "device/transports/calc.h"
#include "recording/recordingstore.h"

class DeviceLibrary;
class MeterController;
class ServiceHost;

/// The meters of one process, each opened from "My devices" (open():
/// Connect, --connect, SCPI QTDMM:DEVice:OPEN). The window has none of its
/// own. They share the network services (host()); INSTrument chooses among
/// them, starting with the first.
///
/// A meter opened here has the rules of its entry (alarms, PASS/FAIL
/// test; a change goes back into the entry); its readings carry its source
/// ("device:<id>", Reading::source) and reach SCPI.
///
/// The recording is the session's (setRecordingSetup(), startRecording()
/// ...): the meter of the trigger value leads (lead(); without one there
/// the first), its store takes the start mode, the
/// threshold and the clock time. The others start at the same moment, on
/// one time base (RecordingStore::startAt()), with its sample time, and
/// stop with it, whatever started or stopped it (Record, a trigger, the
/// length, SCPI). A recording keeps its lead until it ends. INITiate and
/// ABORt on any meter go to the window's Record button (setRecordingRequests(),
/// its questions first), with no window to the session. A recording not saved
/// (QTDMM:RECord:SAVE) is overwritten by the next.
/// A recording started from a graph takes only the meters whose values it
/// shows (setRecordingScope()); the others stay in Live.
class DeviceSession : public QObject
{
  Q_OBJECT
public:
  explicit DeviceSession(QObject *parent = nullptr);
  /// The meters go first, while the session is whole: what follows their
  /// destroyed() (AlarmService) may still ask controllers().
  ~DeviceSession() override;

  ServiceHost *host() const { return m_host; }
  /// "My devices" in @p dir (Settings::configDir()), for open(id).
  void        setConfigDir(const QString &dir);
  /// This instance's id (--config-id), for the meters' mDNS and SCPI names.
  void        setInstanceId(const QString &id) { m_instanceId = id; }
  /// sigrok-cli for the meters read through it ("Port settings/sigrok_exe").
  void        setSigrokExe(const QString &exe) { m_sigrokExe = exe; }

  /// Opens and connects the entry @p id of "My devices". Null and the
  /// reason in @p error (a SCPI error, "<code>,<text>", in English) when it
  /// is unknown, already open or does not connect.
  MeterController *open(const QString &id, QString *error = nullptr);
  /// The same with the entry's keys and name given.
  MeterController *open(const QString &id, const QVariantMap &keys, const QString &name, QString *error = nullptr);
  /// Closes a meter open() opened; false (and why) for one not open.
  bool        close(const QString &id, QString *error = nullptr);

  /// @name The session's recording
  /// @{
  /// The Recording page's settings for all stores; the lead's takes the start.
  /// Where a meter's request to start or stop the recording goes (SCPI
  /// INITiate, an alarm's program): the window's Record button with its
  /// questions; none set, the session starts and stops at once.
  void        setRecordingRequests(std::function<void(bool start)> handler) { m_requests = std::move(handler); }
  void        setRecordingSetup(const RecordingStore::Setup &setup);
  const RecordingStore::Setup &recordingSetup() const { return m_setup; }
  /// The meter whose store leads: the recording's while one runs, else the
  /// one of the trigger value, else the first opened; null with none.
  MeterController *lead() const;
  void        startRecording();
  void        stopRecording();
  /// Live for all (a recording not saved is replaced).
  void        goLive();
  /// Clears the recording; one that runs goes on.
  void        clearRecording();
  bool        isRecording() const;
  /// The meters whose recording is not saved (stopped and not exported, or
  /// still running), by name; @p id only that one of My devices.
  QStringList unsavedRecordings(const QString &id = QString()) const;
  /// @}

  /// The next recording takes, of the meters here, only those
  /// of @p sources (MeterController::source(): a graph's values), also one
  /// opened while it runs; the recording after takes all again.
  void        setRecordingScope(const QStringList &sources);
  /// The next recording takes all of them (a start that did not come).
  void        clearRecordingScope() { m_nextScope.reset(); }

  /// The meters open() opened, in their order.
  QList<MeterController *> controllers() const;
  /// The meter whose readings come from @p source (MeterController::source()).
  MeterController *controller(const QString &source) const;
  /// The meter of @p value; nullptr for an empty source or while that meter
  /// is not open.
  MeterController *controller(const ValueRef &value) const;
  /// What a calculated value of this process reads (CalcDevice): the main
  /// value of each meter here by its name in My devices, as a variable
  /// ("UT61E Bench": ut61e_bench).
  CalcDevice::Inputs calcInputs() const;
  /// The variable of a meter named @p name in a formula: lower case,
  /// other characters than letters, digits and '_' as '_', a '_' before a
  /// leading digit.
  static QString variableName(const QString &name);

  /// The entries of "My devices", for QTDMM:DEVice:CATalog?.
  DeviceLibrary *library() const { return m_library; }

Q_SIGNALS:
  void        opened(MeterController *ctl);
  /// Before a meter opened here is closed: its store goes with it.
  void        closing(MeterController *ctl);
  /// After the controller is gone.
  void        closed(const QString &source);
  /// A meter's store changed its state (Live, Record, View): isRecording()
  /// and unsavedRecordings() may have changed.
  void        recordingStateChanged();
  /// For the status bar: a meter could not be opened, or was closed for the window.
  void        error(const QString &text);
  /// For the status bar: a message of a meter here ("recording saved as ...").
  void        info(const QString &text);

private:
  void        installScpi();
  /// The rules of the entry @p id on @p ctl, and its changes back into the entry.
  void        applyRules(MeterController *ctl, const QString &id);
  /// Starts, stops and clears the others with the lead's store (watchRecording()).
  void        watchRecording(MeterController *ctl);
  /// The lead's recording started (Record), ended (View), or it went Live.
  void        followLead(int state);
  /// setRecordingSetup() on every store, the lead's with the start.
  void        applySetup();
  /// Starts the recording of @p ctl at @p t0 on the core clock, wall clock
  /// @p start, the grid @p sampleTime (tenths, 0: its own).
  void        startFollower(MeterController *ctl, qint64 t0, const QDateTime &start, int sampleTime);
  /// The window went Live: @p ctl too (its recording not saved is replaced).
  void        liveFollower(MeterController *ctl);
  /// The message that the recording of @p ctl not saved is replaced.
  void        warnUnsaved(MeterController *ctl);
  /// INITiate or ABORt on a meter here.
  void        onRecordingRequested(bool start);

  ServiceHost *m_host;
  DeviceLibrary *m_library = nullptr;
  std::function<void(bool start)> m_requests;   ///< setRecordingRequests()
  QList<MeterController *> m_opened;
  /// setRecordingScope() for the next recording, and that of the one that
  /// runs; none: all.
  RecordingStore::Setup m_setup;
  QPointer<MeterController> m_running;   ///< the lead of the recording that runs
  std::optional<QStringList> m_nextScope;
  std::optional<QStringList> m_scope;
  /// Whether @p ctl records with the recording that runs.
  bool        inScope(const MeterController *ctl) const;
  QString     m_instanceId = "default";
  QString     m_sigrokExe = "sigrok-cli";
};
