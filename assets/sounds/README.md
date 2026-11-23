# Alarm sounds

The sounds an alarm can play (Settings > Alarms > Sound), built into the
program as `:/sounds/<name>.wav`. All from [freesound.org](https://freesound.org),
released there under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)
(public domain, no attribution required); cut, made mono and converted for QtDMM.

| File             | Shown as | Freesound source (CC0 1.0) |
|------------------|----------|----------------------------|
| `alarm_warn.wav` | Warning  | [#262861](https://freesound.org/s/262861/) "Blip 5" by deleted_user_2906614 |
| `alarm_high.wav` | High     | [#704134](https://freesound.org/s/704134/) "Beep Sound" by Entershift |
| `alarm_low.wav`  | Low      | [#562773](https://freesound.org/s/562773/) "Sync Blip.wav" by BIONN |
| `alarm_crit.wav` | Critical | [#262861](https://freesound.org/s/262861/) "Blip 5" by deleted_user_2906614 |
| `alarm_ok.wav`   | OK (also the sound when an alarm clears) | [#268162](https://freesound.org/s/268162/) "player green.wav" by thirteenthfail |

A new sound must be CC0 too: many sounds on freesound.org are CC BY or CC
BY-NC, which would need credits in the program or forbid its use.

WAV, 16 bit PCM, 44.1 kHz: what `QSoundEffect` plays on every platform.
