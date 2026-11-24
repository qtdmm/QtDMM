// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <limits>

class RequestDeadlines;

/// A power supply QtDMM operates: the simulated one (SimPsuNode), later
/// real ones (OWON SPM6103, SCPI-client supplies). Without UI; the front
/// panel (PsuFrontPanel) works on this and nothing else.
///
/// - **Channels:** channelCount(), each with a name, its limits and what it
///   can (ChannelInfo). The measured values stay ordinary values of the
///   source (graph, table, limit test, logic nodes as usual); the backend
///   also hands them to setMeasured() for the front panel.
/// - **Setting is asynchronous:** requestVoltage() sends, the backend
///   answers with confirmVoltage() when it read the value back, or
///   failVoltage(). Without an answer within confirmTimeout() the request
///   fails ("No answer"). Until then the channel's state says Pending and
///   the front shows the value sent; a failed one shows its error. The
///   simulation confirms at once.
/// - **Output:** on or off per channel (requestOutput(), confirmOutput()),
///   allOff() for every channel shown.
/// - **Settings per channel** (ChannelSettings, stored by the backend with
///   its entry): shown or not, colour, voltage limits within the device's,
///   steps of the knob. A channel not shown is not on the front, not
///   recorded, and QtDMM never touches its output (allOff() and the
///   requests leave it alone).
/// - **Prepared, not used yet:** a setpoint from a logic node (EXT, the
///   knob locked), the confirmation before an output goes on and the
///   project's control release for real devices, the output off when
///   QtDMM disconnects.
class PowerSupply : public QObject
{
  Q_OBJECT
public:
  static constexpr int kMaxChannels = 4;
  static constexpr double kNone = std::numeric_limits<double>::quiet_NaN();
  static constexpr int kDefaultConfirmTimeout = 2000;   ///< ms

  /// What a channel can do.
  enum Capability
  {
    SetVoltage = 0x01,
    MeasureCurrent = 0x02,   ///< I and so P on the front
    SetCurrent = 0x04,       ///< later: the current limit
    Regulation = 0x08,       ///< later: reports CV/CC
    Protection = 0x10,       ///< later: OVP/OCP/OTP
  };
  Q_DECLARE_FLAGS(Capabilities, Capability)

  /// A channel as the device has it.
  struct ChannelInfo
  {
    QString name;                 ///< "CH1"
    double  uMin = 0, uMax = 30;  ///< what the device can set (V)
    double  iMax = 5;             ///< for the display's digits (A)
    int     uDecimals = 2;        ///< the resolution it sets and shows
    int     iDecimals = 3;
    Capabilities caps = SetVoltage;
  };

  enum class SetState
  {
    Idle,       ///< nothing asked yet
    Pending,    ///< sent, not read back yet
    Confirmed,  ///< read back
    Failed      ///< refused or no answer: error says why
  };
  enum class Mode { None, CV, CC };

  /// What a channel is doing.
  struct ChannelState
  {
    bool    output = false;      ///< confirmed by the device
    Mode    mode = Mode::None;
    bool    external = false;    ///< the setpoint comes from a logic node: the knob is locked
    bool    connected = true;
    double  setpoint = kNone;    ///< the voltage set, as read back
    double  requested = kNone;   ///< the voltage sent last
    SetState setState = SetState::Idle;
    QString error;               ///< why the last request failed
    double  u = kNone, i = kNone, p = kNone;   ///< measured
  };

  /// The user's choices for a channel.
  struct ChannelSettings
  {
    bool    shown = true;
    QColor  color;                 ///< invalid: defaultColor()
    double  uMin = kNone;          ///< NaN: the device's
    double  uMax = kNone;
    double  stepCoarse = 0.1;      ///< V per wheel step or drag step
    double  stepFine = 0.01;       ///< with Shift
    bool    operator==(const ChannelSettings &o) const;
    QJsonObject toJson() const;
    static ChannelSettings fromJson(const QJsonObject &o);
  };

  explicit PowerSupply(QObject *parent = nullptr);
  ~PowerSupply() override;

  virtual int channelCount() const = 0;
  virtual ChannelInfo channel(int ch) const = 0;
  ChannelState state(int ch) const;

  /// @name The user's choices
  /// @{
  ChannelSettings settings(int ch) const;
  void    setSettings(int ch, const ChannelSettings &settings);
  /// All channels' as one array, as a backend stores them.
  QJsonObject settingsJson() const;
  void    setSettingsJson(const QJsonObject &o);
  /// The channels shown, in order.
  QList<int> shownChannels() const;
  /// The range the knob may set: the settings' limits within the device's.
  double  minVoltage(int ch) const;
  double  maxVoltage(int ch) const;
  /// The colour of channel @p ch (0-based) when none is chosen: amber,
  /// blue, magenta, green - one for each of four, readable on every LCD.
  static QColor defaultColor(int ch);
  /// The colour shown: the chosen one or defaultColor().
  QColor  color(int ch) const;
  /// @}

  /// @name Asking the device
  /// @{
  /// Sends voltage @p u (clamped to minVoltage()..maxVoltage(), rounded
  /// to the channel's resolution); false when not sent (a channel not
  /// shown or external, no SetVoltage, not connected).
  bool    requestVoltage(int ch, double u);
  /// Switches the output; false when not sent (as requestVoltage()).
  bool    requestOutput(int ch, bool on);
  /// Every shown channel's output off, also one set from outside (EXT) or
  /// while control is not allowed: off is always safe (the emergency off).
  void    allOff();
  /// Within this the device answers a request, else it failed (ms).
  int     confirmTimeout() const;
  void    setConfirmTimeout(int ms);
  /// For real devices later: an output goes on only after the user
  /// confirmed it (the front asks), and only while the project lets QtDMM
  /// control devices. The simulation needs neither.
  virtual bool confirmOutputOn() const { return false; }
  virtual bool controlAllowed() const { return true; }
  /// @}

Q_SIGNALS:
  /// Channel @p ch's state changed (a request, an answer, measured values).
  void    stateChanged(int ch);
  /// The settings of a channel changed (shown, colour, limits, steps).
  void    settingsChanged(int ch);
  /// The setpoint read back changed (to store it with the entry).
  void    setpointChanged(int ch);

protected:
  /// @name The backend
  /// @{
  /// Send the voltage / the output to the device; answer with confirm...()
  /// or fail...() (at once or later).
  virtual void sendVoltage(int ch, double u) = 0;
  virtual void sendOutput(int ch, bool on) = 0;
  void    confirmVoltage(int ch, double u);
  void    failVoltage(int ch, const QString &error);
  void    confirmOutput(int ch, bool on);
  /// The measured values of channel @p ch (kNone: none).
  void    setMeasured(int ch, double u, double i = kNone, double p = kNone);
  void    setMode(int ch, Mode mode);
  void    setExternal(int ch, bool external);
  void    setConnected(bool connected);
  /// The channels changed (count, names): the states and settings follow.
  /// A backend calls it once its channels are known (its constructor).
  void    channelsChanged();
  /// @}

private:
  bool    valid(int ch) const { return ch >= 0 && ch < int(m_states.size()); }
  bool    sendable(int ch) const;
  void    expired(int ch);

  QList<ChannelState> m_states;
  QList<ChannelSettings> m_settings;
  RequestDeadlines *m_deadlines;   ///< per channel: when a pending request fails
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PowerSupply::Capabilities)
