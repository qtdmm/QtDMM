// Copyright (c) 2026 The QtDMM developers
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <array>
#include <limits>

class RequestDeadlines;

/// A function generator QtDMM operates: the simulated one (next), later
/// the generator in the OWON SDS215S - a device of its own in the UI that
/// talks through the oscilloscope's connection (the backend decides how it
/// reaches the device; setConnected() tells the front). Without UI; the
/// front panel works on this and nothing else. Built like PowerSupply.
///
/// - **Channels:** channelCount() (the SDS215S has one), each with its
///   name, the shapes it can make and its limits (ChannelInfo).
/// - **Setting is asynchronous:** requestShape(), requestValue() for the
///   frequency, amplitude, offset, duty cycle and symmetry, requestOutput().
///   The backend answers with confirmShape() / confirmValue() when it read
///   the setting back, or failRequest(). Without an answer within
///   confirmTimeout() the request fails ("No answer"). Until then the
///   channel's state says Pending and the front shows what was sent.
/// - **Limits:** the frequency's upper one depends on the shape (a sine
///   goes higher than a square); the amplitude (Vpp) between the device's
///   and the user's limits; the offset so that offset ± amplitude/2 stays
///   within offsetMax(). Requests are clamped and rounded.
/// - **Settings per channel** (ChannelSettings, stored by the backend with
///   its entry): shown, colour, the highest amplitude and offset allowed
///   (to protect what is connected), steps of the knobs.
class FunctionGenerator : public QObject
{
  Q_OBJECT
public:
  static constexpr int kMaxChannels = 2;
  static constexpr double kNone = std::numeric_limits<double>::quiet_NaN();
  static constexpr int kDefaultConfirmTimeout = 2000;   ///< ms

  /// The basic shapes, on keys of their own; Arbitrary is one of the
  /// device's other shapes, named by ChannelState::arbitrary.
  enum class Shape { Sine, Square, Ramp, Pulse, Arbitrary };
  static constexpr int kBasicShapes = 4;
  /// What requestValue() sets.
  enum class Value { Frequency, Amplitude, Offset, Duty, Symmetry };
  static constexpr int kValues = 5;

  /// A channel as the device has it.
  struct ChannelInfo
  {
    QString name;                             ///< "CH1"
    QList<Shape> shapes { Shape::Sine, Shape::Square, Shape::Ramp, Shape::Pulse };
    QStringList arbitrary;                    ///< the device's other shapes, by its names
    std::array<double, kBasicShapes + 1> fMax { 25e6, 5e6, 1e6, 5e6, 5e6 };   ///< Hz, per Shape
    double  fMin = 0.1;                       ///< Hz
    double  ampMin = 0.005, ampMax = 3;       ///< Vpp
    double  offsetMax = 3;                    ///< |offset| + amplitude/2 stays within (V)
    int     fDigits = 7;                      ///< significant digits it sets
    int     ampDecimals = 3;                  ///< V
    int     offsetDecimals = 3;               ///< V
    int     percentDecimals = 1;              ///< duty, symmetry
  };

  enum class SetState
  {
    Idle,       ///< nothing asked yet
    Pending,    ///< sent, not read back yet
    Confirmed,  ///< read back
    Failed      ///< refused or no answer: error says why
  };

  /// What a channel is doing, as read back.
  struct ChannelState
  {
    bool    output = false;
    bool    external = false;     ///< set from outside (a logic node): the knobs are locked
    bool    connected = true;
    Shape   shape = Shape::Sine;
    QString arbitrary;            ///< the shape's name when Arbitrary
    std::array<double, kValues> value { kNone, kNone, kNone, kNone, kNone };       ///< read back
    std::array<double, kValues> requested { kNone, kNone, kNone, kNone, kNone };   ///< sent last
    SetState setState = SetState::Idle;
    QString error;                ///< why the last request failed
    double  get(Value v) const { return value[size_t(v)]; }
  };

  /// The user's choices for a channel.
  struct ChannelSettings
  {
    bool    shown = true;
    QColor  color;                 ///< invalid: defaultColor()
    double  ampMax = kNone;        ///< Vpp; NaN: the device's
    double  offsetMax = kNone;     ///< V; NaN: the device's
    double  ampStepCoarse = 0.1, ampStepFine = 0.01;         ///< Vpp per knob step, with Shift
    double  offsetStepCoarse = 0.1, offsetStepFine = 0.01;   ///< V
    bool    operator==(const ChannelSettings &o) const;
    QJsonObject toJson() const;
    static ChannelSettings fromJson(const QJsonObject &o);
  };

  explicit FunctionGenerator(QObject *parent = nullptr);
  ~FunctionGenerator() override;

  virtual int channelCount() const = 0;
  virtual ChannelInfo channel(int ch) const = 0;
  ChannelState state(int ch) const;

  /// @name The user's choices
  /// @{
  ChannelSettings settings(int ch) const;
  void    setSettings(int ch, const ChannelSettings &settings);
  QJsonObject settingsJson() const;
  void    setSettingsJson(const QJsonObject &o);
  QList<int> shownChannels() const;
  /// The range of @p v for channel @p ch with its shape now: the device's
  /// limits within the settings'; the offset's leaves room for the amplitude.
  double  minimum(int ch, Value v) const;
  double  maximum(int ch, Value v) const;
  /// Highest frequency of @p shape on channel @p ch.
  double  maxFrequency(int ch, Shape shape) const;
  /// Whether @p v belongs to @p shape (duty to Pulse, symmetry to Ramp).
  static bool hasValue(Shape shape, Value v);
  static QColor defaultColor(int ch);
  QColor  color(int ch) const;
  /// "sine", "square", "ramp", "pulse", "arbitrary" (stored) and back.
  static QString shapeName(Shape shape);
  static Shape shapeOf(const QString &name);
  /// @}

  /// @name Asking the device
  /// @{
  /// Sends @p shape (@p arbitrary: its name when Arbitrary); false when not
  /// sent (not shown, external, not connected, a shape the channel lacks).
  bool    requestShape(int ch, Shape shape, const QString &arbitrary = QString());
  /// Sends @p value clamped to minimum()..maximum() and rounded to the
  /// channel's resolution; false when not sent (as requestShape(), or a
  /// value the shape has not).
  bool    requestValue(int ch, Value v, double value);
  bool    requestOutput(int ch, bool on);
  /// Every shown channel's output off.
  void    allOff();
  /// @p value of @p v as channel @p ch would set it (clamped, rounded).
  double  limited(int ch, Value v, double value) const;
  int     confirmTimeout() const;
  void    setConfirmTimeout(int ms);
  virtual bool controlAllowed() const { return true; }
  /// @}

Q_SIGNALS:
  /// Channel @p ch's state changed (a request, an answer, connection).
  void    stateChanged(int ch);
  void    settingsChanged(int ch);
  /// What the device was set to changed (to store it with the entry).
  void    setpointChanged(int ch);

protected:
  /// @name The backend
  /// @{
  /// Send to the device; answer with confirm...() or failRequest() (at once
  /// or later). After a shape the backend confirms the frequency too when
  /// the device had to lower it (a square goes less high than a sine).
  virtual void sendShape(int ch, Shape shape, const QString &arbitrary) = 0;
  virtual void sendValue(int ch, Value v, double value) = 0;
  virtual void sendOutput(int ch, bool on) = 0;
  void    confirmShape(int ch, Shape shape, const QString &arbitrary = QString());
  void    confirmValue(int ch, Value v, double value);
  void    failRequest(int ch, const QString &error);
  void    confirmOutput(int ch, bool on);
  void    setExternal(int ch, bool external);
  void    setConnected(bool connected);
  /// The channels changed (count, names): states and settings follow.
  void    channelsChanged();
  /// @}

private:
  bool    valid(int ch) const { return ch >= 0 && ch < int(m_states.size()); }
  bool    sendable(int ch) const;
  void    pending(int ch);
  void    expired(int ch);

  QList<ChannelState> m_states;
  QList<ChannelSettings> m_settings;
  RequestDeadlines *m_deadlines;
};
